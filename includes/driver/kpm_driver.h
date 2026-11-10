#pragma once

#include "driver_config.h"

// ============================================================
// Driver Mode 5: KPM (Neo prctl Kernel Patch Module Backend)
// ============================================================
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

class c_kpm_driver {
private:
    bool active = false;
    pid_t pid = -1;

public:
    c_kpm_driver() : active(false), pid(-1) {}

    bool probe() {
        na_cmd cmd;
        prctl(0xDEAD, 0, 0, 0, 0);
        memset(&cmd, 0, sizeof(cmd));
        errno = 0;
        int r = prctl(NA_PRCTL_MAGIC, NA_CMD_GET_PID, (unsigned long)&cmd, 0, 0);
        active = (r == 0);
        return active;
    }

    bool init() {
        if (probe()) {
            printf("\033[42m\033[37mKPM (Neo) backend active\033[0m\n");
            return true;
        } else {
            printf("\033[41m\033[37m[FATAL] KPM not available on this kernel! Exiting...\033[0m\n");
            return false;
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

    bool write(uintptr_t addr, void *buffer, size_t size) {
        if (!active || pid <= 0 || !buffer || size == 0) return false;

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

    uintptr_t get_module_base(const char *name) {
        if (pid <= 0 || !name) return 0;
        char maps_path[64];
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", this->pid);
        FILE *f = fopen(maps_path, "r");
        if (!f) return 0;
        char line[512];
        uintptr_t base = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, name)) {
                base = (uintptr_t)strtoull(line, nullptr, 16);
                break;
            }
        }
        fclose(f);
        return base;
    }

    void close_driver() {
        active = false;
    }
};
