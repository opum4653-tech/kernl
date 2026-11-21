# NINMODS KERNEL — Current Architecture

This file describes the tree as it is. Offset numbers come from `Offsetsmax.h` (dated 16 September 2026). Older tables in this file that named OB55 `1.132.1` and `GameFacade = 0xA72E4E0` as the live chain were wrong.

---

## 1. What the binary is

- Module: `ninmods-kernel`
- Menu name: NinMods
- Domain: Android ARM64 native executable, root, plus a loaded kernel driver
- Game package scanned from `/proc`: `com.dts.freefiremax` only (MAX-only build; Free Fire Global / `com.dts.freefireth` is not supported)
- Memory path: the selected kernel driver. `mem_read` / `mem_write` do not fall back to `process_vm_readv` or `/proc/<pid>/mem`

```text
target process (libil2cpp.so)
        ^
        | ioctl / prctl of the selected driver
driver client (includes/driver.h)
        ^
overlay (src/main.cpp, ImGui, native surface)
```

---

## 2. Build

Shipping builds run in GitHub Actions (`.github/workflows/build.yml`):

- NDK r25c
- `ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk`
- Artifact: `libs/arm64-v8a/ninmods-kernel`, then `llvm-strip`

`CMakeLists.txt` is a second description of the same target. The workflow does not call CMake. A green local CMake configure does not mean the Actions job passed.

There is no `crash_handler.cpp`. Signals are not installed by this tree.

---

## 3. Process lifecycle (`src/main.cpp`)

1. `PR_SET_DUMPABLE` 0, `PR_SET_NAME` `kworker/u16:1`, then license check (`src/protect/license.cpp`).
2. `start_overlay()` reads display size, creates the native surface, loads fonts, starts touch.
3. Once a second the render loop looks for the game:
   - `get_pid()` walks `/proc` with direct syscalls.
   - `proc::publish_attachment(pid, lib, is_max)` stores pid, `libil2cpp.so` base, and the Max/TH flag as one seqlock snapshot (`src/other/memory.hpp`).
   - `driver->initialize(pid)` runs when the pid changes, not on every read.
   - `get_lib("libil2cpp.so")` resolves the base. Then `data::thread_start()` and `trajectory::thread_start()` run. Both are idempotent if the worker is already up.
4. If `/proc/<pid>` disappears, the loop publishes pid 0 and calls `data::note_detached()`. It does not `join` the workers. A join on this thread froze the frame while a worker sat inside a driver read.
5. `data::thread_stop()` and `trajectory::thread_stop()` run only when the overlay loop exits.

`g_ShowMenu` starts false. The menu opens from the floating logo tap and closes from the menu X (`src/ui/menu.cpp`, `src/ui/floating_logo.cpp`).

---

## 4. Driver client (`includes/driver.h`)

Startup probes, in order, when selection is 0: RT DEV, QX, DEV COMPAT, KPM, RT HOOK. The first success sets `m_selection` to 1..5.

After that, `read` and `write` call only that engine. They do not walk the other loaded drivers on failure. The walk remains only while `m_selection` is still 0.

Options: 1 RT Hook, 2 RT DEV, 3 DEV COMPAT, 4 QX, 5 KPM (TB `prctl` `0x63687501` / Neo `prctl` `0x4E41`).

---

## 5. Memory access (`src/other/memory.hpp`)

`mem_read` rejects non-userspace pointers, a dead pid, and a missing driver. On failure it zeroes the caller buffer.

`mem_write` returns before any ioctl when any of these fail:

- `proc::wpm_allowed` is false (set true only while master + an aim toggle is on, `src/func/aim/aim.hpp`)
- size is not 1, 2, 4, 8, or 12
- size >= 4 and the address is not 4-byte aligned
- the address falls in `[libBase, libBase + 0x1500000)`

`proc::snapshot()` retries until the seqlock epoch is even and unchanged across the pid, lib, and variant loads.

---

## 6. Offsets

Live constants are `Offsetsmax.h`, re-exported by `src/offsets/offsets.hpp`.

