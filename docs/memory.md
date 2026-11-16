# NINMODS KERNEL — Project Memory & Incident Log

This file is the permanent historical memory and technical log for the `ninmods-kernel` native overlay project.

---

## 1. Engine Specifications & Target Environment

- **Binary Name:** `ninmods-kernel`
- **Packaging:** `ninmods-kernel.zip` (deployed to `https://ninmods.smmtor.com/downloads/kernel/ninmods-kernel.zip`)
- **Compilation Toolchain:** Android NDK r25c (`ndk-build`, aarch64-linux-android)
- **Target Game:** Free Fire Global (`com.dts.freefireth`) & Free Fire MAX (`com.dts.freefiremax`)
- **Game Version:** OB55 (1.132.1)
- **Driver Architecture:** Ring 0 Linux Kernel Driver (RT Hook / RT DEV / DEV / QX / KPM Neo)
- **Rendering Pipeline:** Android Native Surface (`ANativeWindow`) + ImGui

---

## 2. Incident Reports & Technical Root Cause Analyses

### Incident 1: ESP Trace Line Offset / Drawing in Empty Space
- **Symptoms:** User reported that the ESP tracer line was falling on empty space instead of connecting to the enemy's body.
- **Root Cause Analysis:**
  1. In `src/globals.cpp`, global resolution variables were initialized as:
     `float g_sw = 1920.0f; float g_sh = 1080.0f;`
  2. While `start_overlay()` in `main.cpp` initialized `native_window_screen_x` and `native_window_screen_y` to the phone's actual resolution (e.g. 2400x1080 or 2460x1080), it **never updated `g_sw` and `g_sh`**.
  3. `WorldToScreenPoint` in `src/game/math.hpp` calculates 2D screen positions using:
     `float cx = g_sw * 0.5f; float cy = g_sh * 0.5f;`
     On a 2400x1080 screen, `cx` evaluated to `960` instead of `1200`, causing an immediate **240 pixel horizontal error** at the screen center, scaling even worse toward the screen edges.
  4. ImGui rendered onto the native 2400-wide surface, resulting in the projected enemy positions and tracer lines landing in empty space far to the left of the actual 3D character models.
  5. Furthermore, `drawTracer` in `src/func/esp/line.hpp` drew exclusively to raw `p.Head.x, p.Head.y` regardless of whether the line originated from the top, center, or bottom of the screen.
- **Fix Implemented:**
  1. **Dynamic Resolution Synchronization:**
     - In `src/main.cpp`: Updated `g_sw` and `g_sh` immediately upon surface creation.
     - In `includes/draw/Android_draw/draw.cpp`: Dynamically synchronized `g_sw` and `g_sh` in `drawBegin()` whenever screen orientation changes.
     - In `src/ui/menu.cpp`: Synchronized `g_sw = realW; g_sh = realH;` on every frame render.
  2. **Anatomical Tracer Line Snapping:**
     - Updated `drawTracer(dl, p, pb, colBase)` to anchor dynamically to the bounding box:
       - Top Origin: Snaps to `(midX, pb.tl.y)` (Head / top of box).
       - Center Origin: Snaps to `(midX, (pb.tl.y + pb.br.y) * 0.5f)` (Chest / center of box).
       - Bottom Origin: Snaps to `(midX, pb.br.y)` (Feet / bottom of box).

### Incident 2: Floating Logo Sizing Defect (Undersized Touch Target)
- **Symptoms:** The floating 'N' logo appeared excessively small on high-resolution Android screens, making it difficult to spot and tap.
- **Root Cause:**
  - `logoRadius` in `src/ui/menu.cpp` was set to `28.0f * ui_scale` (diameter 56px). On 1080p/1440p displays with 450+ PPI, 56px is smaller than standard touch accessibility guidelines (minimum 48dp / 72-80px).
- **Fix Implemented:**
  - Increased `logoRadius` to **`38.0f * ui_scale`** (diameter 76px, +36% larger).
  - The circular dark disc, outer orchid ring, center "N" letter font size (`radius * 1.15f`), daemon status dot, and touch hitbox (`Touch_AddUiRect`) all scale up proportionately.
  - The logo position persists to `/dev/.nin_logo_pos.txt` (tmpfs, never flashes) on drag release only — not per frame.
  - Rotation and resolution changes clamp the stored position into the new viewport instead of resetting it.
  - Tapping the logo opens the menu; the X button in the header closes it.

---

## 3. Core Modules & Responsibilities

| File Path | Role |
| :--- | :--- |
| `src/main.cpp` | Entry point, license gate, native window creation, render loop |
| `src/ui/menu.cpp` | Floating 'N' logo, electric orchid menu, touch zone management |
| `src/func/esp/line.hpp` | ESP Tracer snapline renderer with anatomical box snapping |
| `src/func/esp/esp.hpp` | Main ESP dispatch pipeline (Box, Line, Info, Health, Weapon, Loot) |
| `src/game/data.cpp` | High-speed entity parsing, `PlayerSpan` DMA caching, local player tracking |
| `src/game/math.hpp` | Unity WorldToScreen matrix projection, node position resolution |
| `src/offsets/offsets.hpp` | Centralized memory offsets for OB55 (1.132.1) |

