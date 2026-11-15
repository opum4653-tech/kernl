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
// Stealth storage path isolated in root /data/local/tmp/. Never expose plaintext files on /storage/emulated/0/
static std::string configPath = "/data/local/tmp/.nin_drv_cfg";

inline void SaveConfig() {
    // Clean up any legacy public traces from external storage
    unlink("/storage/emulated/0/VoidHaxConfig.txt");

    std::ofstream file(configPath);
    if (!file.is_open()) {
        return;
    }
    file << "DRIVER_OPTION=" << g_appConfig.driverOption << "\n";
    file << "CAPTURE_MODE=" << g_appConfig.captureMode << "\n";
    file.close();
}

inline bool LoadConfig() {
    // Clean up any legacy public traces from external storage
    unlink("/storage/emulated/0/VoidHaxConfig.txt");

    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) continue;

        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);

        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        if (key == "DRIVER_OPTION") {
            try { g_appConfig.driverOption = std::stoi(value); } catch (...) {}
        } else if (key == "CAPTURE_MODE") {
            try { g_appConfig.captureMode = std::stoi(value); } catch (...) {}
        }
    }

    file.close();

    if (g_appConfig.driverOption < 1 || g_appConfig.driverOption > 5) {
        g_appConfig.driverOption = 0;
        return false;
    }

    if (g_appConfig.captureMode < 1 || g_appConfig.captureMode > 2) {
        g_appConfig.captureMode = 0;
        return false;
    }

    g_appConfig.settingsLoaded = true;
    printf("[Config] Loaded successfully!\n");
    printf("[Config] Driver Option: %d\n", g_appConfig.driverOption);
    printf("[Config] Capture Mode: %d\n", g_appConfig.captureMode);
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
