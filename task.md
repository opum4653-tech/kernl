# NINMODS KERNEL — Comprehensive Anti-Ban & Stealth Architecture Roadmap

---

## 🎯 Executive Summary & Forensic Autopsy: Why Accounts Get Banned with Kernel Drivers

A Kernel Driver operates at **Ring-0**. It successfully hides the cheat process from userspace scanners (`ps`, `/proc/<pid>/maps`, `/proc/<pid>/status`, and `ptrace`). 

**However, a Kernel Driver CANNOT prevent:**
1. **Server-Side Anomaly & Telemetry Bans:** The game server runs physics replication. If the bullet trajectory deviates 40° from camera forward, or `HitGroup` is forced to Head (1) when crosshair is pointing elsewhere, the server flags automated aimbot detection.
2. **The `ViewBlocked = false` Magic Bullet Trap:** Overwriting `ViewBlocked` allows bullets to hit through walls. Server physics detects raycast intersection with static obstacles -> **Instant 10-year ban**.
3. **In-Engine Heap Watchdogs:** Anti-cheat threads inside the game (`libanort.so`, `libgcloud.so`) monitor their own weapon structs (`AimInfo`) for unexpected asynchronous memory modifications.
4. **Statistical Heuristics (100% Headshot Rate):** Unnatural kill-to-headshot ratios trigger automated machine learning flags on the backend.
5. **Lack of Hardware Touch Aim Alternative:** Silent aim modifies game memory. A true undetectable aimbot operates externally via touch event simulation (physically moving the camera view without writing a single byte to game memory).

---

## 📋 Master Task Breakdown

### Phase 1: Game Server & Telemetry Anti-Ban Hardening (Silent Aim / Trajectory)
- [x] **Task 1.1: Eliminate the `ViewBlocked = false` Wall Penetration Ban Trap (`src/func/aim/silent.hpp`)**
  - **Forensic Cause:** `wpm<bool>(hitObjAddress + Offsets::Aim::ViewBlocked, false)` permits shooting through obstacles. When the server validates trajectory through cover, it triggers an instant server-side ban.
  - **Fix:** `ViewBlocked` write completely removed. Only `Offsets::Aim::Direction` (0x40) is written — the physics engine validates raycast naturally.
- [x] **Task 1.2: Angular Deviation Clamping (Camera Cone Guard) (`src/func/aim/silent.hpp`)**
  - **Forensic Cause:** Firing at targets far outside the crosshair creates a sharp angle between camera forward and bullet direction, easily flagged by server-side dot-product telemetry.
  - **Fix:** Camera forward extracted live from published `data::viewMatrix_pub` row 2 (`-(m31, m32, m33)`). Zero extra RPM overhead. `dot(CameraFwd, BulletDir) < cos(cone)` → write aborted, bullet fires naturally. Constant tightened to `MAX_AIM_COS = 0.99027f` (8°) in Phase 2.5.4.
- [x] **Task 1.3: Eliminate Hardcoded HitGroup Forcing (`src/func/aim/silent.hpp`)**
  - **Forensic Cause:** Overwriting `Offsets::Aim::HitGroup = 1` directly inside the game buffer desynchronizes client physics collider hits with reported hitgroup, causing mismatch telemetry flags.
  - **Fix:** `HitGroup` write completely removed. The steered Direction vector causes the physics engine to register the correct hitgroup from the actual raycast intersection.
- [x] **Task 1.4: Humanized Micro-Drift & Non-Static Trajectory Jitter (`src/func/aim/silent.hpp`)**
  - **Forensic Cause:** Mathematical vectors pointing to exact center of bone nodes with zero variance produce artificial telemetry signatures.
  - **Fix:** Box-Muller Gaussian jitter applied to `normDir` *after* clean cone validation using `thread_local` seed. Sigma scales dynamically with range (`0.012m` at 10m → `0.045m` at 100m). Vertical axis is tightened (`0.5x` sigma). Full 3-axis `std::isfinite` vector guard prevents degenerate inputs.