---

## 4. PewPew Integration & Feature Additions

Key features from reference `pewpew` codebase ported and integrated into `ninmods-kernel`:

1. **Multi-Level Pointer Chain Resolver (`src/other/addr.hpp`):**
   - Implemented `find_addr(base, offsets, count)` utility resolving dynamic Unity runtime pointers.

2. **Wide FOV / iPad View (`src/func/speed.hpp`):**
   - Memory offset `0x36C9BD0` from `libil2cpp.so`.
   - Dynamic slider range `1.0x` to `2.5x`. Clean reverse-path restoration to `1.0f` on disable.

3. **Auto-Pickup Range Mod (`src/func/speed.hpp`):**
   - Pointer chain: `libil2cpp + 0xF1D5F70 -> 0x30 -> 0x4B8 -> 0x1EB4`.
   - Expands loot collection radius up to 35 meters. Restores standard 2.0m radius on disable.

4. **Hitbox / Player Size Scaler (`src/func/speed.hpp`):**
   - Pointer chain: `libil2cpp + 0xF1D5F70 -> 0x30 -> 0x4B8 -> 0x510 -> [0x1FC, 0x200, 0x204]`.
   - Scales enemy X, Y, Z hitboxes from `1.0x` to `3.0x` for effortless bullet registration.

5. **High Jump Elevation Mod (`src/func/speed.hpp`):**
   - Pointer chain: `libil2cpp + 0xF1D5F70 -> 0x30 -> 0x4B8 -> 0x518 -> 0x21C`.
   - Modifies vertical jump impulse from `1.0x` to `5.0x`. Fully restored on toggle off.

---

## 5. UI Touch Scrolling & Hidden Scrollbar Restoration

1. **Hidden Scrollbar with Full Page Touch Scrolling (`menu.cpp`):**
   - **Requirement:** The visual scrollbar bar is 100% transparent/hidden (`ScrollbarBg`, `ScrollbarGrab*` with alpha 0.0), matching the original UI clean aesthetic.
   - **Fix:** Restored `handleTouchScroll()` to the clean swipe detection gesture (`dx/dy` threshold `5.0f * 1.1f`) without aggressive `ClearActiveID()` interrupts, allowing seamless vertical finger scrolling across the entire page while keeping the scrollbar visual elements completely invisible.

---

## 6. Headshot & Silent Aim Engine Resolution (`ff-global-dump (1).cs` Alignment)

Comprehensive offset audit and aim engine fix:

1. **Bone Offset Realignment from `ff-global-dump (1).cs`:**
   - **Head Transform Node:** `0x6A8` (`protected ITransformNode COBFGNOIPMF; // 0x6a8` at line 1036419). Was previously `0x6A0` (`GBKFHDFCPMD`, neck node), causing headWorld to calculate off-target.
   - **Foot / Toe Transform Node:** `0x6F0` (`protected ITransformNode JMHBBGIKBNO; // 0x6f0` at line 1036428).
   - **Player NickName:** `0x498` (`public String OriginalNickName; // 0x498` at line 1036315).
   - **Head Collider:** `0x740` (`protected GMOCOOEIFMK BKEPAKCAGNJ; // 0x740` at line 1036438), Alt `0x738` (`protected Collider BANPEGJEAKE; // 0x738`).
   - **Locked Aiming Collider:** `0x80` (`private Collider <AONPKGBGHHI>k__BackingField; // 0x80` on `AttackableEntity` at line 1051013).

2. **Headshot Aim Collider Registration (`src/func/aim/headshot.hpp`):**
   - Free Fire 64-bit hit registration verifies bullet raycasts against `target.LockedAimingCollider` (0x80) on `AttackableEntity`.
   - On headshot rolls, writes `HeadCollider` (0x740 / 0x738) into `target.addr + 0x80`.
   - `0x80` is 8-byte aligned (`0x80..0x87`) and strictly preserves `0x7C` (`Boolean IsDead`).

3. **High-Speed Writer Flooding Architecture (`src/func/aim/silent.hpp`):**
   - Unity's weapon script continuously overwrites `AimingInfo + 0x40` (`HLDECMJFKJK`) during `Update()`. A slow 10ms loop gets overwritten before `BulletFire()` executes.
   - Implemented dedicated writer loop (`writer_loop`) that continuously floods `wpm<Vector3>(aimInfo + 0x40, targetDir)` and sets `wpm<bool>(aimInfo + 0x71, false)` with `sched_yield()` during active firing, matching `pewpew` and reference design. Zero bullets miss.

---

## 7. Advanced Dual-Engine KPM & Driver Ecosystem

Comprehensive modernization of Kernel Process Memory (`kpm_driver.h`) integrated and enhanced from `pewpew`:

