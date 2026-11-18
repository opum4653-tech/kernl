---
name: mod-any-game
description: >-
  End-to-end game modding, reverse engineering, and cheat engine development guide
  for Mobile (Android ARM64/ARMv7, Unity IL2CPP, Unreal Engine, Native C++) and PC games.
  Covers engine recon, route selection (External Kernel Driver, Zygisk Injection, Native Hooks),
  centralized offset architectures, ESP rendering, silent aim trajectory mathematics,
  diagnostic telemetry, and automated GitHub Actions remote compilation.
---

# Universal Game Modding & Reverse Engineering Architecture

This skill defines the complete, battle-tested methodology for modding, reverse engineering,
and building high-performance memory engines for games (Mobile Android Unity/IL2CPP, Unreal Engine,
Native C/C++, and PC targets).

---

## 1. Core Modding Routes & Architecture Selection

Select the lowest-overhead, highest-stealth route that accomplishes the technical objective:

| Route | Execution Model | Memory / Hooking Footprint | Best For |
| :--- | :--- | :--- | :--- |
| **External Kernel Driver (Default)** | Out-of-process daemon (`kworker/u16:1`) + Kernel Driver (`/dev/*`) + ImGui Native Surface Overlay | **Zero memory patch** in game code (`.text`). Only reads state (`rpm`) and writes aiming/input structures (`wpm`) during active triggers. | Competitive online Android games (Free Fire, PUBG, CODM), strict anti-cheat environments. |
| **In-Process Zygisk / Dobby Hooking** | In-process shared object (`.so`) injected at zygote fork (`ZygiskApi`) | Inline function hooks (`DobbyHook`, `And64InlineHook`), method redirection, symbol export via `xdl`. | Calling internal game methods, bypasses requiring native stack context, offline/lab mods. |
| **Native Memory Patcher** | Process memory patcher (`mprotect` + byte rewrite) | Modifies executable bytes (e.g., `NOP` sleds, `RET` overrides, float constant overrides). | Recoil zeroing, weapon spread mutes, offline logic bypassing. |
| **Engine Data & Script Modding** | File asset replacement (AssetStudio, FModel, UABEA) | Asset pak overrides, Mono script injection, data table mods. | Custom models, textures, animations, PC mod loaders (tModLoader, BepInEx, UE4SS). |

---

## 2. Supreme Architectural Laws (Strict Enforcement)

Every modding project must follow these strict engineering disciplines:

### Rule 1: Absolute Offset Centralization (`offsets.hpp`)
* **Zero Hardcoded Offsets:** Never hardcode memory offsets, class offsets, bone indices, or enum constants in `.cpp`, `.hpp`, or feature classes.
* **Single Source of Truth:** All offsets must be isolated inside `src/offsets/offsets.hpp` organized into namespaces:
  ```cpp
  namespace Offsets {
      namespace Base { ... }
      namespace Player { ... }
      namespace Bone { ... }
      namespace Aim { ... }
      namespace ESP { ... }
  }
  ```
* **Game Update Modularity:** When the game receives an update (e.g., OB55 → OB56), **only** `src/offsets/offsets.hpp` is modified. The ESP math, bounding box algorithms, silent aim loops, and driver routines remain 100% untouched.

### Rule 2: Remote-Build Axiom (Zero Local Compilers)
* The local host is strictly an authoring, structuring, and staging environment.
* Never invoke local compilers (`cl`, `gcc`, `clang`, `rustc`, `make`, `msbuild`) or local build runners.
* All builds, tests, and release binaries are strictly compiled remotely via **GitHub Actions CI/CD workflows** on Ubuntu runners using the Android NDK.

---

## 3. Reverse Engineering & Reconnaissance Workflow

### Step 1: Binary & Metadata Extraction
1. Extract the target APK / binary package.
2. Inspect architecture (`arm64-v8a`, `armeabi-v7a`, `x86_64`).
3. Run `Il2CppDumper` on `libil2cpp.so` + `global-metadata.dat`:
   * Generates `dump.cs`, `script.json`, and symbol maps.
   * Search for core game managers (e.g., `GameFacade`, `BaseGame`, `Match`, `World`, `Player`).

### Step 2: Traversing the Base Pointer Chain
Map the root pointer chain from a static class field to the active entity dictionary:
```text
Static Class Base Address
  └── StaticFields (+0xB8)
        └── BaseGame Instance (+0x8)
              └── Current Match (+0x90)
                    ├── LocalPlayer (+0xD8)
                    └── PlayerDictionary (+0xC0)
                          ├── Count (+0x20)
                          └── Entries Array (+0x18 -> +0x20 header -> stride 0x18)
```

---

## 4. Feature Implementation Playbooks

### Playbook A: ESP Visuals & 3D Skeletal Bone Tracking

