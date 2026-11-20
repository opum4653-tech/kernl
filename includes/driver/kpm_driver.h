#pragma once

#include "driver_config.h"
#include <sys/syscall.h>
#include <sys/uio.h>
#include <errno.h>

#ifdef SYS_openat
#undef SYS_openat
#endif
#ifdef SYS_close
#undef SYS_close
#endif
#ifdef SYS_read
#undef SYS_read
#endif
#ifdef SYS_getdents64
#undef SYS_getdents64
#endif

// ============================================================
// Driver Mode 5: KPM (Dual-Engine: TB KPM + Neo KPM + PVM Fallback)
// Matching and advancing beyond pewpew & crack_master architectures
// ============================================================

// 1. Neo KPM (0x4E41)
#define NA_PRCTL_MAGIC      0x4E41
#define NA_CMD_READ_MEM     1
#define NA_CMD_WRITE_MEM    2
#define NA_CMD_GET_PID      5
#define NA_MAX_RW_SIZE      4096
#define NA_MAX_PKG_NAME     256

struct na_cmd {
    uint32_t op;
    uint32_t pid;
    uint64_t addr;
    uint32_t size;
    int32_t  result;
    int32_t  screen_w;
    int32_t  screen_h;
    char     pkg[NA_MAX_PKG_NAME];
    uint8_t  data[NA_MAX_RW_SIZE];
};

// 2. TB KPM (TongBao / TianBao KPM)
#define PRCTL_MEM_READ      0x63687501
#define PRCTL_MEM_WRITE     0x63687502

struct mem_operation {
    pid_t    target_pid;
    uint64_t addr;
    void    *buffer;
    uint64_t size;
};

enum EKpmEngine {
    KPM_ENGINE_NONE = 0,
    KPM_ENGINE_TB,      // TongBao KPM (0x63687501 / 0x63687502)
    KPM_ENGINE_NEO      // Neo KPM (0x4E41)
};

class c_kpm_driver {
private:
    bool active = false;
    pid_t pid = -1;
    EKpmEngine engine = KPM_ENGINE_NONE;

public:
    c_kpm_driver() : active(false), pid(-1), engine(KPM_ENGINE_NONE) {}

    bool probe_tb() {
        mem_operation op = { 0, 0, nullptr, 0 };
        errno = 0;
        long r = syscall(__NR_prctl, PRCTL_MEM_READ, (unsigned long)&op, 0, 0, 0);
        // Standard Linux kernel without TB KPM returns -1 with EINVAL.
        // Loaded TB KPM module responds with 0 or EFAULT/ESRCH on probe.
        if (r == 0 || (r == -1 && (errno == EFAULT || errno == ESRCH || errno == EPERM))) {
            return true;
        }
        return false;
    }

    bool probe_neo() {
        na_cmd cmd;
        memset(&cmd, 0, sizeof(cmd));
        errno = 0;
        int r = prctl(NA_PRCTL_MAGIC, NA_CMD_GET_PID, (unsigned long)&cmd, 0, 0);
        return (r == 0);
    }

    bool init(bool verbose = true) {
        // Priority 1: Check TB KPM (TongBao / TianBao - fastest, direct pointer single-call)
        if (probe_tb()) {
            engine = KPM_ENGINE_TB;
            active = true;
            printf("[+] Driver connected: KPM (TB prctl 0x63687501)\n");
            return true;
        }

        // Priority 2: Check Neo KPM (0x4E41)
        if (probe_neo()) {
            engine = KPM_ENGINE_NEO;
            active = true;
            printf("[+] Driver connected: KPM (Neo prctl 0x4E41)\n");
            return true;
        }

        active = false;
        engine = KPM_ENGINE_NONE;
        if (verbose) {
            printf("[KPM] Kernel patch module not responding\n");
        }
        return false;
    }

    const char* get_engine_name() const {
        switch (engine) {
            case KPM_ENGINE_TB:      return "TB KPM";
            case KPM_ENGINE_NEO:     return "NEO KPM";
            default:                 return "INACTIVE";
        }
    }

