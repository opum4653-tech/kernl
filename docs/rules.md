# NINMODS KERNEL — Operational & Engineering Rules

This document establishes the mandatory design, architecture, and coding rules for the `ninmods-kernel` native C++ codebase. Every developer and AI agent working on this project must strictly comply with these rules.

---

## 1. Display Resolution & Projection Invariants

1. **Dynamic Resolution Synchronization:**
   - Never assume a fixed screen resolution (e.g. 1920x1080). Modern Android gaming phones have wide aspects (2400x1080, 2340x1080, 2712x1220, 3200x1440).
   - Global projection variables `g_sw` and `g_sh` must strictly reflect the active display viewport:
     - Initialized from `displayInfo` in `main.cpp` (`start_overlay`).
     - Synchronized dynamically in `drawBegin()` whenever orientation changes.
     - Kept in sync with `ImGui::GetIO().DisplaySize` in `ui::menu::render()`.
   - Failing to sync `g_sw` causes the WorldToScreen matrix projection to drift, rendering ESP boxes and tracer lines in empty space off the enemy's body.

2. **ESP Tracer Line Snapping Doctrine:**
   - Tracer lines must snap to logical body anchors depending on their origin:
     - **Top Origin (`tracer_pos == 0`):** Snaps to the top-center (`midX, topY`) of the enemy bounding box (Head).
     - **Center Origin (`tracer_pos == 1`):** Snaps to the chest/pelvis center (`midX, (topY + botY) * 0.5f`) of the enemy.
     - **Bottom Origin (`tracer_pos == 2`):** Snaps to the feet/bottom-center (`midX, botY`) of the enemy.
   - Drawing a bottom tracer line all the way to the head point slices through the player's model and causes awkward visual overlap.

---

## 2. Floating UI & Touch Target Invariants

1. **Floating Logo Visibility & Sizing:**
   - The floating circular 'N' logo must remain prominent and effortlessly touchable on high-DPI screens.
   - Logo base radius must be maintained at **`38.0f * ui_scale`** (diameter ~76px).
   - Never shrink the logo below `35.0f * ui_scale` on modern high-resolution displays.
   - The logo incorporates:
     - Dark disc base (`IM_COL32(18, 14, 26, 245)`)
     - Electric Orchid outer border (`theme_orchid::ColPrimary`)
     - Bold "N" letter sized proportionally (`fontSize = radius * 1.15f`)
     - Status dot showing kernel daemon connectivity (Green = Connected, Red = Disconnected)

2. **Touch Penetration Barrier:**
   - Every live UI panel appends its own rect each frame:
     `Touch_AddUiRect(x, y, w, h)`. There is no single-rect replacement API.
   - The render loop wraps the frame in `Touch_ClearUiRects()` … `Touch_CommitUiRects()`.
   - A gesture that begins inside a rect stays swallowed until the finger lifts.
   - Touches within the logo or menu bounds must never penetrate into the underlying game canvas.

3. **Menu Dimensions & Aspect Ratio:**
   - Sidebar width and header height must maintain a strict 1:1 aspect ratio (`headerH = sidebarW`) for visual symmetry.
   - Total menu window size must account for both sidebar and content areas dynamically scaled via `ui_scale`.

4. **Draw-List Layer Order (menu is always on top):**
   - The menu window and the floating logo draw with `ImGui::GetWindowDrawList()`.
   - Every world-space overlay — ESP boxes, tracers, health bars, name pills, loot pills, crosshair, FOV ring, target lock marker — must draw on `ImGui::GetBackgroundDrawList()`.
   - `ImGui::Render()` emits **background → all windows → foreground**. Using `GetForegroundDrawList()` for a world overlay paints it over the menu's glass cards and over the logo disc. Never do it.
   - Both draw lists render into the same overlay surface. The game canvas is a separate surface underneath, so switching to the background list does not hide anything behind the game — it only decides ordering against ImGui windows.

---

## 3. Kernel Memory & Safety Standards

1. **Ring 0 Kernel-Space Memory Operations:**
   - Memory reading must strictly utilize the kernel driver channel (`driver->read(...)` via IOCTL).
   - Userspace fallbacks (`process_vm_readv` or `/proc/pid/mem`) are strictly prohibited in production builds to prevent detection flags.

2. **Zero Persistent Storage Trace:**
   - The engine must never write temporary configuration, log, or position files to `/data/local/tmp` or `/sdcard`.
   - Logo positions and runtime configurations are preserved strictly in RAM.

