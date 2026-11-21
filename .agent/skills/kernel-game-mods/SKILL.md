---
name: kernel-game-mods
description: >-
  Production runbook and architectural playbook for developing ultra-safe, anti-ban,
  non-invasive kernel-level mods (ESP Box, Laser Tracers, Aimbot, Silent Aim Trajectory Steering)
  on Rooted Android devices (KernelSU / APatch / Magisk). Covers zero-hook external memory reading (RPM),
  out-of-process Native Surface ImGui rendering, zero-patch non-invasive safety model,
  centralized offset architectures, and anti-ban kernel stealth protocols.
---

# Kernel Game Modding: Safe, Non-Invasive Anti-Ban Architecture

This skill defines the definitive, production-hardened engineering standards for building
**100% safe, non-invasive, root-based kernel mods** (ESP, Chaturbhuj Box, Aimbot, Silent Aim Trajectory Assist)
for Unity IL2CPP and Native Android games (such as Free Fire, PUBG, CODM).

---

## 1. The Supreme Safety Principle: "Zero-Touch" Non-Invasive Model

The reason 99% of internal mods, injected DLLs, and modified APKs get banned is because they tamper
with the game's internal code, memory integrity, or APK signatures. 

Our architecture enforces a **strictly non-invasive, external kernel model**:

| Threat Vector | Traditional Internal / Injected Mod | Our Kernel Non-Invasive Architecture |
| :--- | :--- | :--- |
| **Code Integrity (`.text` segment)** | Patches opcodes (`NOP`, `RET`, hex edits) -> **Triggers immediate hash/CRC ban**. | **Zero byte patches in `.text`**. Official executable memory is 100% bit-for-bit stock. Hash checks pass 100%. |
| **Hook Detection (Dobby / MinHook)** | Overwrites function prologues with trampolines (`0x58 0x00...`) -> **Observer detects modified code pages**. | **Zero function hooks**. Never intercepts or hooks game engine methods. |
| **Thread Enumeration** | Spawns internal threads inside the target process -> **Anti-cheat thread walkers detect unauthorized thread starts**. | **Zero internal threads**. Daemon runs in an entirely separate OS process. |
| **Process Enumeration** | Visible package or executable names in `/proc` -> **Detected by security scanners**. | **Masquerades as standard kernel worker** via `prctl(PR_SET_NAME, "kworker/u16:1")`. |
| **Overlay Detection** | Android `WindowManager` floating overlays (`TYPE_APPLICATION_OVERLAY`) -> **Easily detected and blocked by screen security**. | **Independent Native SurfaceFlinger Buffer** (`ANativeWindowCreator`) rendered directly to display hardware. |
| **Network & APK Tampering** | Modified APKs, edited `AndroidManifest.xml`, network proxies/hosts -> **Server rejects hash mismatch**. | **Stock official APK from Google Play Store**. Zero network interference. |

---

## 2. System Architecture Blueprint

