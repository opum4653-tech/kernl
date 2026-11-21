#pragma once

#include "driver_config.h"

// ============================================================
// Driver Mode 3: DEV (Most Compatible Random Device Scanner)
// ============================================================
class c_dev_compat {
private:
    int fd = -1;
    pid_t pid = -1;
    uint32_t active_read_op = DEV_OP_READ_MEM;
    uint32_t active_write_op = DEV_OP_WRITE_MEM;
    uint32_t active_module_op = DEV_OP_MODULE_BASE;

    static bool verify_test_read(int test_fd, uint32_t& out_read, uint32_t& out_write) {
        if (test_fd < 0) return false;

        // Step 1: Send initialization / unlock keys & bind test PID
        const char* keys[] = { "123456", "wanbai", "CheckMe", "lanran", "Ckanri", "ninmods_kernel", "" };
        const uint32_t init_ops[] = { DEV_OP_INIT_KEY, RT_IOCTL_INIT, 0x8000, PROC_OP_INIT_KEY, 600 };
        for (uint32_t op : init_ops) {
            for (const char* k : keys) {
                char buf[0x100] = {0};
                strncpy(buf, k, sizeof(buf) - 1);
                ioctl(test_fd, op, buf);
            }
        }

        pid_t self_pid = getpid();
        ioctl(test_fd, RT_IOCTL_INIT, self_pid);
        ioctl(test_fd, RT_IOCTL_INIT, &self_pid);
        ioctl(test_fd, 0x8004, self_pid);
        ioctl(test_fd, 0x8004, &self_pid);
        ioctl(test_fd, DEV_OP_INIT_KEY, self_pid);

        volatile uint32_t test_magic = 0x5A5A1234;
        uint32_t read_buf = 0;

        COPY_MEMORY cm;
        cm.pid = getpid();
        cm.addr = reinterpret_cast<uintptr_t>(&test_magic);
        cm.buffer = &read_buf;
        cm.size = sizeof(test_magic);

        struct OpPair {
            uint32_t r;
            uint32_t w;
        };
        static const OpPair pairs[] = {
            { DEV_OP_READ_MEM,   DEV_OP_WRITE_MEM   }, // 0x801 / 0x802
            { RT_IOCTL_READ,     RT_IOCTL_WRITE     }, // 0x8001 / 0x8002
            { HOOK_OP_READ_MEM,  HOOK_OP_WRITE_MEM  }, // 601 / 602
            { PROC_OP_READ_MEM,  PROC_OP_WRITE_MEM  }, // 0xC01 / 0xC02
            { 0x400,             0x200              }  // Alt KPM
        };

        for (const auto& p : pairs) {
            read_buf = 0;
            if (ioctl(test_fd, p.r, &cm) == 0 && read_buf == test_magic) {
                out_read = p.r;
                out_write = p.w;
                return true;
            }
        }

        return false;
    }

