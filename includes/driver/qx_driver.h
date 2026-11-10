#pragma once

#include "driver_config.h"

// ============================================================
// Driver Mode 4: QX10~11.4 (Automated Dynamic Major Mknod Driver)
// ============================================================
class c_qx_driver {
private:
    int fd = -1;
    pid_t pid = -1;

    static char* execCom(const char *shell) {
        FILE *fp = popen(shell, "r");
        if (fp == NULL) {
            return NULL;
        }
        char buffer[256];
        char *result = (char *)malloc(1024);
        if (!result) { pclose(fp); return NULL; }
        result[0] = '\0';
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            strncat(result, buffer, 1023 - strlen(result));
        }
        pclose(fp);
        return result;
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
        std::string command = "mknod " + std::string(path) + " c " + std::to_string(major_number) + " " + std::to_string(minor_number);
        system(command.c_str());
    }

    static void removeDeviceNode(const char *path) {
        unlink(path);
    }

public:
    c_qx_driver() : fd(-1), pid(-1) {}

    bool init() {
        char *output = execCom("ls -l /proc/*/exe 2>/dev/null | grep -E \"/data/[a-z]{6} \\(deleted\\)\"");
        char filePath[256] = {0};
        char target_pid[56] = {0};

        if (output != NULL && strlen(output) > 0) {
            char *procStart = strstr(output, "/proc/");
            if (procStart == NULL) {
                printf("\033[41m\033[37m[FATAL] QX driver not found! Exiting...\033[0m\n");
                free(output);
                return false;
            }
            char *pidStart = procStart + 6;
            char *pidEnd = strchr(pidStart, '/');
            if (pidEnd == NULL) {
                printf("\033[41m\033[37m[FATAL] QX driver not found! Exiting...\033[0m\n");
                free(output);
                return false;
            }
            size_t pidLen = pidEnd - pidStart;
            if (pidLen >= sizeof(target_pid)) pidLen = sizeof(target_pid) - 1;
            strncpy(target_pid, pidStart, pidLen);
            target_pid[pidLen] = '\0';

            char *arrowStart = strstr(output, "->");
            if (arrowStart == NULL) {
                printf("\033[41m\033[37m[FATAL] QX driver not found! Exiting...\033[0m\n");
                free(output);
                return false;
            }
            char *start = arrowStart + 3;
            char *end = strchr(output, '(');
            if (end == NULL) {
                printf("\033[41m\033[37m[FATAL] QX driver not found! Exiting...\033[0m\n");
                free(output);
                return false;
            }
            end = end - 1;
            size_t fpLen = end - start + 1;
            if (fpLen >= sizeof(filePath)) fpLen = sizeof(filePath) - 1;
            strncpy(filePath, start, fpLen);
            filePath[fpLen] = '\0';

            char *replacePtr = strstr(filePath, "data");
            if (replacePtr != NULL) {
                memmove(replacePtr + 2, replacePtr + 3, strlen(replacePtr + 3) + 1);
                memmove(replacePtr, "dev", strlen("dev"));
            }
            free(output);
        } else {
            printf("\033[41m\033[37m[FATAL] QX driver not found! Exiting...\033[0m\n");
            if (output) free(output);
            return false;
        }

        char fdPath[256];
        char pattern[100];
        snprintf(pattern, sizeof(pattern), ".*%s.*", filePath + 5);
        int major_number = 0;
        snprintf(fdPath, sizeof(fdPath), "/proc/%s/fd", target_pid);

        regex_t regex;
        if (regcomp(&regex, pattern, 0) != 0) {
            fprintf(stderr, "[FATAL] Failed to compile regex! Exiting...\n");
            return false;
        }

        char result[1024];
        if (findFirstMatchingPath(fdPath, &regex, result)) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "ls -AL -l  %s | grep -Eo '[0-9]{3},' | grep  -Eo '[0-9]{3}'", result);
            char *fdInfo = execCom(cmd);
            if (fdInfo == NULL || strlen(fdInfo) == 0) {
                printf("\033[41m\033[37m[FATAL] Driver not loaded! Exiting...\033[0m\n");
                regfree(&regex);
                if (fdInfo) free(fdInfo);
                return false;
            }
            fdInfo[strlen(fdInfo) - 1] = '\0';
            major_number = atoi(fdInfo);
            free(fdInfo);
        } else {
            printf("\033[41m\033[37m[FATAL] Driver not loaded! Exiting...\033[0m\n");
            regfree(&regex);
            return false;
        }
        regfree(&regex);

        if (filePath[0] != '\0') {
            createDriverNode(filePath, major_number, 0);
            fd = open(filePath, O_RDWR);
            if (fd == -1) {
                removeDeviceNode(filePath);
                printf("\033[41m\033[37m[FATAL] Driver not loaded! Exiting...\033[0m\n");
                return false;
            } else {
                removeDeviceNode(filePath);
            }
        } else {
            printf("\033[41m\033[37m[FATAL] Driver not loaded! Exiting...\033[0m\n");
            return false;
        }

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