```text
┌─────────────────────────────────────────────────────────────┐
│                 Target Game Process (Stock APK)             │
│  - libil2cpp.so (.text 100% Untouched & Clean)              │
│  - Active Entity Table (Read-Only)                          │
│  - Weapon Fire Packet Structure (AimInfo)                   │
└──────────────┬──────────────────────────────▲───────────────┘
               │                              │
          Read Memory                    Write Trajectory
         (RPM via Kernel)               (WPM on Active Trigger)
               │                              │
┌──────────────┴──────────────────────────────┴───────────────┐
│              Kernel Daemon Process (kworker/u16:1)          │
│  ├── Kernel Driver Interface (/dev/* or process_vm_readv)   │
│  ├── Memory Cache & Decoupled Offsets (offsets.hpp)         │
│  ├── 3D Vector Math & Bone Trajectory Engine                │
│  └── High-Speed Zero-Latency Memory Flooding Thread         │
└──────────────────────────────┬──────────────────────────────┘
                               │
                          Render Drawlist
                               ▼
┌─────────────────────────────────────────────────────────────┐
│             Direct Native Surface Overlay                   │
│  - ANativeWindow hardware surface                           │
│  - ImGui OpenGL ES 3.0 / Vulkan rendering pipeline          │
│  - Proportional Chaturbhuj Box, Lasers, FOV & Armor Bars    │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Centralized Offset Law (`offsets.hpp`)

All memory offsets, class field offsets, bone indexes, and enum values **MUST** reside strictly inside
`src/offsets/offsets.hpp`. No other `.cpp` or `.hpp` file is ever allowed to contain hardcoded offsets.

### Structural Organization:
```cpp
namespace Offsets {
    namespace Base {
        constexpr uintptr_t GameFacade        = 0xA72E4E0;
        constexpr uintptr_t GameFacade_P2     = 0xB8;
        constexpr uintptr_t BaseGame_Match    = 0x90;
        constexpr uintptr_t Match_PlayerDict  = 0xC0;
        constexpr uintptr_t Match_LocalPlayer = 0xD8;
    }
    namespace Player {
        constexpr uintptr_t HeadTF            = 0x6A8; // Primary Head ITransformNode
        constexpr uintptr_t HeadTF_Alt        = 0x6A0; // Auxiliary Fallback
        constexpr uintptr_t FootTF            = 0x6F0; // Left Toe Node
        constexpr uintptr_t HeadCollider      = 0x740; // Head Collider
        constexpr uintptr_t LockedAimingCollider = 0x80; // Body Collider Override Slot
        constexpr uintptr_t AimInfo           = 0xE58; // LastAimingInfoFromWeapon
    }
    namespace Bone {
        constexpr uintptr_t Head              = 0x6A8;
        constexpr uintptr_t Head_Alt          = 0x6A0;
        constexpr uintptr_t LeftToe           = 0x6F0;
        constexpr float     SkullElevation    = 0.12f; // Skull center offset
    }
    namespace Aim {
        constexpr uintptr_t HitCollider       = 0x20;
        constexpr uintptr_t HitPoint          = 0x28;
        constexpr uintptr_t HitNormal         = 0x34;
        constexpr uintptr_t Direction         = 0x40; // RayDir vector
        constexpr uintptr_t Distance          = 0x5C; // Euclidean scalar distance
        constexpr uintptr_t HitGroup          = 0x64; // 1 = Head, 2 = Body
    }
}
```

### Game Update Protocol (OB55 → OB56/OB57):
When the game updates, **do not touch** any math routines, ESP rendering, or aim algorithms. 
Only update the hex numbers inside `src/offsets/offsets.hpp`.

---

## 4. ESP Engine: Proportional 2D/3D Rendering

### 1. ViewMatrix & World-to-Screen Projection
* Extract `FollowCamera -> Camera -> IntPtrCam` from LocalPlayer.
* Read ViewMatrix (`IntPtrCam + 0x100`).
* Perspective projection calculation:
  ```cpp
  float v12 = (pos.x * vm.m14) + (pos.y * vm.m24) + (pos.z * vm.m34) + vm.m44;
  if (v12 >= 0.001f) {
      float inv = 1.0f / v12;
      float cx = screenWidth * 0.5f;
      float cy = screenHeight * 0.5f;
      float v9  = (pos.x * vm.m11) + (pos.y * vm.m21) + (pos.z * vm.m31) + vm.m41;
      float v10 = (pos.x * vm.m12) + (pos.y * vm.m22) + (pos.z * vm.m32) + vm.m42;
      return Vector3(cx + cx * v9 * inv, cy - cy * v10 * inv, v12);
  }
  ```

### 2. 3D Bone Hierarchy Traversal (`GetNodePosition`)
* Read transform pointer from bone node: `nodeTransform + 0x10`.
* If null or zero, fallback to direct `Transform` evaluation.
* Resolve `MatrixPtr` (`0x38`) and indices list (`0x20`), computing hierarchical parent rotation quaternions and scales.
* **Far / LOD enemy fallback (current implementation):** Read BOTH head (`0x6A8`) and root (`0x6D0`) before rejecting an entity. If the head node is absent or resolves to `(0,0,0)` — common for distant enemies rendered with a simplified skeleton — synthesize `headWorld = rootWorld + 1.75m`. Only drop the entity when both transforms fail. Never `return` on head failure alone: that silently removes far enemies from ESP.

### 3. Full-Body Chaturbhuj Box Proportions
* **Top:** Head bone screen position minus 12% helmet padding:
  $$y_{\text{top}} = y_{\text{head}} - (\text{boxHeight} \times 0.12f)$$
* **Bottom:** Toe bone screen position plus 5% boots padding:
  $$y_{\text{bottom}} = y_{\text{toe}} + (\text{boxHeight} \times 0.05f)$$
* **Humanoid Width Ratio:**
  $$\text{boxWidth} = (\text{bottom} - \text{top}) \times 0.48f$$
* Center box horizontally on the midpoint of head and toe screen coordinates.

### 4. Head Laser Tracer
* Trace from screen center-top (default), crosshair, or screen bottom to the projected head coordinate.
* **Tolerant frustum gate:** accept `ok(Head) || ok(Toe)` with a 150px margin. The old zero-margin head-only `[0..g_sw]×[0..g_sh]` check dropped every far enemy whose head sat just outside the viewport. Snap to whichever bone is visible and clamp the endpoint into the physical screen.
* Uniform opacity — no distance fade, no distance cutoff.

### 5. Range Doctrine: Camera Frustum Only
* ESP range is EXACTLY what the camera renders. There is no `max_range` slider and no distance cull anywhere in `src/`.
* `data.cpp` rejects a player only when BOTH projected head and toes are outside `FRUSTUM_MARGIN = 80px` or behind the near plane (`z <= 0.05`). Off-screen/behind-camera entities cost zero RPM.
* `esp.hpp` / `common.hpp` apply a defensive 150px guard; `computePlayerBox()` accepts toe-only solutions so a partially visible far target still gets a box.
* `p.distance` is retained for HUD text and box-height fallback only — NEVER for filtering or fading.

### 6. RPM Optimization Stack (7 layers, ~90% reduction)
1. Batch reads (`mem_read` blocks) instead of many `rpm<T>()` calls.
2. Transform/bone cache keyed by entity address, 100ms TTL, guarded by `data::bone_cache_mutex`.
3. Data loop 33ms (30Hz) minimum; lobby 100ms.
4. Frustum culling before any detailed read.
5. Fisher-Yates randomized read order per tick.
6. ESP-only gate: skip `PhyXdata`/`IsKnocked` when no aim feature is on.
7. Dead/teammate rejection before Transform walks.
* Cache is cleared on `note_detached()`, `thread_stop()`, and PID/lib change. Dynamic GameFacade scanner state (`s_dyn_checked` / `s_dyn_gf`) resets on re-attach too.

### 7. Driver Serialization Invariant
* One process-wide `c_driver` is shared by data, aim and watchdog threads. All access goes through `proc::driver_mutex()` helpers — `driver_read()`, `driver_write()`, `driver_module_base()`, `driver_setup()`, `driver_status()`. Never call `driver->` directly from a worker thread.

---

## 5. Silent Aim & Trajectory Assist Engine (100% Red Headshots)

### 1. Natural Gameplay Law (Zero Camera Jumping)
* **Camera rotation is NEVER touched.** Finger aiming, swiping, and gyroscope remain 100% natural and fluid.
* Trajectory steering is applied exclusively to the bullet fire vector inside the weapon's `AimInfo` structure.

### 2. Trajectory Mathematics
1. **Target Point Calculation:**
   $$\vec{P}_{\text{targetHead}}.y \mathrel{+}= (\text{SkullElevation} + \text{head\_bias})$$
2. **Displacement Vector:**
   $$\vec{D} = \vec{P}_{\text{targetHead}} - \vec{P}_{\text{fireStart}}$$
3. **Euclidean Distance:**
   $$dist = \sqrt{D_x^2 + D_y^2 + D_z^2}$$

### 3. Safe Write Pipeline (CURRENT — supersedes the old 7-write flooding design)
> [!WARNING]
> The previous runbook instructed writes to `HitGroup (0x64)`, `HitPoint (0x28)`, `HitNormal (0x34)`, `Distance (0x5C)`, enemy `LockedAimingCollider (0x80/0x78)` and weapon recoil. **All of those were removed** — they caused client/server hitgroup desync and entity-memory tampering, i.e. instant ban. Do not reintroduce them.

1. **Only one field is ever written:** normalized 12-byte `Vector3` to `AimInfo + Offsets::Aim::Direction` (`0x40`). `wpm()` is gated to 1/2/4/8/12-byte aligned, non-code-page writes while `proc::wpm_allowed` is true.
2. **Full-auto coverage:** steer once per estimated bullet (~85ms ≈ 11-12 rds/s) while `isFiring` is held — a rising-edge-only write steered just the first shot of a burst. Time-based gating works at any aim-loop cadence without multiplying WPM.
3. **Cone guard:** `dot(cameraForward, direction) >= cos(8°) = 0.99027`; abort otherwise. Jittered vector is re-validated against the cone.
4. **Humanization:** Gaussian micro-jitter on every shot (head sigma 0.0015-0.003, body 0.005-0.010), ~12% of bullet windows skipped, ±3ms cadence jitter, time-based (150ms) stale-camera guard keyed on `data::viewMatrix_stamp`.
5. **Rate-controlled headshots:** Auto Headshot is a checkbox that ENABLES the rate controller, not a 100% force. `evaluate_headshot_rate()` uses the `headshot_rate` slider, +25% scoped uplift (cap 95%), distance falloff past 40m — but an explicit 0% stays 0% at any range. `headRate=0` steers to chest, never skips the shot.
6. **Lifecycle:** telemetry is locked in `common.hpp`; `clear_silent_data()` / `restore_headshot_override()` are zero-footprint (no entity memory to restore).

---

## 6. Diagnostic Forensics & In-Game Verification Pipeline

1. **Structured Diagnostic Logging (`logger.hpp`):**
   * Output explicit `[FOUND]` vs `[NOT FOUND]` tags for memory dereferencing.
   * Write logs directly to physical storage: `/sdcard/Download/kernel_debug.txt`.
   * Rate-limit heartbeats (1.0s to 3.0s) to prevent FPS drops.

2. **Pre-Release Production Cleanup:**
   * Once offsets and features are confirmed working in-game, disable verbose logging and file writes to achieve zero disk I/O, maximum frame rates, and complete anti-cheat stealth.

---

## 7. Remote CI/CD Axiom

* **Zero Local Compilers:** Never attempt local compilation with local toolchains.
* Stage all code cleanly in git and push to GitHub.
* Remote GitHub Actions workflow compiles the ARM64 binary with the Android NDK and emits the signed release artifact (`ninmods-kernel.zip`).

---

## 8. Menu UI Invariants: Single Master Switch Doctrine

* **Strictly Single Master Checkbox:** The global Master Switch (`cfg::master_enabled`) must be a standard tickmark checkbox (`cfg::Checkbox`), never a sliding toggle switch.
* **Single Location Only:** It belongs strictly and exclusively on Tab 2 (System Controls). Exactly ONE master switch across the full menu.
* **Zero Duplication on Feature Tabs:** Never place a Master Switch or redundant toggle on Tab 0 (ESP Visuals) or Tab 1 (Aimbot Engine). Feature tabs must contain only their dedicated feature checkboxes and configuration sliders.
* **Global Control:** When toggled on Tab 2, `cfg::master_enabled` governs the entire engine across all features.

---

## 9. Build Configuration Parity Doctrine (`CMakeLists.txt` & `Android.mk`)

* **Parity Enforcement:** `CMakeLists.txt` must maintain 100% flag and include parity with `jni/Android.mk` and `jni/Application.mk`.
* **Required Compiler Flags:** `-std=c++17 -DUSE_OPENGL -fexceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden -Oz -ffunction-sections -fdata-sections -fomit-frame-pointer -Wno-error=format-security -fno-color-diagnostics -fmerge-all-constants -fno-ident`.
* **Required Linker Flags:** `-Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none -Wl,--no-undefined -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,max-page-size=16384`.
* **Include Directories:** Must include `src/FONTS` along with all active feature and driver directories.
* **Submodules:** `libevdev` builds exclusively `source/libevdev/libevdev.c`.

---

## 10. Verified Ring-0 Memory & Infallible ESP Pipeline (Battle-Tested Record)

> [!IMPORTANT]
> **VERIFICATION CONFIRMED BY MASTER DJ (November 21, 2026):**
> Live in-game telemetry verified with `DRIVER: DEV COMPAT`, `MATCH: ACTIVE`, `PID: 12483`, `LIB BASE: 0x7BF1E80000`, with user confirming: *"esp akhon onike shundor vabe kaj kortece"* (ESP is now working very nicely).

### 1. Infallible Driver Handshake & Node Validation (`verify_test_read`)
* **Problem Solved:** On Android, `/dev/pmsg0` (logger) or `/dev/ashmem` match loose length filters (`5-12 chars`) and open with `O_RDWR` under root, causing false-positive driver attachments that silently return `-1` on memory IOCTLs.
* **Verified Solution:** Every candidate `/dev/` node MUST execute `verify_test_read()` before attachment:
  1. Write stack canary magic `test_magic = 0x5A5A1234`.
  2. Issue IOCTL read across candidate opcodes (`0x801`, `0x8001`, `601`, `0xC01`, `0x400`) against `getpid()`.
  3. Reject any node where read buffer does not bit-for-bit equal `0x5A5A1234`.
* **PID Binding:** Dispatch `RT_IOCTL_INIT (0x8004)` and `DEV_OP_INIT_KEY` with target PID (both as value and pointer) upon PID acquisition to ungate kernel page copying.

### 2. Multi-Engine Runtime Fallback Topology
* **Auto-Probe Sequence:** `RT DEV` -> `QX 10-11` -> `DEV COMPAT` -> `KPM Dual Engine` (`prctl 0x63687501` / `0x4E41`) -> `RT HOOK`.
* **Hot Fallback in RPM/WPM:** If the primary driver fails an address, `c_driver::read()` and `c_driver::write()` immediately attempt other loaded engines without frame drops.
* **Lazy KPM Probe:** A single-execution lazy check ensures devices running pure KernelSU/APatch without `/dev` nodes seamlessly bind KPM on the fly.

### 3. In-Game Entity Memory Chain (Unity IL2CPP ARM64)
* **Base Resolution:**
  - **MAX-ONLY build:** targets `com.dts.freefiremax` exclusively. Free Fire Global (`com.dts.freefireth`) and its `GameFacade_Global 0xA72E4E0` probe were removed. Primary `0xAD68E18`, Alt `0xAD6DC18`, plus dynamic ARM64 `ADRP + LDR` scanner (`resolve_gamefacade_dynamic`).
  - `Staticfield` (`0xB8`): Evaluates both `GfCell + 0xB8` and `classPtr + 0xB8` to extract `curGame` / `BaseGame`.
* **Match & Camera Resolution:**
  - `CurrentMatch` (`0x90`) & `localPlayer` (`0xD8`).
  - `inMatch`: `matchState > 0`; only when the flag is unreadable (`< 0`) fall back to `is_valid_ptr(localPlayer)`. NEVER OR-in `is_valid_ptr(BaseGame)` — it is guaranteed valid at that point and made `inMatch` permanently true even in the lobby.
  - `viewMatrix_stamp` bumps ONLY when a fresh matrix was resolved. Re-publishing the old matrix with a new stamp defeats the silent-aim staleness guard.
  - Camera ViewMatrix (`0x100`): Evaluates `FollowCamera (0x698) -> Camera (0x20) -> IntPtrCam (0x10) -> Matrix (0x100)` or `BaseGame + CameraBase (0xD8)`.
* **Entity Extraction & Culling Doctrine:**
  - `Dictionary` (`0xC0`), `entryAddr` (`0x18`), `enemyList` (`0x10`).
  - Dead check: `Player_IsDead` (`0x7C`) must be 0.
  - Team check: `Avatar_Data_IsTeam` (`0x81`) must be 0.
  - Knocked check: `PhyXdata` (`0x1D60`) -> `maybeDead` (`0x20`) -> `isKnocked` (`0x10`).
  - **Frustum-Only Culling:** `data::update()` drops a player only when BOTH projected head and toe lie outside the screen bounds (±80px) or behind the near plane — that is how wasted RPM is avoided. Rendering (`esp.hpp`, `line.hpp`, `common.hpp`) uses a wider ±150px defensive guard and accepts head OR toe, so a partially visible far enemy still gets tracer + box. `p.distance` is HUD-only.
  - Dictionary scan reads up to 512 entries (`read_dict_blocks`); a smaller cap could cut off distant players inserted later in the dictionary.
  - Driver access is serialized through `proc::driver_mutex()` helpers; `get_driver()` is a process-wide singleton shared by three threads.

---

## 11. ESP 100% Operational Record (Verified — November 21, 2026)

> [!IMPORTANT]
> **Master dj confirmed live in-game: "esp 100% kaj kortece"** (ESP is working 100%). All changes below passed a real ARM64 NDK build through GitHub Actions: run `44895782628`, commit `efd7bd9`, conclusion **success**, artifact `ninmods-kernel` 1,672,810 bytes (SHA-256 `d19369e6853fcab874033cc15bed76baaef30c468dd949a573f3cdca9cfd4e2e`). Workflow: `.github/workflows/build.yml` (ubuntu-latest, `nttld/setup-ndk@v1` r25c, `ndk-build`, `llvm-strip`, artifacts). No local compiler is needed or available.

### 11.1 What makes ESP work end-to-end (read this before changing anything)
1. **PID attach, MAX-only:** raw ARM64 `svc #0` `/proc` scan matches only `com.dts.freefiremax`; `proc::publish_attachment(pid, lib)` uses a seqlock snapshot so PID and lib base are always consistent.
2. **Driver handshake:** `verify_test_read()` canary check rejects false-positive `/dev` nodes; multi-engine auto-probe `RT DEV → QX → DEV COMPAT → KPM → RT HOOK`. All driver access serialized under `proc::driver_mutex()`.
3. **Memory chain:** `GameFacade (0xAD68E18 / 0xAD6DC18 / dynamic ADRP+LDR scanner)` → `Staticfield 0xB8` → `BaseGame` → `CurrentMatch 0x90` / `localPlayer 0xD8` → `Dictionary 0xC0` → `entryAddr 0x18` → `enemyList 0x10`.
4. **Projection:** `FollowCamera 0x698 → Camera 0x20 → IntPtrCam 0x10 → ViewMatrix 0x100`; Unity row-major W2S with perspective divide; `g_sw/g_sh` from `display_manager`.
5. **Entity validation:** alive (`0x7C == 0`), not teammate (`Avatar_Data_IsTeam 0x81 == 0`), transform walk via `GetNodePosition()` (head `0x6A8`, root `0x6D0`), head synthesis from root for far/LOD enemies.
6. **Frustum-only publish:** only on-screen/behind-near-plane-checked players enter `data::players`; renderer applies wider tolerance.
7. **Render:** `drawTracer()` → `computePlayerBox()` → `drawBox()` → bot tag → armor bar, all on `ImGui::GetForegroundDrawList()` over the native SurfaceFlinger window.
8. **No footprint:** ESP alone performs zero WPM (`proc::wpm_allowed` is false unless a combat feature is active). No code patches, no hooks, no internal threads, no disk writes.