3. **Performance & Syscall Minimization:**
   - Utilize batched memory structures (`PlayerSpan`) to extract all player entity fields in a single read rather than making isolated syscalls per field.
   - Dictionary slots must be extracted in blocks of 64 (`MonoDictionary::read_block`).

---

## 4. Pure Kernel Silent Aim & Trajectory Integrity

1. **Zero Entity & Internal HitInfo Tampering (Kick / Desync Prevention):**
   - **NEVER** write to enemy entity memory (e.g., `LockedAimingCollider` at `0x80`).
   - **NEVER** write to internal weapon hit fields on `AimInfo`:
     - `HitCollider` (`0x20`)
     - `HitLocation` (`0x28`)
     - `HitNormal` (`0x34`)
     - `HitGroup` (`0x64`)
     - `IgnoreHappens` (`0x70`)
     - `SpecialHitType` (`0x80`)
   - Writing to these internal hit fields from out-of-process corrupts Unity's physics raycast state and triggers server-side packet validation failures, resulting in the server disconnection error:
     `"Match results is still being calculated and will be available in match history after you return to the lobby."`

2. **Pure Raycast Direction Vector Steering (0x40 Only):**
   - Trajectory steering must write **strictly and exclusively** to `AimInfo + Offsets::Aim::Direction` (`0x40`) with a normalized unit vector (`diff.normalized()`), and reset `AimInfo + 0x71` (`ViewBlocked` / adjustment flag).
   - Unity's native weapon physics engine performs the raycast naturally along this direction vector, intersecting the enemy's head collider and generating an authentic, 100% legitimate server-validated damage packet.

3. **Hitbox Vertical Elevation Bias (+0.20m to +0.24m):**
   - Free Fire's `Bone_Head` (`0x6A8`) pivot is located at the neck / jaw base.
   - The enemy's capsule body collider extends vertically above this pivot. Firing directly at `headWorld` (+0.0m) causes the ray to hit the upper body capsule first, producing body hits (yellow/white numbers).
   - Adding a vertical elevation bias (`+0.22m` default via `cfg::aim::head_bias`) elevates the ray directly into the center of the head hitbox sphere, guaranteeing 100% RED headshot numbers.

4. **Strict Menu Toggle Gating & Background Thread Sanitation:**
   - Silent Aim and Auto Headshot execution must be completely isolated to when their respective menu toggles (`cfg::aim::silent`, `cfg::aim::auto_headshot`) are enabled.
   - When toggles are disabled or when target is lost, immediately invoke `clear_silent_data()` to set `s_hasSilentData = false` and nullify `s_silentAimInfo`.
   - The dedicated writer thread `writer_loop()` must verify `cfg::aim::silent` and `cfg::aim::auto_headshot` on every cycle. If false, it must immediately flush shared data and sleep, completely preventing any background memory writes when the feature is turned off.

---

## 5. Absolute Prohibition of Speed Hack & Movement Manipulation (Supreme Anti-Ban Law)

1. **Permanent Invariant: Zero Speed Hack Policy (কোনো প্রকার স্পিড হ্যাক চিরতরে নিষিদ্ধ):**
   - **NO speed hack, velocity modifier, sprint multiplier, high jump, fast fall, or movement manipulation of ANY KIND is permitted inside this project.**
   - All forms of movement modification (including `RunSpeedUpScale` at `0x2C0`, `Player_Speed` at `0x560`, `FallingSpeedUpScale` at `0x2BC`, or time-scale acceleration) are strictly, completely, and permanently banned from this repository.
   - The game's character controller and physics movement must remain **100% official, natural, and untouched (Stock Model)** at all times.

2. **Zero Persistent Memory Polling / Reading for Movement:**
   - No thread, render loop, or watchdog is ever permitted to poll or read player speed/attributes in real time. Continuous reading and writing of velocity values creates unnecessary IOCTL driver traffic and triggers server-side displacement heuristics.

3. **Master Override Reminder Protocol (পূর্বনির্ধারিত সতর্কতা প্রোটোকল):**
   - **MANDATORY DIRECTIVE FOR ALL AI AGENTS & DEVELOPERS:**
     If Master dj (or any user or agent) ever instructs, requests, proposes, or asks to add, implement, restore, or test any Speed Hack, Movement Modifier, or Run Speed feature in the future, the agent/developer **MUST IMMEDIATELY REFUSE AND REMIND MASTER**:
     > *"Master, আপনি নিজেই কঠোরভাবে নির্দেশ দিয়ে চিরতরে মানা করে দিয়েছেন যে এই কার্নেল প্রজেক্টের ভেতরে কোনো প্রকার স্পিড হ্যাক বসানো যাবে না। স্পিড হ্যাক সবসময় সিস্টেম ও একাউন্টের জন্য অত্যন্ত ক্ষতিকর এবং সার্ভার-সাইড ফিজিক্স ডিটেকশন ট্রিগার করে। `docs/rules.md` অনুযায়ী এটি সম্পূর্ণ নিষিদ্ধ।"*

