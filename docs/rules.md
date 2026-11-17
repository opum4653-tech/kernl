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

## 5. Anti-Ban Safe Movement & Speed Hack Architecture

1. **Sprint Scale Clamping (1.00x - 1.30x Safe Range):**
   - Speed modifications must strictly target `RunSpeedUpScale` (`0x2C0`) on `PlayerAttributes`. This mirrors in-game character sprint skills (Kelly, Alok, Joseph).
   - **NEVER** modify `Player_Speed` (`0x560`) or multiply `Player_Speed` and `RunSpeedUpScale` simultaneously, which causes exponential velocity explosions (`mult * mult = 25x`) and instant server teleport bans.
   - All speed multipliers must be clamped strictly to `1.00x - 1.30x` (maximum 30% sprint boost), keeping velocity safely below the server's 10.5 m/s anti-teleport threshold.

2. **Zero Gravity / Vertical Tampering:**
   - **NEVER** write to `FallingSpeedUpScale` (`0x2BC`). Modifying fall velocity alters gravity physics and immediately triggers server-side vertical teleport detection.

3. **Smart Firing Guard:**
   - When actively firing (`isFiring == true`), the sprint multiplier automatically drops to baseline `1.0x`. This prevents weapon desync, dropped bullets, and rate-of-fire validation anomalies during combat.