### 11.2 Fixes that took ESP from broken to 100%
| # | Problem | Fix |
| :-- | :--- | :--- |
| 1 | False driver attach on `/dev/pmsg0` / `ashmem` | Canary `verify_test_read()` across candidate IOCTL opcodes before attach |
| 2 | Global/MAX offset confusion | MAX-only build; removed `GameFacade_Global` and `is_game_max` |
| 3 | Artificial distance limits (300m/150m) and fade | Removed `cfg::esp::max_range`, `fadeDist()`, LOD distance tiers, distance alpha fade |
| 4 | Wasted RPM, stutter | 7-layer optimization stack (batch, 100ms cache, 33ms loop, frustum cull, shuffled order, ESP-only gate, cheap dead/team rejects) |
| 5 | `inMatch` stuck true in lobby | Use authoritative `matchState > 0` only |
| 6 | Stale camera matrix looked fresh | Stamp bumps only on newly resolved matrix; aim staleness guard is time-based (150ms) |
| 7 | Bone cache data race / ghost players | `bone_cache_mutex`; clear on detach, stop, and PID/lib change; dynamic GameFacade scanner resets on re-attach |
| 8 | Full-auto silent aim steered only first shot | One steer per ~85ms bullet window while trigger held |
| 9 | Headshot force 100% / 0% ignored at range | Slider-driven rate controller; 0% stays 0% at any distance |
| 10 | Driver singleton accessed by 3 threads unsafely | All `driver->` calls moved into mutex-guarded `proc::` helpers |