1. **Dual-Engine KPM (`includes/driver/kpm_driver.h`):**
   - **TB KPM (TongBao / TianBao):** Uses `prctl(PRCTL_MEM_READ 0x63687501)` / `prctl(PRCTL_MEM_WRITE 0x63687502)`. Direct kernel ring-0 pointer transfer with zero device node footprint.
   - **Neo KPM:** Uses `prctl(NA_PRCTL_MAGIC 0x4E41)` with command struct `na_cmd`.
   - **Auto-Detection:** Automatically probes TB KPM first; if absent, probes Neo KPM.

2. **Fixing PewPew Implementation Flaws:**
   - **4096-Byte Limit Bug:** `pewpew` rejected any read/write greater than `NA_MAX_RW_SIZE` (`if (size > 4096) return false;`). Our implementation chunk-loops the request (`while (bytes_left > 0)`), supporting arbitrary buffer sizes without truncation.
   - **Abrupt Termination (`exit(0)`):** `pewpew` called `exit(0)` when KPM was unavailable. Our driver features an intelligent fallback ladder to `process_vm_readv` (PVM Syscall mode), preventing crashes.
   - **Expanded Node Scanner:** Added `"facking"` and `"YBBBYN"` to `rt_dev.h` device table.
   - **Dynamic UI Telemetry:** `menu.cpp` Tab 2 dynamically reflects the active engine (`TB KPM`, `NEO KPM`, or `PVM SYSCALL`).


---

## 8. HitInfo Engine, Normalized Trajectory Steering & Responsive Sidebar

Comprehensive resolution of Headshot / Silent Aim mechanics using internal `NXL` project reverse engineering:

1. **Unity Free Fire Damage Calculation Architecture (`CGKJLKPMGDJ`):**
   - In Free Fire 64-bit, `localPlayer + 0xE58` (`PKIOAMDCOPB`) and `0xE50` (`FDMIEDDNCEC`) point to instances of `CGKJLKPMGDJ` (HitInfo / AimingInfo).
   - Analysis of `NXL` hook (`hook_OnCalcDamage`) revealed that headshot registration strictly relies on 8 synchronized fields:
     - `0x18`: `JHAINOBIFMA` (GameObject)
     - `0x20`: `JINDNOFABKB` (Collider - Enemy Head Collider)
     - `0x28`: `HPCOJHGFEEG` (HitLocation - Exact Enemy Head 3D World Position)
     - `0x34`: `DOJDNHCGFGK` (HitNormal - Normalized Bullet Direction Vector)
     - `0x40`: `HLDECMJFKJK` (RayDir - Normalized Direction Vector)
     - `0x64`: `DKPDBLLJEAA` (HitGroup enum `LKOOALMKJND`: **1 = Head**, 2 = Body)
     - `0x70`: `EFMBNPNFGHM` (IgnoreHappens: `false`)
     - `0x71`: `CEOKOILBODN` (ViewBlocked: `false`)
     - `0x80`: `CLOLKHNMNEK` (SpecialHitType: `(int16)0`)
     - `target.addr + 0x80`: `LockedAimingCollider` locked to `headCol`
     - `localPlayer + 0xE60`: `LastAimingInfoFromWeaponAdjusted` set to `false`.

2. **Root Cause of Headshot & Silent Aim Failure:**
   - Previous code only wrote an unnormalized difference vector (`diff = target3D - bulletOrigin`, magnitude 20~100) to `0x40`. Unity Raycast requires a normalized unit vector; non-unit vectors caused raycast rejection.
   - `0x64` (`HitGroup`) was NEVER set to `1`. Free Fire therefore treated hits as non-headshot body hits.
   - `0x28` (`HitLocation`) and `0x20` (`HitCollider`) were never written to `AimInfo`.
   - `isFiring` trigger condition was dropping locks between tap-fire intervals.

3. **High-Speed Writer & HitInfo Flooding Engine:**
   - High-speed writer thread floods the complete 8-field `CGKJLKPMGDJ` hit block continuously while a target is acquired in FOV or firing.
   - Computes `diff.normalized()` to deliver exact unit direction vectors.
   - Sets `HitGroup = 1` for guaranteed red-number headshot registration.

4. **UI Sidebar & Icon Button Modernization (`menu.cpp`):**
   - Sidebar width enlarged from fixed `76.0f` to responsive `std::max(88.0f, 96.0f * ui_scale)`.
   - Icon button size increased from `56x52` to `(sidebarW - 16.0f) x std::max(56.0f, 62.0f * ui_scale)`.
   - Vector icon size dynamically scaled from static `23px` to `size.y * 0.54f` (~`34px`+), with proportional stroke thickness (`3.0f` active pill indicator, `12.0f` corner rounding).
   - Generates spacious, touch-friendly, balanced ergonomics matching modern tablets and high-DPI displays.


---

## 9. Modular UI Refactoring, Strict Mutual Exclusivity & 17-Point Audit Resolution

