#pragma once

#include "driver_config.h"

// ============================================================
// Driver Mode 1: RT Hook (Syscall/IOCTL Hook)
// ============================================================
class c_rt_hook {
private:
    int fd = -1;
    pid_t pid = -1;

public:
    c_rt_hook() : fd(-1), pid(-1) {}

    bool init(bool verbose = true) {
        volatile uint32_t test_magic = 0x5A5A1234;
        uint32_t read_buf = 0;
        COPY_MEMORY test_cm;
        test_cm.pid = getpid();
        test_cm.addr = reinterpret_cast<uintptr_t>(&test_magic);
        test_cm.buffer = &read_buf;
        test_cm.size = sizeof(test_magic);
        errno = 0;
        int ret = ioctl(0, HOOK_OP_READ_MEM, &test_cm);
        if (ret == 0 && read_buf == test_magic) {
            fd = 0;
            printf("[+] Driver connected: RT HOOK\n");
            return true;
        }
        if (verbose) {
            printf("[RT HOOK] Syscall hook interface not installed\n");
        }
        return false;
    }

    void set_pid(pid_t p) {
        this->pid = p;
    }

    bool is_loaded() const {
        return fd >= 0;
    }

    bool read(uintptr_t addr, void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return (ioctl(fd, HOOK_OP_READ_MEM, &cm) == 0);
    }

    bool write(uintptr_t addr, const void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = const_cast<void*>(buffer);
        cm.size = size;
        return (ioctl(fd, HOOK_OP_WRITE_MEM, &cm) == 0);
    }

    uintptr_t get_module_base(const char* name) {
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
        if (fd >= 0) {
            ioctl(fd, HOOK_OP_HIDE_PROCESS);
        }
    }

    void close_driver() {
        if (fd > 0) {
            close(fd);
        }
        fd = -1;
    }
};
