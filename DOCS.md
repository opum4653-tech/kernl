# NINMODS KERNEL — System Architecture & Technical Documentation

This document defines the architecture, kernel communication protocol, offset matrix, and operational contracts of the **NinMods Kernel** floating engine (`ninmods-kernel`).

---

## 1. System Architecture & Scope

- **Module Name:** `ninmods-kernel`
- **Menu Brand Name:** `NinMods`
- **Primary Theme Color:** Cyber Purple / Violet (`#AD47FA` / `ImVec4(0.68f, 0.28f, 0.98f, 1.0f)`)
- **Execution Domain:** Linux / Android ARM64-v8a Native Executable
- **Privilege Requirement:** Root (UID 0) + Ring 0 Linux Kernel Driver
- **Memory Operation Mode:** Strictly Kernel-Space Memory Operations (Zero userspace `process_vm_readv` or `/proc/pid/mem` fallbacks)

```text
┌────────────────────────────────────────────────────────┐
│                   Target Application                   │
│          com.dts.freefireth (Unity Player)             │
└──────────────────────────▲─────────────────────────────┘
                           │ (Ring 0 Memory Read / Write)
┌──────────────────────────┴─────────────────────────────┐
│              Linux Kernel Space (Ring 0)               │
│        Driver: RT Hook / RT DEV / DEV / QX / KPM       │
└──────────────────────────▲─────────────────────────────┘
                           │ IOCTL (0x801 / 0x802 / KPM)
┌──────────────────────────┴─────────────────────────────┐
│             NinMods Kernel Floating Engine             │
│            Executable: /dev/ninmods-kernel             │
│   SurfaceComposer Native Window + ImGui Cyber Purple   │
└────────────────────────────────────────────────────────┘
```

---

## 2. Kernel Driver Dependency Contract

`ninmods-kernel` is engineered strictly as a client to a Ring 0 Kernel Driver. It does not perform unprivileged userspace memory probing.

### Supported Driver Channels
1. **RT Hook:** Direct kernel hook interface via character device nodes.
2. **RT DEV:** Real-time character device driver binding.
3. **DEV (Generic):** Standard high-compatibility device communication node.
4. **QX Daemon:** QX 10.x - 11.4 specialized protocol interface.
5. **KPM (Neo):** Kernel Patch Module interface utilizing custom `prctl` syscall dispatch.

### Strict Driver Initialization Invariant
During startup, `driver->initialize((pid_t)game_pid)` attempts to acquire a valid communication channel:
- If a valid driver node or KPM interface is not active, the engine terminates immediately:
  ```text
  [FATAL] Driver not loaded! Exiting...
  ```
- Users must load the matching kernel driver via **DriverLoader** prior to launching the floating engine.

---

## 3. Game Target & Offset Synchronization (OB55 / 1.132.1)

The engine synchronizes with `libil2cpp.so` and `libunity.so` structures for target version **OB55 (1.132.1)**:

| Symbol / Field | Offset (Hex) | Description |
| :--- | :--- | :--- |
| `GameFacade` | `0xA72E4E0` | Global facade manager entry point |
| `Player_Rotation` | `0x614` | View angle / orientation quaternion |
| `Player_HeadTF` | `0x6A0` | Head transform node pointer |
| `Player_FootTF` | `0x6E8` | Foot / root transform node pointer |
| `Player_Camera` | `0x690` | Active camera follow transform |
| `Player_IsFiring` | `0x848` | Firing state boolean |
| `Player_Avatar` | `0x770` | Character model transform collection |
| `Player_HedColider` | `0x738` | Head collider component reference |
| `Player_DeathInfo` | `0x24A8` | Player death / knock-down state |
| `Player_IsBot` | `0x4A0` | AI / Bot discriminator flag |
| `Player_Name` | `0x490` | Player nickname string pointer |
| `Camera_Follow` | `0x28` | Main camera follow matrix pointer |
| `WeaponData` | `0x80` | Current active weapon state offset |
| `Bullet_SpawnPos` | `0x4C` | Muzzle spawn position (`hitPos`) |
| `Bullet_DirWrite` | `0x40` | Direction vector write (`firePos`) |

---

## 4. Zero-Trace Display & Crash-Proof Architecture

To prevent runtime crashes and memory leaks across Android 10 through 15:

### 1. Hardware Direct Display Probing (`display_manager.cpp`)
- **Zero-Fork Kernel Nodes:** Reads `/sys/class/graphics/fb0/virtual_size` and `/sys/class/graphics/fb0/modes` directly (0.05ms execution time, 0% CPU overhead, 0 fork syscalls).
- **Framebuffer IOCTL:** Hardware-level query via `FBIOGET_VSCREENINFO` on `/dev/graphics/fb0`.
- **System Properties:** In-memory Bionic property fallback (`ro.surface_flinger.max_graphics_width/height`).
- **RAM Caching:** Display parameters are queried once at initialization and cached in RAM. Render loops execute with zero external syscalls.

### 2. Isolated Native Surface (`ANativeWindowCreator.h`)
- Volatile `libgui.so` binder calls (`getDisplayState` and `getInternalDisplayToken`) that cause `SIGSEGV (Signal 11)` on modern Android systems are decoupled and replaced with the safe display provider.

### 3. Modular Crash Guard (`crash_handler.cpp`)
- Configures an alternate signal stack (`sigaltstack`) with 64 KB of dedicated memory.
- Installs zeroed `sigaction` handlers for `SIGSEGV`, `SIGBUS`, `SIGFPE`, `SIGILL`, and `SIGABRT` to cleanly catch and intercept memory exceptions without terminal aborts.

### 4. Zero-Trace Persistent Anchor
- Floating logo position is stored in pure memory-backed file descriptor paths (`/dev/.nin_logo_pos.txt`), ensuring zero persistent trace on `/data/local/tmp`.

---

## 5. Directory & File Matrix

```text
Kernel/Kernel/
├── .github/workflows/
│   └── build.yml                            # GitHub Actions CI for ARM64 NDK compilation
├── .gitignore                               # Ignores obj/, libs/, logs, and temporary artifacts
├── DOCS.md                                  # Enterprise architecture and technical documentation
└── jni/
    ├── Android.mk                           # NDK build configuration & static lib linkage
    ├── Application.mk                       # Application flags & ABI definition (arm64-v8a)
    ├── include/
    │   ├── Android_draw/draw.h              # Native surface draw definitions
    │   ├── Android_touch/TouchHelperA.h     # Input device event parser
    │   ├── ImGui/                           # ImGui 1.90.1 core & Android/OpenGL backends
    │   ├── native_surface/                  # Isolated ANativeWindow surface creator
    │   ├── external/                        # Prebuilt OpenSSL, libcurl, and JSON headers
    │   └── Scarecrow/offsets.h              # OB55 synchronized game offsets
    └── src/
        ├── main.cpp                         # Process lifecycle, touch routing, floating logo
        ├── display_manager.hpp & .cpp       # Zero-fork hardware display probe & RAM cache
        ├── crash_handler.hpp & .cpp         # Alternate signal stack crash protection
        ├── Android_draw/draw.cpp            # EGL surface initialization & frame presenter
        └── ENC/oxorany.cpp                  # Compile-time string & symbol encryption
```

---

## 6. Automated CI Build & Release Pipeline

Compilation is automated on GitHub Actions using Android NDK `r25c`:
1. **Trigger:** Automatically invoked on every `push` or `pull_request` to the `main` branch.
2. **Build Toolchain:** `ndk-build` targeting `arm64-v8a` with Clang `-Oz` optimization, hidden visibility, and full binary stripping (`llvm-strip`).
3. **Artifact Output:** Standalone native ELF binary named **`ninmods-kernel`** packaged and published to GitHub Actions artifacts.

---

## 7. Codebase Style & Commenting Invariants

1. **Strict Prohibition of Decorative Box-Drawing Comment Banners:**
   - Decorative unicode box-drawing banners (e.g. `// ══════════════...`, `// ──────────────...`, or fancy ASCII separators) are strictly prohibited.
   - All comments must be minimal, clean, standard single-line (`// Comment`) without decorative ASCII borders or box art.
2. **Language Invariant:**
   - All source code, symbols, logging, comments, and strings must be written strictly in professional English.
   - Regional or non-English scripts (such as Bengali) must never be added to code comments or source files.
3. **Clean Code & Zero Clutter Policy:**
   - Backup files (`.bak`, `.old`), uncommitted binaries (`libs/`), intermediate object folders (`obj/`), and empty directories must never be checked into the repository.