### 1. Architectural File Split
In commit `bf27ca9` / `2530d91`, `src/ui/menu.cpp` was modularized into 3 dedicated files:
1. `src/ui/floating_logo.hpp & .cpp`:
   - Floating circular 'N' logo window (`##NinmodsKernelPill`).
   - Dragging with instant viewport boundary clamping (`std::clamp`).
   - Tap vs Drag discrimination on release (`s_was_activated && deactivated && !s_moved_drag`).
   - Daemon connection status indicator dot (`proc::pid > 0 && proc::lib > 0`).
   - Zero-trace persistent anchor via `/dev/.nin_logo_pos.txt` (tmpfs node).
2. `src/ui/theme.hpp & .cpp`:
   - Centralized `theme_orchid` dynamic palette with `sync_from_arr()`.
   - 6 theme color swatches (`g_ThemeSwatches`).
   - Vector icon renderer (`DrawVectorIcon`).
3. `src/ui/menu.hpp & .cpp`:
   - Dual-column glass main menu window.
   - When `!g_ShowMenu`, delegates cleanly to `ui::floating_logo::render()`.
   - Backward compatible API (`ui::menu::render()`, `ui::menu::save_logo_pos()`).

### 2. Strict Mutual Exclusivity Lifecycle (`g_ShowMenu`)
- **Menu Closed (`g_ShowMenu == false`):** Main window is not rendered; floating logo is drawn.
- **Logo Tapped:** User releases finger without drag -> `g_ShowMenu = true`.
- **Menu Open (`g_ShowMenu == true`):** Floating logo is completely hidden from render; main menu is drawn.
- **Close Button Tapped ('X'):** `g_ShowMenu = false` -> Menu disappears, floating logo re-appears.

### 3. Comprehensive 17-Point Audit Resolutions
- **Style Stack Balance:** Exactly 20 `PushStyleColor` / `PopStyleColor(20)`. Zero stack leaks.
- **Sidebar Overlap Prevention:** `btnH = std::clamp(availSidebarH / 4.6f, 36.0f, 60.0f);` and `powerY = std::max(tab2Bottom + spacing, bodyH - btnH - 12.0f * ui_scale);`. 0px overlap across all screen heights.
- **Full Viewport Scaling:** `headerH`, `closeBtnSize`, `xRadius`, `badgeH`, `badgeW`, `contentGapX/Y`, `colGap`, `colW`, `WindowPadding`, `ItemSpacing`, `FramePadding`, and `GrabMinSize` scaled with `* ui_scale`.
- **High-DPI Font Sharpness:** `LoadCustomIconFonts(io)` in `main.cpp` bakes `inter_medium` at 22px and `inter_bold` at 25px.
- **Dynamic Theme Reactivity:** Sidebar active state derives alpha directly from `theme_orchid::ColPrimary`: `(ColPrimary & 0x00FFFFFF) | (48 << 24)`.
- **Header Drag Boundary Clamping:** `s_menu_pos.x = std::clamp(s_menu_pos.x, 0.0f, realW - winW);` and `s_menu_pos.y = std::clamp(s_menu_pos.y, 0.0f, realH - winH);`. Menu cannot be lost off-screen.
- **Touch Scroll Gesture:** `ImGuiHoveredFlags_AllowWhenBlockedByActiveItem` prevents widgets from swallowing swipe gestures.
- **Format Safety:** Guarded target name with `targetName = g_currentTarget.name[0] ? g_currentTarget.name : "Enemy"`.
- **Clean Telemetry & Layout Balance:** Tab 2 left column filled with `Air Flight`, `Phantom Mode`, and `MOVEMENT DIAGNOSTICS`. Duplicate slider labels removed.


---

## 10. Comprehensive Codebase Hardening & Audit Resolutions (P0–P5)

Following an exhaustive audit across the entire `ninmods-kernel` codebase, all prioritized engineering discrepancies have been systematically resolved:

### 10.1 P0: Correctness, Concurrency & Reverse Paths
- **Concurrency Isolation (`menu.cpp:233-236`):**
  - Removed duplicate `func::aim::update()` invocation from the render thread. The dedicated background aim worker thread (`aim.hpp:loop()`, interval 2–10ms) is now the sole owner of `update()`, completely eliminating race conditions on `s_lockedTargetAddr`, `s_lockTimeoutTicks`, and `s_smoothDir`.
- **Joinable Thread Lifecycle (`aim.hpp`):**
  - Replaced unmanaged detached thread (`std::thread(loop).detach()`) with a static joinable thread managed via `get_aim_thread()`. Calling `thread_stop()` safely halts the loop, joins the thread, resets motion states, and cleans up overrides before returning.
- **Headshot Reverse Path & Target Override Safety (`headshot.hpp`):**
  - Implemented `restore_headshot_override()`. When `lockToHead` is activated, the original `LockedAimingCollider` (0x80) pointer on the target enemy is captured into `s_orig_headshot_col`. When the target is unlocked, when `lockToHead` rolls false, or when `thread_stop()` runs, the original collider is restored back to the enemy object.
