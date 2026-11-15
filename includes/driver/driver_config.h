#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <regex.h>
#include <sys/uio.h>
#include <iostream>
#include <string>
#include <sys/utsname.h>
#include <sys/system_properties.h>
#include <sys/prctl.h>
#include <fstream>

inline int 选择值 = 0;
inline int 隐藏 = 0;

struct AppConfig {
    int driverOption = 0;
    int captureMode = 0;
    bool settingsLoaded = false;
};

static AppConfig g_appConfig;

inline void SaveConfig() {
}

inline bool LoadConfig() {
    g_appConfig.settingsLoaded = true;
    return true;
}

inline void DisplayCurrentConfig() {
    if (g_appConfig.settingsLoaded) {
        const char* driverNames[] = {
            "Unknown",
            "RT Hook",
            "RT DEV",
            "DEV (Most Compatible)",
            "QX10~11.4",
            "KPM (Neo prctl)"
        };

        const char* captureNames[] = {
            "Unknown",
            "Hidden from recordings/screenshots",
            "Visible everywhere (normal)"
        };

        printf("  | Driver Option : %s\n", driverNames[g_appConfig.driverOption]);
        printf("  | Capture Mode  : %s\n", captureNames[g_appConfig.captureMode]);
    }
}

// ── Shared IOCTL Structures & Opcodes ──────────────────────────
typedef struct _COPY_MEMORY {
    pid_t pid;
    uintptr_t addr;
    void* buffer;
    size_t size;
} COPY_MEMORY, *PCOPY_MEMORY;

typedef struct _MODULE_BASE {
    pid_t pid;
    char* name;
    uintptr_t base;
} MODULE_BASE, *PMODULE_BASE;

enum DRIVER_OPERATIONS {
    HOOK_OP_READ_MEM     = 601,
    HOOK_OP_WRITE_MEM    = 602,
    HOOK_OP_MODULE_BASE  = 603,
    HOOK_OP_HIDE_PROCESS = 605,

    DEV_OP_INIT_KEY      = 0x800,
    DEV_OP_READ_MEM      = 0x801,
    DEV_OP_WRITE_MEM     = 0x802,
    DEV_OP_MODULE_BASE   = 0x803,
    DEV_OP_HIDE_PROCESS  = 0x804,

    PROC_OP_INIT_KEY     = 0xC00,
    PROC_OP_READ_MEM     = 0xC01,
    PROC_OP_WRITE_MEM    = 0xC02,
    PROC_OP_MODULE_BASE  = 0xC03,
    PROC_OP_HIDE_PROCESS = 0xC04,
};