---

## 6. 100% Ring-0 Memory & Pure ESP Stealth Architecture (Undetectable Standards)

1. **Strict 100% Ring-0 Driver Invariant (Zero Userspace Syscall Fallbacks):**
   - All memory reading (`mem_read` / `rpm`) and memory writing (`mem_write` / `wpm`) must strictly go through Ring-0 kernel drivers (`driver->read`, `driver->write`).
   - Direct ARM64 assembly syscall fallbacks (`process_vm_readv` = 270 and `process_vm_writev` = 271) are **permanently purged** from the codebase.
   - If a kernel driver is not loaded or encounters an unmapped page, the engine returns `false` without ever generating seccomp/auditd syscall traces.
   - In `c_kpm_driver`, silent fallbacks to PVM syscall mode are permanently banned. Only authentic TB KPM (`PRCTL_MEM_READ`) and Neo KPM (`NA_PRCTL_MAGIC`) are permitted.

2. **Physical Zero-WPM Gate (Strict Hardware-like Write Protection):**
   - In Pure ESP Mode (when Combat / Aim features are toggled off), writing to target memory is physically blocked at the engine layer via `proc::wpm_allowed`.
   - `mem_write` strictly checks `proc::wpm_allowed` and early-returns `false` before dispatching to the kernel driver. Target process memory remains 100% read-only.

3. **In-Process Kernel Discovery (Zero Shell Subprocess Footprint):**
   - Spawning `/system/bin/sh`, `popen()`, or `system()` to discover modules, major numbers, or device paths is strictly prohibited.
   - Target PID discovery, `/proc/<pid>/maps` parsing, and device node resolution must run 100% in-process via POSIX directory traversal and `stat()` / `major(st.st_rdev)`.

4. **SurfaceFlinger Invisibility & Anti-Screenshot Protection:**
   - The NativeSurface overlay must always register with `eSecure (0x80)` and `eSkipScreenshot (0x4000)` flags.
   - SurfaceFlinger compositing automatically omits the overlay layer from `screencap`, screen recorders, and game in-process screenshot grabs.
   - Layer naming must avoid cheat signatures (`"AImGui"`) and use disguised system layer identifiers (`"SurfaceView[ScreenDecor]"`).

5. **Zero Leaked Cheat Signatures:**
   - Device node scanners (`scan_driver_path`) must dynamically inspect character device attributes without maintaining hardcoded blacklisted cheat strings (`"wanbai"`, `"CheckMe"`, etc.) that trigger static YARA scanners.

6. **Zero Logcat Footprint:**
   - `__android_log_print` calls using detectable tags (`"ImGui"`, `"Overlay"`) are permanently banned across all compilation units to eliminate logcat telemetry detection.

7. **Authentic Touch Hardware Cloning:**
   - Touchscreen virtual injection (`Touch.cpp`) must clone the real physical digitizer name via `EVIOCGNAME`, bus ID (`BUS_I2C = 0x18`), and physical sysfs path (`EVIOCGPHYS`). Generating synthetic random strings in `/proc/bus/input/devices` is prohibited.

8. **Zero Shell Footprint in Display Pipeline:**
   - `display_safe` must resolve screen dimensions and orientation 100% in-process via sysfs (`/sys/class/graphics/fb0/virtual_size`), `/dev/graphics/fb0` ioctls, and Android Bionic system properties. Spawning `popen("dumpsys input...")` or `popen("wm size...")` is permanently prohibited.

9. **Universal Anti-Cheat Quarantine & Code-Page Protection:**
   - `mem_write` must physically reject any write attempt targeting executable code segments (`.text`, `.rodata`) or loaded module spaces (including `libil2cpp.so` or any security module regardless of its file name).
   - Trajectory writes are exclusively permitted on validated dynamic heap weapon buffers (`AimInfo + 0x40`).

10. **Universal Process Shielding (Air-Gap Protection):**
    - The engine must set `prctl(PR_SET_DUMPABLE, 0)` at process initialization to forbid all userland ptrace attachments and `/proc/self/mem` inspection from any security thread or sandboxed app.
    - Process cmdline memory (`argv[0]`) and thread comm must remain permanently camouflaged as `"kworker/u16:1"`.