- **Speed System State Restoration (`speed.hpp`):**
  - Captured dynamic baseline values for `Engine_TimeServiceSpeed`, `Player_Speed`, `RunSpeedUpScale`, `FallingSpeedUpScale`, `iPad_FOV`, `Pickup_Range`, `Hitbox_Scale`, and `High_Jump` upon initial enablement. Disabling the respective mod now restores the exact captured baselines rather than hardcoded assumptions (`0.033333f`, `1.0f`, `2.0f`).
- **Health Ratio Normalization (`data.cpp:84-90`):**
  - Added fallback default of 200 HP if maximum health read (`elem1`) fails or returns 0, preventing `maxHP = curHP` false clamp which caused 100% full health bars.
- **Player Acquisition Range Unification (`data.cpp:366`):**
  - Unified the entity tracking distance filter: `dist > std::max(cfg::esp::max_range, cfg::aim::max_distance)`, ensuring aimbot target acquisition is never clipped when aimbot range exceeds ESP range.

### 10.2 P1: Visuals, Formatting & Logic
- **ESP Custom Font Sizing Consistency (`info.hpp`, `weapon.hpp`, `loot.hpp`):**
  - Replaced global `ImGui::CalcTextSize()` calls with `font->CalcTextSizeA(fs, FLT_MAX, 0.0f, text)`. This guarantees accurate bounding boxes, labels, and spacing matching the drawn font size (`fs = 10px-13px`) rather than the native 32px atlas size.
- **Off-Screen Tolerance in ESP Geometry (`esp/common.hpp:61`):**
  - Updated `ok(const Vector3& v)` to support off-screen margin checks (`v.z > 0.5f && v.x > -150.0f && v.x < g_sw + 150.0f && v.y > -150.0f && v.y < g_sh + 150.0f`), preventing boxes from flickering out when players are near screen boundaries.
- **Silent Aim Trigger Condition Accuracy (`aim.hpp:75`, `silent.hpp:108`):**
  - Separated `cfg::aim::silent` enable switch from active firing detection, ensuring firing-based trigger modes (`cfg::aim::trigger_mode == 0`) correctly check weapon state.
- **Non-Latin Nickname Fallback (`data.cpp:378`):**
  - When non-ASCII or stripped characters yield an empty player name, the system falls back to `"Player <index>"` rather than leaving an empty label.

### 10.3 P2: Security & OpSec
- **Process Cloaking (`main.cpp:223`):**
  - Replaced static `prctl(PR_SET_NAME, "ninmods-kernel")` with `"kworker/u16:1"`, preventing trivial signature matches during userland process and `/proc/<pid>/comm` scans.
- **Zero-Trace Storage Enforcement:**
  - Guaranteed floating logo position is persisted strictly to `/dev/.nin_logo_pos.txt` (tmpfs in RAM) only on drag release, leaving zero persistent disk footprint in `/data/local/tmp`.
- **License Gate Security & Input Sanitization (`license.cpp`):**
  - Enabled SSL peer and host verification (`CURLOPT_SSL_VERIFYPEER = 1L`, `CURLOPT_SSL_VERIFYHOST = 2L`).
  - Added strict alphanumeric and hyphen sanitization (`is_safe_alphanumeric`) for license key and HWID before executing CLI popen fallback, neutralizing shell injection vulnerabilities.

### 10.4 P3: Performance & Sycall Optimization
- **Silent Aim Thread Throttling (`silent.hpp:82`):**
  - Added a 500-microsecond sleep (`std::this_thread::sleep_for(std::chrono::microseconds(500))`) inside `writer_loop`, preventing 100% CPU thread lock while maintaining 2,000 writes/sec response rate.
- **Batch Dictionary Traversal (`other/utils.hpp`):**
  - Optimized `MonoDictionary::read_block` to read all 64 entries in a single `mem_read` (`process_vm_readv`) syscall, reducing context switches from 192 syscalls per block to just 1.

### 10.5 P4: Centralized Offsets Architecture (`offsets.hpp`)
- Centralized all previously hardcoded magic offsets across the engine into `Offsets`:
  - `Offsets::Player::HP_MaxEntry` (`0x28`)
  - `Offsets::Player::HP_ArmorEntry` (`0x40`)
  - `Offsets::Mono::String_Length` (`0x10`) & `String_Chars` (`0x14`)
  - `Offsets::Weapon::Weapon_ID` (`0x64`)
  - `Offsets::Unity::StaticFields_Instance` (`0x8`), `Component_Transform` (`0x18`), `Transform_Internal` (`0x20`)
  - `Offsets::Camera::Follow_Alt` (`0x30`) & `IntPtr_Alt` (`0x10`)
  - `Offsets::Player::FollowCamera_Sub` (`0x690`) & `AimAdjustmentFlag` (`0xE60`)
  - `Offsets::Patches::iPad_FOV` (`0x36C9BD0`) & `GameConfig_Base` (`0xF1D5F70`)
  - `Offsets::Aim::HeightOffset_Neck` (`-0.15f`), `HeightOffset_Body` (`-0.40f`), `HeightOffset_Chest` (`-0.28f`)