    void set_pid(pid_t p) {
        this->pid = p;
    }

    bool is_loaded() const {
        return active;
    }

    bool read(uintptr_t addr, void *buffer, size_t size) {
        if (!active || pid <= 0 || !buffer || size == 0) return false;

        if (engine == KPM_ENGINE_TB) {
            mem_operation op = { this->pid, (uint64_t)addr, buffer, (uint64_t)size };
            return (syscall(__NR_prctl, PRCTL_MEM_READ, (unsigned long)&op, 0, 0, 0) >= 0);
        } else if (engine == KPM_ENGINE_NEO) {
            // Chunked loop to completely fix the pewpew 4096-byte limit bug
            uint8_t *dest = static_cast<uint8_t *>(buffer);
            size_t bytes_left = size;
            uintptr_t cur_addr = addr;

            while (bytes_left > 0) {
                size_t chunk = (bytes_left > NA_MAX_RW_SIZE) ? NA_MAX_RW_SIZE : bytes_left;
                na_cmd cmd;
                memset(&cmd, 0, sizeof(cmd));
                cmd.pid  = (uint32_t)this->pid;
                cmd.addr = (uint64_t)cur_addr;
                cmd.size = (uint32_t)chunk;

                if (prctl(NA_PRCTL_MAGIC, NA_CMD_READ_MEM, (unsigned long)&cmd, 0, 0) != 0) return false;
                if (cmd.result != 0) return false;

                memcpy(dest, cmd.data, chunk);
                dest += chunk;
                cur_addr += chunk;
                bytes_left -= chunk;
            }
            return true;
        }
        return false;
    }

    bool write(uintptr_t addr, const void *buffer, size_t size) {
        if (!active || pid <= 0 || !buffer || size == 0) return false;

        if (engine == KPM_ENGINE_TB) {
            mem_operation op = { this->pid, (uint64_t)addr, const_cast<void*>(buffer), (uint64_t)size };
            return (syscall(__NR_prctl, PRCTL_MEM_WRITE, (unsigned long)&op, 0, 0, 0) >= 0);
        } else if (engine == KPM_ENGINE_NEO) {
            // Chunked loop to completely fix the pewpew 4096-byte limit bug
            const uint8_t *src = static_cast<const uint8_t *>(buffer);
            size_t bytes_left = size;
            uintptr_t cur_addr = addr;

            while (bytes_left > 0) {
                size_t chunk = (bytes_left > NA_MAX_RW_SIZE) ? NA_MAX_RW_SIZE : bytes_left;
                na_cmd cmd;
                memset(&cmd, 0, sizeof(cmd));
                cmd.pid  = (uint32_t)this->pid;
                cmd.addr = (uint64_t)cur_addr;
                cmd.size = (uint32_t)chunk;
                memcpy(cmd.data, src, chunk);

                if (prctl(NA_PRCTL_MAGIC, NA_CMD_WRITE_MEM, (unsigned long)&cmd, 0, 0) != 0) return false;
                if (cmd.result != 0) return false;

                src += chunk;
                cur_addr += chunk;
                bytes_left -= chunk;
            }
            return true;
        }
        return false;
    }

    uintptr_t get_module_base(const char *name) {
        if (pid <= 0 || !name) return 0;
        char maps_path[64];
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", this->pid);
        FILE *f = fopen(maps_path, "r");
        if (!f) return 0;
        char line[512];
        uintptr_t first_base = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, name)) {
                uint64_t start = 0, end = 0;
                if (sscanf(line, "%lx-%lx", &start, &end) == 2 && start > 0) {
                    if (first_base == 0) first_base = static_cast<uintptr_t>(start);
                    uint32_t magic = 0;
                    if (this->read(static_cast<uintptr_t>(start), &magic, sizeof(magic)) && magic == 0x464C457F) {
                        fclose(f);
                        return static_cast<uintptr_t>(start);
                    }
                }
            }
        }
        fclose(f);
        return first_base;
    }

    void hide_process() {
        prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
    }

    void close_driver() {
        active = false;
        engine = KPM_ENGINE_NONE;
    }
};
