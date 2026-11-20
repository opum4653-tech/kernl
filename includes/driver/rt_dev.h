#pragma once

#include "driver_config.h"

// ============================================================
// Driver Mode 2: RT DEV (Character Device Direct Interface)
// Verified binding with NinMods Driver Loader (RT Module)
// ============================================================
class c_rt_dev {
private:
    int fd = -1;
    pid_t pid = -1;
    uint32_t active_read_op = DEV_OP_READ_MEM;
    uint32_t active_write_op = DEV_OP_WRITE_MEM;
    uint32_t active_module_op = DEV_OP_MODULE_BASE;

    static char* scan_driver_path() {
        // Priority 1: Direct known node validation deployed by NinMods loader
        const char* known_nodes[] = {
            "/dev/BYNb",
            "/dev/wanbai",
            "/dev/mem_tool",
            "/dev/nh",
            "/dev/cdev",
            "/dev/entryi",
            "/dev/RTkernel",
            "/dev/RT_Monitor",
            "/dev/suvhwvwrtnb",
            "/dev/rtnb",
            "/dev/RTdrivers",
            "/dev/CheckMe",
            "/dev/Ckanri",
            "/dev/lanran",
            "/dev/video188",
            "/dev/facking",
            "/dev/YBBBYN",
            "/dev/sched_debug",
            "/dev/health_storage_service_alloc"
        };
        for (const char* node : known_nodes) {
            struct stat st;
            if (stat(node, &st) == 0 && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))) {
                int test_fd = open(node, O_RDWR);
                if (test_fd >= 0) {
                    close(test_fd);
                    return strdup(node);
                }
            }
        }

        // Priority 2: Scans /dev for any character devices matching RT / driver patterns
        const char *dev_path = "/dev";
        DIR *dir = opendir(dev_path);
        if (dir == NULL) return NULL;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            if (strstr(entry->d_name, "gpiochip") != NULL || strstr(entry->d_name, "std") != NULL) continue;

            bool is_rt_name = (strncmp(entry->d_name, "RT", 2) == 0 || strncmp(entry->d_name, "rt", 2) == 0);

            char file_path[256];
            snprintf(file_path, sizeof(file_path), "%s/%s", dev_path, entry->d_name);

            struct stat file_info;
            if (stat(file_path, &file_info) < 0) continue;

            if (S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode)) {
                if (file_info.st_uid == 0 && file_info.st_gid == 0) {
                    if (is_rt_name || (strlen(entry->d_name) >= 5 && strlen(entry->d_name) <= 12)) {
                        int test_fd = open(file_path, O_RDWR);
                        if (test_fd >= 0) {
                            close(test_fd);
                            closedir(dir);
                            return strdup(file_path);
                        }
                    }
                }
            }
        }
        closedir(dir);
        return NULL;
    }

    void send_unlock_keys() {
        if (fd < 0) return;
        const char* keys[] = { "123456", "wanbai", "CheckMe", "lanran", "Ckanri", "ninmods_kernel", "" };
        const uint32_t init_ops[] = { DEV_OP_INIT_KEY, PROC_OP_INIT_KEY, 600 };
        for (uint32_t op : init_ops) {
            for (const char* k : keys) {
                char buf[0x100] = {0};
                strncpy(buf, k, sizeof(buf) - 1);
                ioctl(fd, op, buf);
            }
        }
    }

public:
    c_rt_dev() : fd(-1), pid(-1) {}

    bool init(bool verbose = true) {
        char *device_name = scan_driver_path();
        if (device_name == NULL) {
            if (verbose) {
                printf("[RT DEV] Driver node not found in /dev\n");
            }
            return false;
        }
        fd = open(device_name, O_RDWR);
        if (fd == -1) {
            if (verbose) {
                printf("[RT DEV] Cannot open device: %s\n", device_name);
            }
            free(device_name);
            return false;
        }
        printf("[+] Driver connected: RT DEV (%s)\n", device_name);
        free(device_name);

        send_unlock_keys();
        return true;
    }

    void set_pid(pid_t p) {
        this->pid = p;
    }

    bool is_loaded() const {
        return fd >= 0;
    }

    bool init_key(char* key) {
        if (fd < 0) return false;
        send_unlock_keys();
        if (key && strlen(key) > 0) {
            char buf[0x100] = {0};
            strncpy(buf, key, sizeof(buf) - 1);
            ioctl(fd, DEV_OP_INIT_KEY, buf);
        }
        return true;
    }

    bool read(uintptr_t addr, void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;

        if (ioctl(fd, active_read_op, &cm) == 0) return true;

        static const uint32_t candidate_ops[] = { DEV_OP_READ_MEM, HOOK_OP_READ_MEM, PROC_OP_READ_MEM };
        for (uint32_t op : candidate_ops) {
            if (op == active_read_op) continue;
            if (ioctl(fd, op, &cm) == 0) {
                active_read_op = op;
                return true;
            }
        }
        return false;
    }

    bool write(uintptr_t addr, void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;

        if (ioctl(fd, active_write_op, &cm) == 0) return true;

        static const uint32_t candidate_ops[] = { DEV_OP_WRITE_MEM, HOOK_OP_WRITE_MEM, PROC_OP_WRITE_MEM };
        for (uint32_t op : candidate_ops) {
            if (op == active_write_op) continue;
            if (ioctl(fd, op, &cm) == 0) {
                active_write_op = op;
                return true;
            }
        }
        return false;
    }

    uintptr_t get_module_base(const char* name) {
        if (fd < 0 || pid <= 0 || !name) return 0;
        static const uint32_t mod_ops[] = { DEV_OP_MODULE_BASE, HOOK_OP_MODULE_BASE, PROC_OP_MODULE_BASE };
        for (uint32_t op : mod_ops) {
            MODULE_BASE mb;
            memset(&mb, 0, sizeof(mb));
            char buf[0x100] = {0};
            strncpy(buf, name, sizeof(buf) - 1);
            mb.pid = this->pid;
            mb.name = buf;
            mb.base = 0;
            if (ioctl(fd, op, &mb) == 0 && mb.base > 0) {
                uint32_t magic = 0;
                if (this->read(mb.base, &magic, sizeof(magic)) && magic == 0x464C457F) {
                    active_module_op = op;
                    return mb.base;
                }
            }
        }

        // Direct /proc/<pid>/maps scan fallback (zero-fork, zero popen)
        char maps_path[64];
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", this->pid);
        FILE *f = fopen(maps_path, "r");
        if (f) {
            char line[1024];
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
            if (first_base > 0) return first_base;
        }
        return 0;
    }

    void hide_process() {
        if (fd >= 0) {
            ioctl(fd, active_module_op == HOOK_OP_MODULE_BASE ? HOOK_OP_HIDE_PROCESS : DEV_OP_HIDE_PROCESS);
        }
    }

    void close_driver() {
        if (fd > 0) {
            close(fd);
            fd = -1;
        }
    }
};
