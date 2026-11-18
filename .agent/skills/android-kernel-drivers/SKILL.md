---
name: android-kernel-drivers
description: >-
  Comprehensive architectural guide and runbook for Android root game modding kernel drivers:
  KPM (Kernel Patch Module / Neo prctl & TB prctl for APatch/KernelSU), QX (QingXu dynamic mknod),
  RT (RedTea Hook & DEV), and standard /dev IOCTL interfaces. Covers ring-0 memory copying,
  device node obfuscation, SELinux zero-footprint bypasses, and multi-engine auto-probing
  on KernelSU, APatch, and Magisk.
---

# Android Kernel Driver Architectures: KPM, QX, RT & Direct IOCTL

This runbook documents the engineering architectures, communication protocols, memory copying routines,
and stealth mechanisms for Android Root Kernel Drivers used in high-performance external game modding
(KernelSU, APatch, and Magisk).

---

## 1. Comparative Driver Architecture Matrix

| Driver Engine | Transport Mechanism | Kernel Footprint | Userland Detectability | Target Environments |
| :--- | :--- | :--- | :--- | :--- |
| **KPM (Kernel Patch Module)** | `sys_prctl` syscall hijacking (`0x4E41` / `0x63687501`) or Direct Syscall 270/271 | Ring 0 kernel patch module (`.kpm`). Direct `copy_from_user` / `copy_to_user`. | **Zero character device (`/dev`)**. Invisible to all filesystem/VFS audits. | APatch, KernelSU (Rooted Android 12-15) |
| **QX Driver (QingXu)** | Dynamic Major/Minor character device (`mknod`) + IOCTL | Dynamic character device linked to deleted daemon in `/data/[a-z]{6}`. | Random device path; auto-unlinked after open. High stealth. | APatch, Magisk, Custom Kernels |
| **RT DEV (RedTea DEV)** | Known/Scanned character device in `/dev/` (`wanbai`, `video188`, `CheckMe`, etc.) | Standard Linux character device driver with proprietary IOCTL structures. | Scannable by device names unless unlinked or masked. | Magisk, KernelSU, Custom ROMs |
| **RT Hook (RedTea Hook)** | Intercepted syscall/IOCTL dispatch table (`HOOK_OP_*`) | Hooks kernel syscall dispatch or existing driver file operations. | Low userland presence; uses virtual fd. | APatch, KernelSU |
| **DEV (Most Compatible)** | Direct character device node (`/dev/*`) + handshake key (`0x800`) | Standard `miscdevice` or `cdev` with security token verification. | High compatibility across all Android kernels (3.18 to 6.6). | Universal Android Root |

---

## 2. Engine 1: KPM (Kernel Patch Module — Neo & TB prctl)

KPM is the apex stealth driver architecture. It runs as a native kernel patch module inside APatch or KernelSU,
intercepting the standard `sys_prctl` system call to execute memory reads and writes without opening any file descriptors.

### Protocol A: Neo KPM (`0x4E41`)
* **Magic Number:** `0x4E41` (`NA_PRCTL_MAGIC`)
* **Command Structure:**
  ```cpp
  #define NA_PRCTL_MAGIC      0x4E41
  #define NA_CMD_READ_MEM     1
  #define NA_CMD_WRITE_MEM    2
  #define NA_CMD_GET_PID      5
  #define NA_MAX_RW_SIZE      4096
  #define NA_MAX_PKG_NAME     256

  struct na_cmd {
      uint32_t op;                     // 1 = Read, 2 = Write, 5 = Get PID
      uint32_t pid;                    // Target Game PID
      uint64_t addr;                   // Virtual Memory Address
      uint32_t size;                   // Read/Write Byte Count (<= 4096)
      int32_t  result;                 // 0 = Success, <0 = Error
      int32_t  screen_w;
      int32_t  screen_h;
      char     pkg[NA_MAX_PKG_NAME];   // Package Name Filter
      uint8_t  data[NA_MAX_RW_SIZE];   // Payload Buffer
  };
  ```
* **Execution via `prctl`:**
  ```cpp
  na_cmd cmd{};
  cmd.op = NA_CMD_READ_MEM;
  cmd.pid = target_pid;
  cmd.addr = target_address;
  cmd.size = read_size;
  syscall(__NR_prctl, NA_PRCTL_MAGIC, &cmd, 0, 0, 0);
  if (cmd.result == 0) {
      memcpy(dest_buffer, cmd.data, read_size);
  }
  ```

### Protocol B: TB KPM (TongBao / TianBao)
* **Magic Opcodes:**
  * `PRCTL_MEM_READ`: `0x63687501`
  * `PRCTL_MEM_WRITE`: `0x63687502`
* **Operation Structure:**
  ```cpp
  struct mem_operation {
      pid_t    target_pid;
      uint64_t addr;
      void    *buffer;
      uint64_t size;
  };
  ```
* **Execution:**
  ```cpp
  mem_operation op{ target_pid, target_addr, dest_buf, size };
  long ret = syscall(__NR_prctl, PRCTL_MEM_READ, &op, 0, 0, 0);
  return (ret == 0);
  ```

### Direct Syscall Fallback (ARM64 `process_vm_readv`):
When KPM module is not loaded, direct kernel syscall execution bypasses standard libc wrappers:
* `__NR_process_vm_readv` on ARM64: **Syscall 270**
* `__NR_process_vm_writev` on ARM64: **Syscall 271**
```cpp
struct iovec local{ buffer, size };
struct iovec remote{ (void*)addr, size };
ssize_t bytes = syscall(270, pid, &local, 1, &remote, 1, 0);
```