| Field | Value | Where |
| :--- | :--- | :--- |
| `GameFacade` | `0xAD68E18` | `Offsetsmax.h`. `offsets.hpp` also keeps `GameFacade_Alt` `0xAD6DC18` as a MAX probe inside `get_lib`. The former `GameFacade_Global` `0xA72E4E0` probe was removed (MAX-only build) |
| `CurrentMatch` | `0x90` | from the resolved base game object |
| `localPlayer` | `0xD8` | |
| `Dictionary` | `0xC0` | entity list |
| `Head` | `0x6A8` | |
| `Hip` / `Spine` / `Root` | `0x6B0` / `0x6B8` / `0x6D0` | |
| `FollowCamera` | `0x698` | |
| `isFiring` | `0x850` | `IsFiring_Alt` `0x848` is a second probe in `target.hpp` |
| `aimInfo` | `0xE58` | `silent.hpp` also probes `0xE50` |
| `HedColider` | `0x740` | read-only from this tree |
| `raycast` (`Aim::Direction`) | `0x40` | the only trajectory store |
| `startPos` | `0x4C` | muzzle read |

`data::update()` (`src/game/data.cpp`) walks GameFacade, match, camera view matrix, and the player dictionary. The view matrix is stored in `data::viewMatrix_pub` under `data::viewMatrix_mutex`.

World-to-screen is `src/game/math.hpp`. Screen size comes from `g_sw` / `g_sh`, set from the display in `start_overlay()`.

---

## 7. Trajectory (`src/func/aim/`)

`trajectory::loop` sleeps `cfg::aim::read_interval_ms`, clamped to 8..33 ms, otherwise 16. It does not drop to 4 ms in a match.

`trajectory::update` calls `apply_trajectory` only on the rising edge of `isFiring` (false to true). Holding fire does not write every tick. `triggerActive` is not a write gate. That flag is forced on whenever silent or auto-headshot is enabled, so using it as the write condition wrote on every poll.

`apply_trajectory` (`silent.hpp`):

1. Resolves the aim-info pointer.
2. Reads start position at `0x4C`. If that vector is unusable, falls back to the local head bone.
3. Picks head or body from `lockToHead`, `auto_headshot`, or `headshot_rate`.
4. Builds a unit direction.
5. Copies `viewMatrix_pub` and takes camera forward as `-(m31, m32, m33)`. If that vector is degenerate, it returns without a write.
6. If `dot(cameraForward, direction) < 0.97815` (`cos(12°)`), it returns without a write.
7. For a body aim, applies Gaussian jitter after the cone check. Sigma is 0.012 at 10 m and 0.045 at 100 m. Vertical scale is half. Head aims are not jittered.
8. Writes one `Vector3` to `AimInfo + 0x40`.

Not written: `HitNormal`, `HitPoint`, `Distance`, `HitGroup`, `ViewBlocked`, `HitCollider`, target `Colider_Field`.

`start_silent_writer()` is empty. There is no second writer thread.

---

## 8. Overlay state

Display size is cached by `src/other/display_manager.cpp` (framebuffer sysfs, then properties). The render loop does not re-query it per frame.

Logo position is a static `ImVec2` in `src/ui/floating_logo.cpp`. `save_pos()` clamps that point. `load_pos()` does not read a file. `/dev/.nin_logo_pos.txt` is not used.

Fonts: `LoadCustomFonts` in `main.cpp` sets `FontDataOwnedByAtlas = false` because the TTF bytes live in `.rodata` (`src/FONTS/MAIN_FONT.h`).

---

## 9. Download layout

Mod and driver zips are served from `https://ninmods.smmtor.com`, in subfolders:

- `downloads/external/gemini-cli.zip`
- `downloads/kernel/ninmods-kernel.zip`
- `downloads/Kernel_Drivers.zip`
- `downloads/app-release.apk`

Flat paths such as `downloads/ninmods-kernel.zip` are not the layout. Packages are not stored in the APK `assets/` directory.

---

## 10. Source rules

- Comments and logs in source files are English.
- No box-drawing comment banners in new code.
- Do not commit `obj/`, `libs/`, `.bak`, or built binaries.
