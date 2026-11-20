#pragma once

#include "driver_config.h"
#include <sys/sysmacros.h>

// ============================================================
// Driver Mode 4: QX10~11.4 (Automated Dynamic Major Mknod Driver)
// 100% In-Process Discovery (Zero shell / Zero popen execution)
// ============================================================
class c_qx_driver {
private:
    int fd = -1;
    pid_t pid = -1;

    static bool find_deleted_qx_target(char *filePath, size_t fpSize, char *target_pid, size_t pidSize) {
        DIR *dir = opendir("/proc");
        if (!dir) return false;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!isdigit(entry->d_name[0])) continue;
            char exeLink[256];
            snprintf(exeLink, sizeof(exeLink), "/proc/%s/exe", entry->d_name);
            char target[512];
            ssize_t len = readlink(exeLink, target, sizeof(target) - 1);
            if (len > 0) {
                target[len] = '\0';
                if (strstr(target, "/data/") && strstr(target, "(deleted)")) {
                    strncpy(target_pid, entry->d_name, pidSize - 1);
                    target_pid[pidSize - 1] = '\0';
                    char *start = strstr(target, "/data/");
                    char *end = strstr(target, " (deleted)");
                    if (start && end && end > start) {
                        size_t plen = end - start;
                        if (plen >= fpSize) plen = fpSize - 1;
                        strncpy(filePath, start, plen);
                        filePath[plen] = '\0';
                        char *rep = strstr(filePath, "data");
                        if (rep) {
                            memmove(rep + 2, rep + 3, strlen(rep + 3) + 1);
                            memmove(rep, "dev", strlen("dev"));
                        }
                        closedir(dir);
                        return true;
                    }
                }
            }
        }
        closedir(dir);
        return false;
    }

    static int findFirstMatchingPath(const char *path, regex_t *regex, char *result) {
        DIR *dir = opendir(path);
        if (dir != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
                if (entry->d_type == DT_LNK) {
                    char linkpath[1024];
                    ssize_t len = readlink(fullpath, linkpath, sizeof(linkpath) - 1);
                    if (len != -1) {
                        linkpath[len] = '\0';
                        if (regexec(regex, linkpath, 0, NULL, 0) == 0) {
                            strncpy(result, fullpath, 1023);
                            result[1023] = '\0';
                            closedir(dir);
                            return 1;
                        }
                    }
                }
            }
            closedir(dir);
        }
        return 0;
    }

    static void createDriverNode(const char *path, int major_number, int minor_number) {
        dev_t dev = makedev(major_number, minor_number);
        mknod(path, S_IFCHR | 0666, dev);
    }

    static void removeDeviceNode(const char *path) {
        unlink(path);
    }

public:
    c_qx_driver() : fd(-1), pid(-1) {}

    bool init(bool verbose = true) {
        char filePath[256] = {0};
        char target_pid[56] = {0};

        if (!find_deleted_qx_target(filePath, sizeof(filePath), target_pid, sizeof(target_pid))) {
            if (verbose) {
                printf("[QX] Driver process target not detected\n");
            }
            return false;
        }

        char fdPath[256];
        char pattern[100];
        snprintf(pattern, sizeof(pattern), ".*%s.*", filePath + 5);
        int major_number = 0;
        snprintf(fdPath, sizeof(fdPath), "/proc/%s/fd", target_pid);

        regex_t regex;
        if (regcomp(&regex, pattern, 0) != 0) {
            if (verbose) {
                fprintf(stderr, "[QX] Failed to compile regex\n");
            }
            return false;
        }

        char result[1024];
        if (findFirstMatchingPath(fdPath, &regex, result)) {
            struct stat st;
            if (stat(result, &st) == 0 && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))) {
                major_number = major(st.st_rdev);
            }
            if (major_number <= 0) {
                if (lstat(result, &st) == 0 && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))) {
                    major_number = major(st.st_rdev);
                }
            }
            if (major_number <= 0) {
                if (verbose) {
                    printf("[QX] Driver major number resolution failed\n");
                }
                regfree(&regex);
                return false;
            }
        } else {
            if (verbose) {
                printf("[QX] No matching fd found for target\n");
            }
            regfree(&regex);
            return false;
        }
        regfree(&regex);

        if (filePath[0] != '\0') {
            createDriverNode(filePath, major_number, 0);
            fd = open(filePath, O_RDWR);
            if (fd == -1) {
                removeDeviceNode(filePath);
                if (verbose) {
                    printf("[QX] Unable to open created device node\n");
                }
                return false;
            } else {
                removeDeviceNode(filePath);
            }
        } else {
            if (verbose) {
                printf("[QX] Empty device node target path\n");
            }
            return false;
        }

        printf("[+] Driver connected: QX 10-11\n");
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
