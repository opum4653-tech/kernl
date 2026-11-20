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
* Dual-tier validation: Always evaluate primary `0x6A8` first; if it produces `(0,0,0)`, evaluate `0x6A0`.

### 3. Full-Body Chaturbhuj Box Proportions
* **Top:** Head bone screen position minus 12% helmet padding:
  $$y_{\text{top}} = y_{\text{head}} - (\text{boxHeight} \times 0.12f)$$
* **Bottom:** Toe bone screen position plus 5% boots padding:
  $$y_{\text{bottom}} = y_{\text{toe}} + (\text{boxHeight} \times 0.05f)$$
* **Humanoid Width Ratio:**
  $$\text{boxWidth} = (\text{bottom} - \text{top}) \times 0.48f$$
* Center box horizontally on the midpoint of head and toe screen coordinates.

### 4. Head Laser Tracer
* Trace from screen center-top or crosshair directly to the skull center coordinate ($y_{\text{head}} + 0.12m$).

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

### 3. Memory Flooding Pipeline (High-Speed Zero-RPM Writer Thread)
When active trigger is detected (`isFiring > 0` or scoping trigger):
1. Write unnormalized displacement vector $\vec{D}$ to `AimInfo + Offsets::Aim::Direction` (`0x40`).
2. Write scalar distance $dist$ to `AimInfo + Offsets::Aim::Distance` (`0x5C`).
3. Write critical damage group `1` to `AimInfo + Offsets::Aim::HitGroup` (`0x64`).
4. Write target head position to `AimInfo + Offsets::Aim::HitPoint` (`0x28`).
5. Write normalized direction to `AimInfo + Offsets::Aim::HitNormal` (`0x34`).
6. **Dual Collider Override (Anti-Torso Fallback):** Overwrite target enemy's `LockedAimingCollider` slots (`0x80` & `0x78`) with target's `HeadCollider` (`0x740`).
7. **Weapon Recoil Stabilization:** Zero out active weapon recoil (`WeaponOnHand + 0x80 + 0x18 = 0.0f`).

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

* **Strictly Single Master Switch:** The global Master Switch (`cfg::master_enabled`) belongs strictly and exclusively on Tab 2 (System Controls).
* **Zero Duplication on Feature Tabs:** Never place a Master Switch or redundant toggle on Tab 0 (ESP Visuals) or Tab 1 (Aimbot Engine). Feature tabs must contain only their dedicated feature checkboxes and configuration sliders.
* **Global Control:** When toggled on Tab 2, `cfg::master_enabled` governs the entire engine across all features.