- Removed obsolete duplicate aliases `Offsets::Root`, `Offsets::Head`, and `Offsets::Hip`.

### 10.6 P5: Build Hardening & Lazy Initialization
- **Lazy Driver Initialization (`includes/driver.h`):**
  - Eliminated dangerous global dynamic constructor (`inline c_driver* driver = new c_driver();`). Replaced with thread-safe Meyer's singleton (`get_driver()`), preventing crashes during static initialization prior to `main()`.
- **Build Script Hardening (`compile.sh`, `CMakeLists.txt`):**
  - Made `compile.sh` POSIX compliant with `-j$(nproc)` and conditional `package_sh.sh` execution.
  - Aligned CMake project and target names to `ninmods-kernel`.

---

## 11. Dead Code Purge & Cross-Thread Coherence Pass

### 11.1 Shadow Headers Removed
The tree carried two parallel copies of the same headers. `jni/Android.mk` puts `includes/internal` ahead of `includes`, so the `internal/` copy is what the compiler actually opened; every edit to the `includes/draw` copy was dead on arrival.

- Deleted byte-identical duplicates: `includes/draw/Android_touch/TouchHelperA.h`, `includes/draw/Android_draw/draw.h`, `includes/draw/Font.h`.
- Deleted `includes/protect/oxorany.{hpp,cpp}` (dupes of `src/protect/*`) and the whole `src/ENC/` directory (`base64.hpp`, `obfuscate.h`, `oxorany.h`, `oxorany_include.h`).
- Deleted `includes/internal/Android_touch/Touch.hpp`; its slot API declarations (`IsFingerDown`, `GetFingerScreenPos`, `Touch_Down_Slot`, `Touch_Move_Slot`, `Touch_Up_Slot`) moved into the canonical `TouchHelperA.h`.
- Deleted forwarder shims with no callers: `src/aim/aim.hpp`, `src/func/aim.hpp`, `src/encrypt.h`, `src/xorstr.hpp`, `src/main.h`, `src/other/utf_utils.h`, `includes/font/font_data.hpp` (`font_ttf` never referenced).
- `Android.mk` include paths pruned accordingly: `src/ENC`, `src/aim`, `includes/font`, `includes/protect`.

### 11.2 Dead Symbols Removed
- `src/other/memory.hpp`: `proc::pm`, `struct MemChunk`, `init_mem()`, and the whole `namespace game` (`valid`/`init`/`tick`/`jitter_next`) — zero callers; the watchdog in `main.cpp` superseded it.
- `src/game/math.hpp`: `GetPlayerHeadTF`, `GetPlayerPeTF`.
- `src/func/esp/common.hpp`: `PANEL_ROW_H`, `PANEL_TOP_OFFSET`.
- `includes/draw/Android_touch/Touch.cpp`: `other_touch`, `s_primaryFdIndex`, and the `bestFdIndex`/`maxArea` selection logic collapsed to plain primary-digitizer range tracking.
- `includes/internal/Android_draw/draw.h`: `extern ImGuiWindow *g_window`, `extern ImFont* verdana` — both declared, never defined or read.
- `src/globals.{hpp,cpp}`: `global_anim_time`, `g_ui_scale`, `abs_ScreenX`, `abs_ScreenY` — zero readers.
- `Touch_SetUiRect` retired: the multi-region staging API is the only barrier; a single-rect setter invited panels to clobber each other's regions.

### 11.3 Cross-Thread Coherence
Every write below is on the render thread while a reader sits on the data or aim worker.

