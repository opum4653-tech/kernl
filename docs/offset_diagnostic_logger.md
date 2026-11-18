# Kernel Offset Diagnostic & File Logging Architecture

**Document Version:** 1.0.0  
**Target Architecture:** Android Linux aarch64 (Kernel DMA / Process VM Read-Write)  
**Target Log Destination:** `/sdcard/Download/kernel_debug.txt`  
**Secondary Fallbacks:** `/storage/emulated/0/Download/kernel_debug.txt`, `/data/local/tmp/kernel_debug.txt`  

---

## 1. Objective & Forensic Rationale
When game features (ESP bounding boxes, laser tracer lines, silent aim bullet redirection, or auto headshot damage) exhibit unexpected behavior in-game—such as bullets registering on the body instead of the head, or ESP boxes failing to track—silent failures in memory traversal make root-cause isolation impossible.

This diagnostic subsystem introduces an **Automated Forensic Memory Auditor** that:
1. Traverses the entire Il2Cpp pointer hierarchy in real time.
2. Explicitly emits `[FOUND]` or `[NOT FOUND]` for every single offset, class pointer, bone node, collider, and weapon state.
3. Persists human-readable structured logs directly into the device's physical **Download folder** as a plain `.txt` file (`kernel_debug.txt`), viewable in any standard Android File Manager.

---

## 2. File Persistence & Storage Pipeline

### 2.1 File Location Strategy
Android storage mounts differ across devices, Android OS versions, and root/kernel environments. The logger attempts to acquire write access using the following priority ladder:
1. **Primary:** `/sdcard/Download/kernel_debug.txt` (Symlinked external storage accessible via stock Android Files and third-party file managers).
2. **Secondary:** `/storage/emulated/0/Download/kernel_debug.txt` (Absolute POSIX path for primary user profile 0).
3. **Tertiary Fallback:** `/data/local/tmp/kernel_debug.txt` (Internal sandbox partition accessible via root shell and ADB).

### 2.2 Storage Flood Prevention (Throttled Heartbeats)
The core rendering and memory loops run at 60Hz - 100Hz. Writing to disk every frame would flood the storage with gigabytes of logs within minutes.
The diagnostic logger employs an **Edge-Triggered State Transition & Periodic Heartbeat** architecture:
- **State Change Triggers:** When an offset or entity transitions from valid to invalid (or vice-versa), it logs immediately.
- **Heartbeat Rate Limiter:** Continuous status snapshots (such as active player count, local player address, and bone coordinates) are rate-limited to **1.0 second (1000 ms)** intervals.
- **Buffer Flushing:** Every written record is immediately flushed via `fflush()` to prevent data loss in the event of an unexpected process exit.

---

## 3. Comprehensive Memory Verification Ladder

The diagnostic system inspects six distinct memory tiers:

### Tier 0: Process & Library Discovery
| Component | Verification Target | Expected Behavior | Failure Log Tag |
| :--- | :--- | :--- | :--- |
| **Process ID (PID)** | Target game process | PID > 0 | `[NOT FOUND] Game Process (com.dts.freefireth / max)` |
| **Library Base** | `libil2cpp.so` load base | Valid 64-bit base address (> 0x1000000000) | `[NOT FOUND] libil2cpp.so memory base address` |

### Tier 1: Base Chain Traversal
| Step | Offset | Source Pointer | Resolved Target | Failure Log Tag |
| :--- | :--- | :--- | :--- | :--- |
| 1 | `Offsets::GameFacade` (`0xA72E4E0`) | `libil2cpp.so` | `GfCell` pointer | `[NOT FOUND] GameFacade cell pointer` |
| 2 | Direct Dereference | `GfCell` | `GfClass` Il2CppClass | `[NOT FOUND] GameFacade class pointer` |
| 3 | `Offsets::GameFacade_P2` (`0xB8`) | `GfClass` | `StaticFields` struct | `[NOT FOUND] GameFacade StaticFields` |
| 4 | `Offsets::StaticFields_Instance` (`0x8`)| `StaticFields` | `BaseGame` instance | `[NOT FOUND] BaseGame instance pointer` |
| 5 | `Offsets::BaseGame_Match` (`0x90`) | `BaseGame` | `m_Match` instance | `[NOT FOUND] Match instance (Offset 0x90)` |
| 6 | `Offsets::Match_LocalPlayer` (`0xD8`) | `m_Match` | `localPlayer` pointer | `[NOT FOUND] LocalPlayer entity (Offset 0xD8)` |

### Tier 2: Camera Matrix Chain
| Step | Offset | Source Pointer | Resolved Target | Failure Log Tag |
| :--- | :--- | :--- | :--- | :--- |
| 1 | `Offsets::Player_Camera` (`0x690`) | `localPlayer` | `FollowCamera` object | `[NOT FOUND] FollowCamera (Offset 0x690)` |
| 2 | `Offsets::Camera_Follow` (`0x28`) | `FollowCamera` | `Camera` internal object | `[NOT FOUND] Camera object (Offset 0x28)` |
| 3 | `Offsets::Camera_IntPtr` (`0x10`) | `Camera` | `IntPtr` native pointer | `[NOT FOUND] Camera native IntPtr (Offset 0x10)` |
| 4 | `Offsets::Camera_Matrix` (`0x100`) | `IntPtr` | `viewMatrix` (4x4 matrix) | `[NOT FOUND] Camera ViewMatrix (Offset 0x100)` |

