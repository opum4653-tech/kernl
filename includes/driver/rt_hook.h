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

    bool init() {
        fd = 0;
        printf("\033[41m\033[37mRT loaded\033[0m\n");
        return true;
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

    bool write(uintptr_t addr, void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return (ioctl(fd, HOOK_OP_WRITE_MEM, &cm) == 0);
    }

    uintptr_t get_module_base(const char* name) {
        if (pid <= 0 || !name) return 0;
        FILE* fp = nullptr;
        char cmd[0x100] = "";
        uintptr_t ret = 0;
        snprintf(cmd, sizeof(cmd), "ls -l /proc/%d/map_files/ | grep '%s'", pid, name);
        fp = popen(cmd, "r");
        if (!fp) {
            std::cerr << "Failed to run command" << std::endl;
            return 0;
        }
        fscanf(fp, "%*s %*d %*s %*s %*d %*s %*s %lx-%*lx", &ret);
        pclose(fp);
        return ret;
    }

    void hide_process() {
        if (fd >= 0) {
            ioctl(fd, HOOK_OP_HIDE_PROCESS);
        }
    }

    void close_driver() {
        if (fd > 0) {
            close(fd);
            fd = -1;
        }
    }
};