---

### Phase 2: In-Engine Memory Quarantine & Zero-WPM Safety Lock
- [x] **Task 2.1: Enforce Strict Zero-WPM Hardware Lock for Pure ESP (`src/other/memory.hpp`, `src/globals.hpp`)**
  - **Forensic Cause:** Users running Pure ESP must have mathematical 100% certainty that zero bytes are ever written into the game process under any condition.
  - **Fix:** Enforced an immutable acquire-barrier gate in `mem_write()`: if combat/aim features are inactive (`proc::wpm_allowed == false`), `mem_write()` immediately aborts with zero kernel driver interaction.
- [x] **Task 2.2: Memory Write Address Quarantine Barrier (`src/other/memory.hpp`)**
  - **Forensic Cause:** Rogue writes into executable code, metadata tables, or entity structures trigger in-engine checksum crashes and integrity bans.
  - **Fix:** Strictly constrained `wpm` destination addresses. Hardened 4-stage barrier: (1) Valid user-space pointer bounds `[0x10000 .. 0x7FFFFFFFFFFF]`, (2) Max permitted payload size locked to 12 bytes (`sizeof(Vector3)`), (3) Strict 4-byte memory alignment (`a & 3 == 0`), (4) Code page quarantine blocking any write to `[libBase .. libBase + 0x15000000]`.
- [x] **Task 2.3: Cadence & Flood Rate Limiter (`src/func/aim/aim.hpp`)**
  - **Forensic Cause:** Over-flooding memory with rapid WPM cycles (500Hz) creates memory bus contention and can be captured by game watchdog sampling loops.
  - **Fix:** Synchronized writes strictly with active weapon discharge cadence (single write per shot cycle), eliminating high-frequency writer threads.

---

### Phase 2.5: Silent Aim Telemetry Humanization (Second Hardening Pass)
- [x] **Task 2.5.1: Jitter on ALL Shots (`src/func/aim/silent.hpp`)**
  - **Forensic Cause:** Jitter was applied only to body shots (`!targetIsHead`). A 100% headshot config wrote byte-identical perfect vectors every shot — a statistical signature.
  - **Fix:** Jitter now applies to headshots too with a tighter sigma (head ~0.09°-0.17°, body ~0.29°-0.57°). Per-shot seed refresh via `fresh_seed()` guarantees consecutive vectors differ. Jittered vector is re-validated against the cone before the write.
- [x] **Task 2.5.2: Probabilistic Per-Shot Steering Skip (`src/func/aim/aim.hpp`)**
  - **Forensic Cause:** Every trigger edge was steered → full-auto burst had a 100% hit ratio, unnatural for a human.
  - **Fix:** ~12% of trigger edges are deliberately left unsteered (thread-local PRNG), so the hit pattern includes natural misses.
- [x] **Task 2.5.3: Fire-Cadence Timing Variation (`src/func/aim/aim.hpp`)**
  - **Forensic Cause:** Fixed 16.000ms aim-loop interval is a machine-periodic signature.
  - **Fix:** ±3ms thread-local jitter added to the loop sleep, breaking exact periodicity.
- [x] **Task 2.5.4: Tighter Cone + Stale-Camera Guard (`src/func/aim/silent.hpp`, `src/game/data.cpp`)**
  - **Forensic Cause:** 12° cone was loose; viewMatrix could be stale during fast flicks, letting an old orientation pass the guard.
  - **Fix:** Cone tightened to 8° (`MAX_AIM_COS = 0.99027f`). Added `data::viewMatrix_stamp` atomic bumped on every matrix publish; steering aborts after 3 consecutive stale stamps.