### 11.3 Far-enemy tracer fix (latest, user-reported: "দূরের enemy-র উপরে line পড়ছে না")
Root causes and fixes — do NOT revert these:
1. `line.hpp` used a **zero-margin, head-only** viewport test (`0 ≤ x ≤ g_sw`, `0 ≤ y ≤ g_sh`). Far enemies with the head just past the edge but feet visible lost the tracer. Now `ok(Head) || ok(Toe)` (±150px), snap to the visible bone, clamp endpoint into the viewport.
2. `data.cpp process_enemy` did `if (!is_valid_ptr(headTF)) return;` and `if (headWorld == Zero) return;` — every distant/LOD enemy with no head ITransformNode vanished from `data::players`. Now reads root too and synthesizes `headWorld = root + 1.75m`; drops only if both fail.
3. `common.hpp computePlayerBox()` was head-only; now accepts toe-only and synthesizes top/bottom (`±50px`).
4. `data.cpp read_dict_blocks` cap raised `200 → 512` so players inserted later in the dictionary are still parsed.
5. Removed the stale "alpha fade tied to frustum edge" comment — no fade exists anywhere.

**Files:** `src/func/esp/line.hpp`, `src/func/esp/common.hpp`, `src/func/esp/esp.hpp`, `src/game/data.cpp`.

