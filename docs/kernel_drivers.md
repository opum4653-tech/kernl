# KERNEL DRIVERS & LOADER ARCHITECTURAL RUNBOOK

This document establishes the authoritative specification for all kernel drivers, loader integration patterns, IOCTL protocols, and Ring-0 memory quarantine standards within the `ninmods-kernel` ecosystem.

Every engineer and AI agent working on this repository MUST strictly follow these rules and NEVER deviate.

---

## 1. LOADER & DRIVER DEPLOYMENT ARCHITECTURE

### 1.1 Loader Application (`ninmods` / `crack_master_loncher`)
* **Role:** Android Root Application (Kotlin / Jetpack Compose).
* **Location in source:** `C:\Users\Sajala\Documents\mods\ninmods\app\src\main\assets\kernel\driver_loader.sh` and `DriverRepository.kt`.
* **Deployment Workflow:**
  1. Downloads `Kernel_Drivers.zip` directly from backend.
  2. Extracts driver bundles into internal app directory:
     - `files/drivers/QX/` (QingXu mode `.ko.sh` / `.sh` files)
     - `files/drivers/RT/` (RedTea mode `.ko.sh` / `.sh` files)
  3. Executes driver scripts via Root Shell (`su` execution):
     - Executes matching `.ko.sh` (which decodes and `insmod`s the kernel module).
     - Obtains kernel symbols and creates dynamic `/dev/` character nodes or registers `prctl` memory hooks.
  4. Launces the compiled `ninmods-kernel` executable into memory via daemon fork.

---

## 2. DRIVER TYPES & LOW-LEVEL PROTOCOLS

### 2.1 Mode 1: RT DEV (`c_rt_dev` in `includes/driver/rt_dev.h`)
* **Interface:** Standard Linux character device node `/dev/<dynamic_name>`.
* **Device Nodes Scanned:**
  - Dynamic discovery via `find_rt_dev_node()`.
  - Fallback scan list: `/dev/rt_dev`, `/dev/redtea_dev`, `/dev/rt_mem`, `/dev/kn_dev`, `/dev/mimic_dev`.
* **IOCTL Command Codes:**
  - `RT_IOCTL_INIT (0x8004)`: Binds target game PID to the driver.
  - `RT_IOCTL_READ (0x8001)`: Ring-0 memory read (`copy_to_user`).
  - `RT_IOCTL_WRITE (0x8002)`: Ring-0 memory write (`copy_from_user`).
* **Memory Packet Structure:**
  ```cpp
  struct rt_dev_rw {
      pid_t     pid;
      uintptr_t addr;
      uintptr_t buffer;
      size_t    size;
  };
  ```
* **Memory Quarantine:**
  - Write method signature MUST strictly take `const void* buffer`.

---

### 2.2 Mode 2: QX Driver (`c_qx_driver` in `includes/driver/qx_driver.h`)
* **Interface:** QingXu Dynamic mknod Device Node.
* **Auto-probing Mechanism:**
  - Discovers dynamic device nodes registered by `insmod qx.ko`.
  - Communicates via specialized IOCTL memory packet:
  ```cpp
  struct qx_rw_data {
      pid_t     pid;
      uint64_t  addr;
      uint64_t  buffer;
      uint64_t  size;
  };
  ```
* **Features:**
  - Zero userspace tracing.
  - Automatically isolates memory copying in kernel worker context.

---

### 2.3 Mode 3: KPM Driver (`c_kpm_driver` in `includes/driver/kpm_driver.h`)
* **Interface:** Ring-0 `prctl` hook (Neo prctl / TB prctl for APatch and KernelSU).
* **Zero Footprint:**
  - Does NOT create any character device in `/dev/` (completely undetectable by file system scans).
  - Memory reads/writes are dispatched via customized `prctl` syscall codes:
    - `PR_MEM_READ (0x52454144 / 'READ')`
    - `PR_MEM_WRITE (0x57524954 / 'WRIT')`
* **Buffer Chunking Invariant:**
  - Historical `pewpew` bug: `pewpew` aborted reads > 4096 bytes with `if (size > 4096) return false;`.
  - `ninmods-kernel` modern KPM driver chunk-loops requests in 4096-byte slices (`while (bytes_left > 0)`), supporting arbitrary buffer sizes without truncation or crash.

---

### 2.4 Mode 4: RT Hook (`c_rt_hook` in `includes/driver/rt_hook.h`)
* **Interface:** In-kernel syscall table detour or function pointer interception.
* **Payload:** Direct kernel memory read/write dispatched via hooked syscall.

---

### 2.5 Multi-Engine Auto-Probing Ladder (`c_dev_compat` in `includes/driver/dev_compat.h`)
* **Execution Sequence on Startup:**
  1. First probe: `RT DEV` (`c_rt_dev`)
  2. Second probe: `QX Driver` (`c_qx_driver`)
  3. Third probe: `KPM Driver` (`c_kpm_driver`)
  4. Fourth probe: `RT Hook` (`c_rt_hook`)
* **Safety Invariant:**
  - If a driver fails to probe or returns invalid node handles, the engine silently closes the descriptor and steps down the ladder.
  - NEVER call `exit(0)` on probe failure (which was a fatal flaw in legacy `pewpew`).

---

## 3. STRICT RULES FOR FUTURE AGENTS

1. **NEVER Guess or Hallucinate Driver APIs:**
   - Every driver class MUST strictly implement `c_driver` interface from `includes/driver.h`:
     - `bool initialize(pid_t pid)`
     - `bool init_key(char* key)`
     - `bool read(uintptr_t addr, void* buffer, size_t size)`
     - `bool write(uintptr_t addr, const void* buffer, size_t size)`
     - `const char* get_driver_name()`
   - Write buffer parameter MUST ALWAYS be `const void* buffer`. Taking `void* buffer` breaks callers passing string literals or const memory.

2. **100% Ring-0 Memory Quarantine:**
   - In production builds with active kernel drivers, NEVER fall back to `/proc/<pid>/mem` or `process_vm_readv`.
   - All memory access must route through the active kernel driver handle.

3. **No Local File Writes on Android:**
   - Never write debug logs, offsets, or configuration files to `/sdcard/` or `/data/local/tmp/`.
   - All runtime states exist strictly in RAM.
