#pragma once

#include "driver_config.h"

// ============================================================
// Driver Mode 3: DEV (Most Compatible Random Device Scanner)
// ============================================================
class c_dev_compat {
private:
    int fd = -1;
    pid_t pid = -1;

    static char* scan_compat_dev() {
        const char *dev_path = "/dev";
        DIR *dir = opendir(dev_path);
        if (dir == NULL) {
            printf("无法打开/dev目录\n");
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

            if (strlen(entry->d_name) > 7 || strlen(entry->d_name) < 5)
                continue;

            if (file_info.st_gid != 0 || file_info.st_uid != 0)
                continue;

            if (S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode)) {
                if (file_info.st_gid == 0 && file_info.st_uid == 0) {
                    printf("%s\n", file_path);
                    char *devpath = strdup(file_path);
                    closedir(dir);
                    return devpath;
                }
            }
        }
        closedir(dir);
        return NULL;
    }

public:
    c_dev_compat() : fd(-1), pid(-1) {}

    bool init() {
        char *device_name = scan_compat_dev();
        if (device_name == NULL) {
            printf("\033[41m\033[37m[FATAL] DEV driver not found! Exiting...\033[0m\n");
            return false;
        }
        fd = open(device_name, O_RDWR);
        if (fd == -1) {
            printf("\033[41m\033[37m[FATAL] Driver not loaded! Exiting...\033[0m\n");
            free(device_name);
            return false;
        }
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