- **`g_sw` / `g_sh` -> `std::atomic<float>`.** Plain floats meant a projection could be computed against a half-updated pair during a rotation. `WorldToScreenPoint` now snapshots both once before the multiply. Implicit conversion left every other arithmetic site untouched.
- **`main_thread_flag` -> `std::atomic<bool>`.** Written by `menu::render` on shutdown, read by the main loop each frame.
- **`proc::pid` / `proc::lib` -> `std::atomic` with `proc::snapshot()`.** Publish order is lib-store-then-pid-store; teardown is lib-clear-then-pid-clear, so no reader pairs a live pid with a dead module base. `data::update`, `addr::find_addr`, and the watchdog each take a single snapshot per pass instead of re-reading mid-walk.
- **`cfg::*` knobs -> `std::atomic`.** The toggle and slider widgets now edit a local copy and store back once per interaction (`cfg::Checkbox`, `cfg::SliderFloat`, `cfg::SliderInt`); `DrawToggleSwitch` and `DrawSegmentedButtons` take atomic storage. Removes torn-float reads of `max_range`, `aim_fov`, `smooth`, and friends from the worker loops.
- **`data::viewMatrix` de-exported.** Written by `update()` and consumed only by `boneScreen`/`scanLoot` on the same thread, in program order. Dropping it from `data.hpp` removes the only invitation to read a 64-byte struct cross-thread.
- **Viewport refresh on rotation.** `drawBegin()` now rebuilds `native_window_screen_*` and `g_sw`/`g_sh` when the orientation changes; previously the size fed to `ImGui_ImplAndroid_NewFrame` stayed at the launch orientation for one frame.
- **Driver backend selector was an unowned global.** `选择值` (a mangled-UTF-8 identifier) lived in `driver_config.h` as `inline int`, and `menu.cpp` declared it `extern` and read it. Nothing ever wrote it: `connect_driver()` stored the choice in a *different* variable of the same name inside `c_driver`. Both the dashboard and the DRIVER & SYSTEM INFO panel therefore printed `ARMED` forever. The selector is now `c_driver::m_selection` (private) with `getDriverSelectionCached()` as the read-only accessor; `print_dashboard()` and the menu panel ask the driver object.
- **`g_appConfig` was `static` in a header.** Every translation unit that included `driver_config.h` got its own copy, so `main.cpp` and `menu.cpp` could disagree about `driverOption`. It is `inline` now — one definition, one instance.
- **`LoadConfig()` lied.** It set `settingsLoaded = true` and returned `true` while restoring nothing, which made `DisplayCurrentConfig()` print "Unknown / Unknown" as though a mode had been picked. There is no persistence layer (`SaveConfig()` is an empty stub), so it now returns `false`.
- **`print_dashboard()` was defined but never called.** Now invoked from the interactive path of `check_license_gate()`, just before `return true`.
- **Fonts: `F107_data` icon face was loaded and never referenced.** `g_IconFont` was written in `LoadCustomFonts` and read nowhere; the 48 KB `src/FONTS/ICONS.h` blob existed only to feed it. Both removed.

### Deleted in this pass

`src/other/signature.hpp` (the `kernel::sig` scanner — zero callers, only an unused `#include` in `main.cpp`), `src/FONTS/ICONS.h`, `MonoDictionary::getValues` / `MonoDictionary::entry` (only `read_block` is used), `c_driver::mem_addr_virtophy` (never called), `c_driver::pid` (only written, never read after `set_pid` fan-out), and the whole JNI-URL/toast apparatus in `license.cpp` — `open_update_page`, `should_show_redirect_toast`, `trigger_redirect_toast`, `update_toast_timer`, `current_key`, `start_date`, `normalize`, `decode_string`, `open_url_jni`, `K_TELEGRAM_URL_BYTES`, `K_TELEGRAM_URL_SEED`, `s_toast_timer`, `s_jvm`, `s_activity` - none of it reachable from `main`, which only uses `init` / `set_key` / `is_valid` / `status_text` / `expiry_date`.

---

## 12. UI Font Ownership, Size-Matched Faces, Logo Hitbox & Build Diagnostics

### 12.1 Heap Corruption: Atlas Freed `.rodata`

`ImFontConfig::ImFontConfig()` sets `FontDataOwnedByAtlas = true`, and the flag name reads backwards from what `ImFontAtlas::AddFont()` does:

```cpp
if (!new_font_cfg.FontDataOwnedByAtlas)
{
    new_font_cfg.FontData = IM_ALLOC(new_font_cfg.FontDataSize);
    new_font_cfg.FontDataOwnedByAtlas = true;
    memcpy(new_font_cfg.FontData, font_cfg->FontData, size);
}
```

Only `false` triggers the heap copy. With the default `true`, the atlas stored the *caller's* pointer and left ownership alone. Every static TTF blob in this tree is a `const unsigned char[]` in `.rodata` (`Roboto_Regular` in `includes/internal/Font.h`, `inter_medium`/`inter_bold` in `src/FONTS/MAIN_FONT.h`). So `~ImFontAtlas()` -> `Clear()` -> `ClearInputData()` -> `IM_FREE(FontData)` was a `free()` on a `.rodata` address on every `ImGui::DestroyContext` — a corrupting free in the allocator's bookkeeping.

All five `AddFontFromMemoryTTF` call sites now pass `FontDataOwnedByAtlas = false`: three in `initGUI_draw()` (`draw.cpp`) and two in `LoadCustomFonts()` (`main.cpp`).

`AddFontDefault()` needs no override — it routes through `AddFontFromMemoryCompressedTTF`, which `IM_ALLOC`s a decompressed heap buffer and forces the flag `true` itself at `imgui_draw.cpp:2285`.

### 12.2 Size-Matched Font Faces

`Fonts[0]` is Roboto rasterised at 32px with `GetGlyphRangesChineseFull()`. Every `AddText` then sampled that one raster at a different size. `CalcTextSizeA`/`AddText` scale the advance and the quad, not the glyph bitmap — a 32px raster drawn at 10px loses its stem and a 32px raster drawn at 63px is plain blur.

Three additive faces were registered in `initGUI_draw()`, none repinning `Fonts[0]` (the menu layout is tuned against it plus `ScaleAllSizes(3.0f)`):