### Phase 2.6: Rate-Controlled Headshot (Auto Headshot redefined)
- [x] **Task 2.6.1: Auto Headshot = Rate Controller, not 100% force (`src/func/aim/headshot.hpp`, `silent.hpp`)**
  - **Forensic Cause:** `auto_headshot = true` hard-forced `targetIsHead = true` → 100% headshot pattern, a top ML ban signal.
  - **Fix:** New `evaluate_headshot_rate(target, isScoping)` returns effective % [0..100]:
    - Auto Headshot ON enables the controller using `headshot_rate` slider value (no 100% force).
    - Scope Aim adds a 25% precision uplift, capped at 95%.
    - Distance falloff beyond 40m: rate decays linearly to 60% at 140m (natural weapon behavior).
    - `apply_trajectory()` now takes `float headRate` and rolls 0..1000 for sub-percent granularity.
    - Rate 0 → body/chest shot (never skips fire). Auto OFF → body only.
  - **Menu:** checkbox relabeled "Auto Headshot (Rate Control)", slider "Base Rate", live status text shows OFF/RISK/SAFE.

---

### Phase 3: Hardware Touch Simulation Engine (100% External Non-Invasive Aimbot)
- [ ] **Task 3.1: External Touch-Based Aimbot Architecture (`includes/draw/Android_touch/Touch.cpp`, `src/func/aim/`)**
  - **Forensic Cause:** Memory-based aimbot modifies the game process. External touch aimbot physically turns the camera via `/dev/input/` or `uinput`, achieving 100% Zero-WPM stealth.
  - **Action Plan:** Integrate a smooth touch drag calculator that injects delta screen swipes onto the aim/drag area to track targets without touching game memory.
- [ ] **Task 3.2: Humanized PID / Bezier Smoothing Curves (`src/func/aim/`)**
  - **Forensic Cause:** Linear camera movement looks robotic and flags behavioral detection.
  - **Action Plan:** Implement cubic Bezier / PID acceleration-deceleration curves with natural overshoot and recovery.

---

### Phase 4: Kernel Driver Stealth & Ring-0 Sanitization
- [x] **Task 4.1: Driver Fallback Lockdown & Strict Validation (`includes/driver.h`, `src/other/memory.hpp`)**
  - **Forensic Cause:** If driver fails to attach, mod silently failing or operating with incorrect device nodes can expose system footprints.
  - **Fix:** Implemented Ring-0 multi-engine dynamic auto-probing (`auto_probe_all()`): sequentially verifies KPM (TB prctl 0x63687501 / Neo prctl 0x4E41) -> QX 10-11 -> RT DEV -> DEV COMPAT -> RT HOOK. Self-heals in daemon mode without blind fallback.
- [x] **Task 4.2: Fileless Driver Configuration & Memory Scrubbing (`includes/driver/driver_config.h`)**
  - **Forensic Cause:** Writing `.nin_driver.cfg` to `/data/local/tmp` leaves forensic disk artifacts visible to root-checking apps.
  - **Fix:** Switched to 100% fileless configuration. Removed disk persistence; legacy config files in `/data/local/tmp/` and working directory are securely scrubbed via `unlink()` on startup. State lives strictly in memory and environment variables (`NIN_DRIVER_OPTION`).

---

### Phase 5: Overlay & Input Subsystem Camouflage
- [ ] **Task 5.1: Passive Touch Interception (Zero-Grab Option) (`includes/draw/Android_touch/Touch.cpp`)**
  - **Forensic Cause:** Calling `EVIOCGRAB` exclusively acquires the touchscreen device node. Security tools checking device grab states can identify external grabbers.
  - **Action Plan:** Support a non-grabbing passive touch mode with seamless menu interaction.
- [ ] **Task 5.2: SurfaceFlinger Window Identity & Screenshot Protection (`includes/draw/Android_draw/draw.cpp`)**
  - **Forensic Cause:** Screen recorders or in-game capture APIs taking screenshots could capture ESP overlays if secure flags are absent.
  - **Action Plan:** Enforce `ISurfaceComposer::eSecure` flags on the native surface window so ESP overlay is completely invisible to game screencaps.