### Tier 3: Player Dictionary & Entity Enumeration
| Step | Offset | Source Pointer | Resolved Target | Failure Log Tag |
| :--- | :--- | :--- | :--- | :--- |
| 1 | `Offsets::Match_PlayerDict` (`0xC0`) | `BaseGame` | MonoDictionary pointer | `[NOT FOUND] Player Dictionary (Offset 0xC0)` |
| 2 | `Offsets::Dict::Entries` (`0x18`) | Dictionary | Array descriptor | `[NOT FOUND] Dictionary Entries array` |
| 3 | `Offsets::Dict::ArrayHeader` (`0x20`)| Array descriptor | Raw entry storage | `[NOT FOUND] Dictionary Array Data` |

### Tier 4: Transform Hierarchy & Bone Nodes
| Target Bone | Offset | Resolution Function | Valid Output Check | Failure Log Tag |
| :--- | :--- | :--- | :--- | :--- |
| **Head Node** | `0x6A0` (`GBKFHDFCPMD`) | `GetNodePosition(node)` | World Vector != (0,0,0) | `[NOT FOUND] Head Transform Node (0x6A0)` |
| **Toe Node** | `0x6E8` (`GMJLIMFHMAE`) | `GetNodePosition(node)` | World Vector != (0,0,0) | `[NOT FOUND] Left Toe Transform Node (0x6E8)` |
| **Spine Node** | `0x6B8` (`CBHHCCNKOND`) | `GetNodePosition(node)` | World Vector != (0,0,0) | `[NOT FOUND] Spine Transform Node (0x6B8)` |

### Tier 5: Silent Aim & Combat State
| Target State | Offset | Validation Criteria | Failure Log Tag |
| :--- | :--- | :--- | :--- |
| **IsFiring** | `0x850` / `0x848` | Boolean state read from player | `[NOT FOUND] IsFiring state` |
| **AimInfo** | `0xE58` (`CGKJLKPMGDJ`)| Valid pointer to trajectory struct | `[NOT FOUND] LastAimingInfoFromWeapon (0xE58)` |
| **Head Collider** | `0x740` / `0x738` | Valid collider pointer on target | `[NOT FOUND] HeadCollider (0x740/0x738)` |
| **Aiming Collider** | `0x80` / `0x78` | Valid AttackableEntity collider | `[NOT FOUND] LockedAimingCollider (0x80/0x78)` |
| **Weapon Recoil** | `0x18` | Active weapon data struct | `[NOT FOUND] Weapon Recoil offset` |

---

## 4. Log Output Format Example

```text
================================================================================
[2026-11-18 10:15:00] [INFO] NinMods Kernel Diagnostic Logger Initialized
[2026-11-18 10:15:00] [INFO] Target Log Path: /sdcard/Download/kernel_debug.txt
================================================================================
[2026-11-18 10:15:01] [FOUND]     Process PID: 14285 (com.dts.freefireth)
[2026-11-18 10:15:01] [FOUND]     libil2cpp.so Base: 0x738A400000
[2026-11-18 10:15:01] [FOUND]     GameFacade Cell: 0x7394B2E4E0 (Offset +0xA72E4E0)
[2026-11-18 10:15:01] [FOUND]     GameFacade Class: 0x7205A12400
[2026-11-18 10:15:01] [FOUND]     StaticFields: 0x7205A124B8 (Offset +0xB8)
[2026-11-18 10:15:01] [FOUND]     BaseGame Instance: 0x7189104080 (Offset +0x8)
[2026-11-18 10:15:01] [FOUND]     Match Instance: 0x7189A20100 (Offset +0x90)
[2026-11-18 10:15:01] [FOUND]     LocalPlayer: 0x719C405000 (Offset +0xD8)
[2026-11-18 10:15:01] [FOUND]     FollowCamera: 0x719C405690 -> IntPtr: 0x718B201000
[2026-11-18 10:15:01] [FOUND]     Local Head Node: 0x71842006A0 -> WorldPos: (124.5, 42.1, 88.3)
[2026-11-18 10:15:01] [FOUND]     Player Dictionary: 0x7189104140 -> Count: 14 players
[2026-11-18 10:15:02] [FOUND]     Target [1]: Addr=0x719A120000 | HP=200/200 | Dist=42.5m
[2026-11-18 10:15:02] [FOUND]       - Head Node: 0x71850106A0 (0x6A0) -> Pos: (132.1, 42.8, 102.4)
[2026-11-18 10:15:02] [FOUND]       - Toe Node:  0x71850106E8 (0x6E8) -> Pos: (132.1, 41.1, 102.4)
[2026-11-18 10:15:02] [FOUND]       - Head Collider: 0x7188120740 (0x740)
[2026-11-18 10:15:02] [FOUND]       - LockedAimingCollider: 0x7188120080 (0x80)
[2026-11-18 10:15:02] [FOUND]     Silent Aim State: IsFiring=1 | AimInfo=0x719C405E58 (0xE58)
[2026-11-18 10:15:02] [DIAGNOSTIC] Bullet redirected to Head: Vector=(7.6, 0.7, 14.1) | HitGroup=1 (Head)
```

If an offset is missing or changed in a future game update, the log will immediately pinpoint the exact failing tier:
```text
[2026-11-18 10:15:05] [NOT FOUND] Match Instance: 0x0 (Offset +0x90 from BaseGame 0x7189104080)
[2026-11-18 10:15:05] [NOT FOUND] Head Transform Node: 0x0 (Offset +0x6A0 from Player 0x719A120000)
```

---

## 5. Architectural Verification & Zero Logic Impact
- The diagnostic logger is completely asynchronous and thread-safe.
- Its logging calls are non-blocking or guarded by timed intervals.
- The logger operates purely as an observer and does not alter the mathematical calculations of ESP projections or silent aim vectors.
- Modifying offsets remains 100% centralized inside `src/offsets/offsets.hpp`.