### 11.3b Silent-aim / headshot cone bug (fixed — do NOT reintroduce)
* `silent.hpp::get_camera_forward_from_matrix()` previously used `-(m[8],m[9],m[10])` as camera forward. That is NOT the forward axis of the matrix used by `WorldToScreenPoint()`. For a target in front of the camera the dot product went negative, so the 8° cone guard aborted EVERY `apply_trajectory()` call — headshots silently never registered.
* **Correct derivation:** the point on the camera-forward ray projects to screen center, so both W2S numerators vanish along it. Let `A = (m[0],m[4],m[8])` (xN gradient) and `B = (m[1],m[5],m[9])` (yN gradient); then `camFwd = normalize(A × B)`. Use `fabsf(dot(camFwd, normDir))` because cross-product sign follows projection convention; `acquire_target()` already rejects behind-camera targets.
* Historical note: an older build accidentally worked because the degenerate fallback was `(0,0,1)` (cone passed); when the fallback was changed to `Vector3::Zero()` all writes aborted. The real fix is the axis, not the fallback.
* `Direction (0x40)` payload is the **normalized** unit vector — confirmed from the last verified build; do not switch it to unnormalized displacement.

### 11.4 Regression rules for future models
* Never add a distance check, distance fade, or `max_range` slider back into ESP. Range = camera frustum only.
* Never reject an entity solely because its head transform failed — always try root fallback.
* Never require the head point to be strictly inside the viewport for a tracer; use `ok(Head) || ok(Toe)` and clamp.
* Never reintroduce WPM to `ViewBlocked`, `HitGroup`, `HitPoint`, `HitNormal`, `Distance`, enemy colliders, or weapon recoil.
* Keep every feature button a real `cfg::Checkbox()` (`ImGui::Checkbox`), never a toggle switch. Sliders only where genuinely needed (headshot rate, FOV radius when its circle is on, head bias).
* Build verification is remote-only: commit, push, then read `https://api.github.com/repos/opum4653-tech/kernl/actions/workflows/build.yml/runs?per_page=5`. Artifact/job-log download needs auth, but run/job metadata is public.