1. **Camera & Projection Matrix:**
   * Read camera pointer (`LocalPlayer + FollowCamera -> Camera -> IntPtrCam`).
   * Extract ViewMatrix (`IntPtrCam + 0x100`).
   * Implement `WorldToScreenPoint` with viewport clipping and perspective divide:
     $$\text{inv} = \frac{1.0f}{v_{12}}, \quad \text{screenX} = cx + cx \cdot v_9 \cdot \text{inv}, \quad \text{screenY} = cy - cy \cdot v_{10} \cdot \text{inv}$$

2. **3D Bone Resolution (`GetNodePosition`):**
   * Traverse Unity transform hierarchy (`ITransformNode`):
     * Read `Transform = rpm<uint64_t>(nodeTransform + 0x10)`.
     * If null or zero, fallback to direct `Transform` evaluation.
     * Resolve `MatrixPtr` (`TransformObj + 0x38`) and indices list (`0x20`), multiplying hierarchical parent rotation quaternions and scales.
   * Always provide a fallback ladder:
     * Primary Head Bone (e.g., `0x6A8`).
     * Secondary / Auxiliary Bone (e.g., `0x6A0`).
     * Cranium center vertical elevation bias (`+0.12m`).

3. **2D Bounding Box (Chaturbhuj Box):**
   * Calculate top from Head screen position and bottom from Toe screen position.
   * Maintain humanoid proportional width ratio:
     $$\text{boxWidth} = \text{height} \times 0.48f$$
   * Add 12% helmet/head padding and 5% foot padding.

---

### Playbook B: Trajectory Assist & Silent Aim Engine

Silent Aim alters bullet trajectory vectors directly in weapon fire memory packets without moving the player's camera:

1. **Displacement Vector Calculation:**
   $$\vec{D} = \vec{P}_{\text{targetHead}} - \vec{P}_{\text{fireStart}}$$
   * Apply cranial elevation bias: $\vec{P}_{\text{targetHead}}.y += (\text{SkullElevation} + \text{head\_bias})$.
   * Compute scalar Euclidean distance: $dist = \|\vec{D}\|$.

2. **Packet Flooding (`AimInfo` Structure):**
   * Write raw vector $\vec{D}$ to `AimInfo + Offsets::Aim::Direction` (`0x40`).
   * Write scalar distance $dist$ to `AimInfo + Offsets::Aim::Distance` (`0x5C`).
   * Force critical damage group: `AimInfo + Offsets::Aim::HitGroup = 1` (`Head`).
   * Write `HitPoint` (`0x28`) = $\vec{P}_{\text{targetHead}}$.
   * Write `HitNormal` (`0x34`) = $\text{Normalize}(\vec{D})$.
   * Clear view obstruction flags: `ViewBlocked` (`0x71`) = `false`.

3. **Dual Collider Override (Anti-Torso Fallback):**
   * Extract target's `HeadCollider` pointer (e.g., `target + 0x740`).
   * Overwrite target's `LockedAimingCollider` slots (`0x80` & `0x78`) with `HeadCollider`.
   * Forces the game's bullet-physics engine to register a direct headshot collision even if the vector intersects body volume.

4. **Weapon Recoil Stabilization:**
   * On active trigger, read active `WeaponOnHand` (`LocalPlayer + 0x600`).
   * Zero out weapon recoil float: `WeaponData + 0x18 = 0.0f`.

---

## 5. Diagnostic Forensics & In-Game Verification Pipeline

Never guess why a mod or cheat is failing. Treat the running game as an empirical crime scene:

1. **Dual-Tier Diagnostic Logger (`logger.hpp`):**
   * Log real-time pointer status with explicit tags:
     * `[FOUND]` when an offset dereferences cleanly into valid memory ($\ge \text{0x10000}$).
     * `[NOT FOUND]` when an offset fails or returns null.
   * Write diagnostic logs to physical device storage:
     `/sdcard/Download/kernel_debug.txt` (accessible via Android file manager).
   * Rate-limit repeating logs (1.0s to 3.0s heartbeats) to prevent memory or I/O stutter.

2. **Log Autopsy Ladder:**
   * Step 1: Verify PID attachment.
   * Step 2: Verify `libil2cpp.so` base address discovery.
   * Step 3: Verify GameFacade → BaseGame → Match dereference.
   * Step 4: Verify LocalPlayer & Camera matrices.
   * Step 5: Verify entity dictionary traversal & active enemies count.
   * Step 6: Verify bone positions & silent aim trajectory vectors.

3. **Pre-Release Production Cleanup:**
   * Once offsets and features are confirmed working in-game, mute diagnostic I/O.
   * Strip file writing routines for zero disk overhead, maximum FPS throughput, and forensic stealth.

---

## 6. Standard CI/CD Workflow (`.github/workflows/build.yml`)

```yaml
name: Build Kernel Mod
on: [push, workflow_dispatch]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up NDK
        uses: nttld/setup-ndk@v1
        with:
          ndk-version: r25c
      - name: Build arm64-v8a Binary
        run: |
          mkdir build && cd build
          cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
                -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 ..
          make -j$(nproc)
      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: ninmods-kernel
          path: build/ninmods_kernel
```