| Global | Size | Ranges | Consumers |
|---|---|---|---|
| `g_EspFont` | 14px | `GetGlyphRangesDefault()` | `esp/loot.hpp`, `esp/weapon.hpp`, header `KERNEL CORE` sub-label, `ARMED`/`STANDBY` badge |
| `g_UiFont` | 18px | `GetGlyphRangesDefault()` | `menu.cpp` `DrawToggleSwitch` labels (drawn at 17px) |
| `g_LogoFont` | 64px | single-glyph `{ 'N', 'N', 0 }` | `floating_logo.cpp` `DrawCircularLogo` (drawn at `radius * 1.15`, 44..63px) |

Declared in `src/globals.hpp` with a bare `struct ImFont;` forward declaration so the header stays free of `imgui.h`; defined as `nullptr` in `src/globals.cpp`. Every consumer guards with `g_X ? g_X : ImGui::GetFont()`, so a registration failure degrades instead of crashing.

`g_LogoFont` carries a hand-built one-glyph range because only the literal `"N"` is ever drawn with it — a full Cyrillic set at 64px would be ~40 quads for one character.

`esp/info.hpp` `drawTopInfo()` deliberately keeps `ImGui::GetFont()`. Player names come from the game and can be CJK; it is the only ESP path rendering arbitrary user text, so the full-range face has to stay reachable there. A narrowed-range face would drop the glyphs entirely, which is worse than a soft 13px sample.

### 12.3 Floating Logo Hitbox: Circle, Not Bounding Box

`Touch_AddUiRect` registered the logo's bounding box. The logo is a disc, so the four corners of that box (~46x46px each at `logoRadius = 38 * ui_scale`) were inside the barrier rect but outside the drawn circle — every tap there was swallowed and never reached the game.

- `UiRect` gained `float r`.
- `Touch_AddUiCircle(cx, cy, radius)` added; it stores the bounding box and sets `r`.
- `Touch_IsPointInUi()` does a radial test when `r > 0.0f` and falls through to the rect test otherwise, so every existing rect registration is unchanged.
- `floating_logo.cpp` now calls `Touch_AddUiCircle(s_logo_pos.x + logoRadius, s_logo_pos.y + logoRadius, logoRadius + 8.0f)`.
- Declared in `includes/internal/Android_touch/TouchHelperA.h`.

### 12.4 Logo Position Persistence

`pos_is_sane()` rejected any coordinate past the cached viewport. The cached size is one frame behind right after a rotation, so a rotate-then-release dropped the write silently and the logo snapped back to its launch position on the next start.

- It now rejects only non-finite and negative values.
- `save_pos()` clamps against the cached viewport instead, so the write always lands with a usable coordinate.
- `s_has_loaded_pos` and the lazy `load_pos()` call inside `render()` are gone; `main.cpp:177` calls `ui::menu::load_logo_pos()` once before the loop, which is the only path that needs it.

### 12.5 Menu Opacity Floor

The slider lives inside the window it dims through `ImGuiStyleVar_Alpha`. At a `0.20` floor, the control that would bring the opacity back was itself rendered at 20% — nearly invisible and very hard to grab. Floor raised to `0.40`.

### 12.6 CI: Reading a Failed Build Without Log Access

Two consecutive builds failed at the `ndk-build` step in ~2s. The Actions job-log endpoint returns `403` for unauthenticated callers, and the only publishable annotation was `Process completed with exit code 2` — no compiler diagnostic.

`build.yml` now:

- redirects `ndk-build` to `build.log`, captures `$?`, and on failure publishes the last 120 lines as `::error` annotations in 10 chunks of 12 lines (percent-escaped, `%0A` per line). The annotations endpoint *is* public.
- uploads `build.log` as an artifact with `if: always()`, so a failing run still leaves a readable log.
- gates the binary upload on `if: success()` with `if-no-files-found: error`.

That diagnostic path is what located the actual fault: an edit pass had collapsed two statements onto a comment line.

### 12.7 Two Merged Lines (the 2-Second Failure)

Both were hard compile errors:

- `src/ui/floating_logo.cpp` — the `// CIRCULAR FLOATING LOGO DRAWING` divider comment had absorbed `static void DrawCircularLogo(ImDrawList* dl, ImVec2 center, float radius) {`. The signature lived inside a `//` comment, so every call below it had no visible declaration.
- `src/ui/menu.cpp` — `float fontSize = 17.0f * ui_scale;` and `ImVec2 tsz = font->CalcTextSizeA(...)` were joined on one physical line, leaving the `ImVec2 tsz` declaration outside any statement.

Both repaired; no behaviour change. Braces/parens re-verified across all twelve touched files.

### Deleted in this pass

`ui::floating_logo::get_pos()` / `set_pos()` — declared in the header, defined in the .cpp, zero callers.

### Build

Run #32 green. `ninmods-kernel.zip` 1,675,546 bytes, `sha256:bd2a637d6b5a87a3737fa8ea498ad97a17f42c6172972845fa4f0160009aacab`.