    static char* scan_compat_dev(uint32_t& out_read, uint32_t& out_write) {
        // Priority 1: Direct known node validation deployed by NinMods loader
        const char* known_nodes[] = {
            "/dev/BYNb",
            "/dev/wanbai",
            "/dev/mem_tool",
            "/dev/nh",
            "/dev/entryi",
            "/dev/RTkernel",
            "/dev/RT_Monitor",
            "/dev/suvhwvwrtnb",
            "/dev/rtnb",
            "/dev/RTdrivers"
        };
        for (const char* node : known_nodes) {
            struct stat st;
            if (stat(node, &st) == 0 && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))) {
                int test_fd = open(node, O_RDWR);
                if (test_fd >= 0) {
                    if (verify_test_read(test_fd, out_read, out_write)) {
                        close(test_fd);
                        return strdup(node);
                    }
                    close(test_fd);
                }
            }
        }

        const char *dev_path = "/dev";
        DIR *dir = opendir(dev_path);
        if (dir == NULL) {
            return NULL;
        }

        struct dirent *entry;
        char file_path[256];
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, "std") != NULL || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strstr(entry->d_name, "gpiochip") != NULL) {
                continue;
            }

            if (strchr(entry->d_name, '_') != NULL || strchr(entry->d_name, '-') != NULL || strchr(entry->d_name, ':') != NULL) {
                continue;
            }

            snprintf(file_path, sizeof(file_path), "%s/%s", dev_path, entry->d_name);

            struct stat file_info;
            if (stat(file_path, &file_info) < 0)
                continue;

            struct tm* tm_ctime = localtime(&file_info.st_ctime);
            int file_year = tm_ctime ? (tm_ctime->tm_year + 1900) : 0;
            time_t now_ts = time(nullptr);
            struct tm* tm_now = localtime(&now_ts);
            int cur_year = tm_now ? (tm_now->tm_year + 1900) : 2026;
            if (cur_year > 1980 && file_year > 0 && file_year <= 1980)
                continue;

            if (strlen(entry->d_name) > 16 || strlen(entry->d_name) < 4)
                continue;

            if (file_info.st_gid != 0 || file_info.st_uid != 0)
                continue;

            if (S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode)) {
                int test_fd = open(file_path, O_RDWR);
                if (test_fd >= 0) {
                    if (verify_test_read(test_fd, out_read, out_write)) {
                        close(test_fd);
                        char *devpath = strdup(file_path);
                        closedir(dir);
                        return devpath;
                    }
                    close(test_fd);
                }
            }
        }
        closedir(dir);
        return NULL;
    }

    void send_unlock_keys() {
        if (fd < 0) return;
        const char* keys[] = { "123456", "wanbai", "CheckMe", "lanran", "Ckanri", "ninmods_kernel", "" };
        const uint32_t init_ops[] = { DEV_OP_INIT_KEY, RT_IOCTL_INIT, 0x8000, PROC_OP_INIT_KEY, 600 };
        for (uint32_t op : init_ops) {
            for (const char* k : keys) {
                char buf[0x100] = {0};
                strncpy(buf, k, sizeof(buf) - 1);
                ioctl(fd, op, buf);
            }
        }
    }

public:
    c_dev_compat() : fd(-1), pid(-1) {}

    bool init(bool verbose = true) {
        uint32_t read_op = DEV_OP_READ_MEM;
        uint32_t write_op = DEV_OP_WRITE_MEM;
        char *device_name = scan_compat_dev(read_op, write_op);
        if (device_name == NULL) {
            if (verbose) {
                printf("[DEV COMPAT] Verified compatible device node not found in /dev\n");
            }
            return false;
        }
        fd = open(device_name, O_RDWR);
        if (fd == -1) {
            if (verbose) {
                printf("[DEV COMPAT] Cannot open verified device: %s\n", device_name);
            }
            free(device_name);
            return false;
        }
        active_read_op = read_op;
        active_write_op = write_op;
        printf("[+] Driver connected & verified: DEV COMPAT (%s) [read_op: 0x%X]\n", device_name, active_read_op);
        free(device_name);

        send_unlock_keys();
        return true;
    }

    void set_pid(pid_t p) {
        if (this->pid != p) {
            this->pid = p;
            if (fd >= 0 && p > 0) {
                ioctl(fd, RT_IOCTL_INIT, p);
                ioctl(fd, RT_IOCTL_INIT, &p);
                ioctl(fd, 0x8004, p);
                ioctl(fd, 0x8004, &p);
                ioctl(fd, DEV_OP_INIT_KEY, p);
            }
        }
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
            ioctl(fd, RT_IOCTL_INIT, buf);
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

        static const uint32_t candidate_ops[] = { DEV_OP_READ_MEM, RT_IOCTL_READ, HOOK_OP_READ_MEM, PROC_OP_READ_MEM, 0x400 };
        for (uint32_t op : candidate_ops) {
            if (op == active_read_op) continue;
            if (ioctl(fd, op, &cm) == 0) {
                active_read_op = op;
                return true;
            }
        }
        return false;
    }

    bool write(uintptr_t addr, const void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = const_cast<void*>(buffer);
        cm.size = size;

        if (ioctl(fd, active_write_op, &cm) == 0) return true;

        static const uint32_t candidate_ops[] = { DEV_OP_WRITE_MEM, RT_IOCTL_WRITE, HOOK_OP_WRITE_MEM, PROC_OP_WRITE_MEM, 0x200 };
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

        // Direct /proc/<pid>/maps fallback (zero-fork, zero popen)
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