---

## 3. Engine 2: QX Driver (QingXu 10 ~ 11.4 Dynamic Mknod)

The QX driver operates by hiding its background server binary inside `/data/` and automatically deleting its own file
(`(deleted)` in `/proc/*/exe`), while dynamically generating character device nodes on the fly.

### Architecture & Discovery Workflow:
1. **Locate Server Daemon:**
   Inspect `/proc/*/exe` links matching deleted binary pattern:
   ```bash
   ls -l /proc/*/exe 2>/dev/null | grep -E "/data/[a-z]{6} \(deleted\)"
   ```
2. **Dynamic Major Number Resolution:**
   Resolve the character device major number allocated dynamically by `alloc_chrdev_region` from the kernel driver.
3. **Mknod & Unlink Cycle:**
   * Create transient device node:
     `mknod /dev/<random_name> c <major> <minor>`
   * Open file descriptor:
     `fd = open("/dev/<random_name>", O_RDWR);`
   * **Stealth Unlink:**
     `unlink("/dev/<random_name>");`
     *(The node is immediately removed from the filesystem, but the open `fd` remains completely valid for IOCTL operations!)*

---

## 4. Engine 3: RT Driver (RedTea Hook & DEV)

RedTea is engineered for high-throughput batch operations, reducing context-switch latency between userland and kernel.

### 1. RT Hook (Mode 1):
* Uses virtual file descriptor dispatch table (`ioctl(0, HOOK_OP_READ_MEM, &cm)`).
* **Opcodes:**
  * `HOOK_OP_READ_MEM` = `601`
  * `HOOK_OP_WRITE_MEM` = `602`
  * `HOOK_OP_MODULE_BASE` = `603`
  * `HOOK_OP_HIDE_PROCESS` = `605`

### 2. RT DEV (Mode 2):
* Auto-scans `/dev/` for standard obfuscated device names:
  `wanbai`, `CheckMe`, `Ckanri`, `lanran`, `video188`, `facking`, `YBBBYN`.
* Connects via standard `COPY_MEMORY` IOCTL structure:
  ```cpp
  typedef struct _COPY_MEMORY {
      pid_t pid;
      uintptr_t addr;
      void* buffer;
      size_t size;
  } COPY_MEMORY;
  ```

---

## 5. Engine 4: DEV Most Compatible Mode (Universal Driver)

The standard universal character device driver implements a three-step security handshake to protect against unauthorized probing:

```text
Userland Engine                               Kernel Driver
       │                                            │
       ├──── DEV_OP_INIT_KEY (0x800: "key") ───────►│  Validate driver token
       │◄─── Status 0 (Success) ────────────────────┤
       │                                            │
       ├──── DEV_OP_READ_MEM (0x801) ──────────────►│  Hardware page table walk
       │◄─── Buffer copied via copy_to_user ────────┤
       │                                            │
       ├──── DEV_OP_HIDE_PROCESS (0x804) ──────────►│  Remove task from task_list
```

### Opcodes:
* `DEV_OP_INIT_KEY` = `0x800`
* `DEV_OP_READ_MEM` = `0x801`
* `DEV_OP_WRITE_MEM` = `0x802`
* `DEV_OP_MODULE_BASE` = `0x803`
* `DEV_OP_HIDE_PROCESS` = `0x804`

---

## 6. Multi-Engine Auto-Probing Ladder

To ensure zero setup friction across diverse user devices, the driver controller must implement
an automated fallback probing ladder:

```cpp
bool c_driver::auto_connect() {
    // 1. Probe KPM (Zero-node apex stealth)
    if (m_kpm.probe_tb() || m_kpm.probe_neo()) {
        m_selection = 5; // KPM
        return true;
    }
    // 2. Probe QX Driver (Dynamic mknod daemon)
    if (m_qx.init()) {
        m_selection = 4; // QX
        return true;
    }
    // 3. Probe RT DEV (Scanned device nodes)
    if (m_rt_dev.init()) {
        m_selection = 2; // RT DEV
        return true;
    }
    // 4. Probe RT Hook
    if (m_rt_hook.init()) {
        m_selection = 1; // RT Hook
        return true;
    }
    // 5. Fallback: Universal DEV Compat
    if (m_dev_compat.init()) {
        m_selection = 3; // DEV Compat
        return true;
    }
    return false;
}
```

---

## 7. Anti-Detection Stealth Protocols for Kernel Drivers

1. **Unlink on Open (`O_RDWR` + `unlink`):**
   Immediately after opening any character device node (`open("/dev/...", O_RDWR)`), call `unlink(path)`.
   The inode disappears from the VFS directory tree, preventing game security scanners from finding the driver node.
2. **Process Masquerading (`prctl`):**
   Always invoke `prctl(PR_SET_NAME, "kworker/u16:1")` on daemon startup to blend seamlessly into standard Linux kernel workqueue threads.
3. **Hardware Page Walking (`pgd/pud/pmd/pte`):**
   When target games set `PROT_NONE` or remove virtual page permissions to detect memory reads, the kernel driver must walk the physical page tables directly, copying physical frames without raising page faults.
4. **Zero Persistent Traces:**
   Never leave temporary files, pipes, or raw debug dumps in `/data/local/tmp` in production release builds.
