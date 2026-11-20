#pragma once

#include "driver_config.h"

// ============================================================
// Driver Mode 2: RT DEV (Character Device Direct Interface)
// ============================================================
class c_rt_dev {
private:
    int fd = -1;
    pid_t pid = -1;

    static char* scan_driver_path() {
        // Priority 1: Direct known node validation deployed by NinMods loader
        const char* known_nodes[] = {
            "/dev/RTkernel",
            "/dev/RT_Monitor",
            "/dev/wanbai",
            "/dev/BYNb",
            "/dev/mem_tool",
            "/dev/nh",
            "/dev/suvhwvwrtnb",
            "/dev/rtnb",
            "/dev/RTdrivers"
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
        return true;
    }

    void set_pid(pid_t p) {
        this->pid = p;
    }

    bool is_loaded() const {
        return fd >= 0;
    }

    bool init_key(char* key) {
        if (fd < 0 || !key) return false;
        char buf[0x100];
        strncpy(buf, key, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return (ioctl(fd, DEV_OP_INIT_KEY, buf) == 0);
    }

    bool read(uintptr_t addr, void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return (ioctl(fd, DEV_OP_READ_MEM, &cm) == 0);
    }

    bool write(uintptr_t addr, void* buffer, size_t size) {
        if (fd < 0 || pid <= 0 || !buffer || size == 0) return false;
        COPY_MEMORY cm;
        cm.pid = this->pid;
        cm.addr = addr;
        cm.buffer = buffer;
        cm.size = size;
        return (ioctl(fd, DEV_OP_WRITE_MEM, &cm) == 0);
    }

    uintptr_t get_module_base(const char* name) {
        if (fd < 0 || pid <= 0 || !name) return 0;
        MODULE_BASE mb;
        char buf[0x100];
        strncpy(buf, name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        mb.pid = this->pid;
        mb.name = buf;
        mb.base = 0;
        if (ioctl(fd, DEV_OP_MODULE_BASE, &mb) != 0) {
            return 0;
        }
        return mb.base;
    }

    void hide_process() {
        if (fd >= 0) {
            ioctl(fd, DEV_OP_HIDE_PROCESS);
        }
    }

    void close_driver() {
        if (fd > 0) {
            close(fd);
            fd = -1;
        }
    }
};
