// ninmods-kernel — PURE KERNEL DRIVER ARCHITECTURE (EXTERNAL ARCHITECTURE PARITY)
#include "Android_draw/draw.h"
#include "ui/menu.hpp"
#include "protect/license.hpp"
#include "other/memory.hpp"
#include "other/crash_handler.hpp"
#include "other/display_manager.hpp"
#include "other/logger.hpp"
#include "game/data.hpp"
#include "game/Offsets.h"
#include "func/esp.hpp"
#include "driver.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <android/log.h>
#include <iostream>
#include <string>
#include <atomic>

#define LOG_TAG "ninmods-kernel"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

extern float global_anim_time;
std::atomic<bool> main_thread_flag{true};

enum TargetGame {
    GAME_FF     = 0,
    GAME_FF_MAX = 1
};

static TargetGame g_targetGame = GAME_FF;

static const char* kFFActivity    = "com.dts.freefireth/com.dts.freefireth.FFMainActivity";
static const char* kFFMaxActivity = "com.dts.freefiremax/com.dts.freefiremax.FFMainActivity";
static const char* kFFPackage     = "com.dts.freefireth";
static const char* kFFMaxPackage  = "com.dts.freefiremax";

static const char* GetTargetActivity() {
    return (g_targetGame == GAME_FF) ? kFFActivity : kFFMaxActivity;
}
static const char* GetTargetPackage() {
    return (g_targetGame == GAME_FF) ? kFFPackage : kFFMaxPackage;
}

static void print_dashboard() {
    time_t exp = lic::expiry_date();
    struct tm* tm_info = gmtime(&exp);
    char exp_buf[64] = {0};
    if (tm_info) {
        strftime(exp_buf, sizeof(exp_buf), "%Y-%m-%d %H:%M:%S UTC", tm_info);
    } else {
        snprintf(exp_buf, sizeof(exp_buf), "2026-11-07 00:00:00 UTC");
    }

    printf("====================================================\n");
    printf("   NinMods Kernel Overlay Engine v2.0 (ARM64)\n");
    printf("====================================================\n");
    printf("Environment    : Android Linux aarch64 (Kernel Driver Safe)\n");
    printf("API Key Status : Valid / Active (%s)\n", lic::status_text());
    printf("Token Expiry   : %s\n", exp_buf);
    printf("Driver Status  : %s\n", (driver && driver->is_driver_loaded()) ? "Loaded / Active" : "Armed (Awaiting Node)");
    printf("Runtime Status : Ready | Kernel DMA Pipeline\n");
    printf("====================================================\n\n");
    fflush(stdout);
}

static bool check_license_gate(int argc, char** argv) {
    lic::init();

    printf("[*] NinMods Kernel Overlay — Authentication Gate\n");
    printf("[*] Validating session key...\n");

    for (int i = 1; i < argc; i++) {
        if (argv[i] && strlen(argv[i]) >= 8 && strcmp(argv[i], "ffmax") != 0 && strcmp(argv[i], "START") != 0) {
            if (lic::set_key(argv[i]) && lic::is_valid()) {
                printf("[+] Pre-authorized via CLI argument.\n");
                printf("[+] Key validated. Overlay authorized.\n\n");
                fflush(stdout);
                return true;
            }
        }
    }

    const char* env_key = getenv("NIN_LICENSE_KEY");
    if (env_key && strlen(env_key) > 0) {
        if (lic::set_key(env_key) && lic::is_valid()) {
            printf("[+] Pre-authorized via secure environment session.\n");
            printf("[+] Key validated. Overlay authorized.\n\n");
            fflush(stdout);
            return true;
        }
    }

    printf("[?] Enter License / Access Key: ");
    fflush(stdout);

    char key_input[128] = {0};
    if (fgets(key_input, sizeof(key_input), stdin)) {
        size_t len = strlen(key_input);
        while (len > 0 && (key_input[len - 1] == '\n' || key_input[len - 1] == '\r' || key_input[len - 1] == ' ' || key_input[len - 1] == '\t')) {
            key_input[len - 1] = '\0';
            len--;
        }
    }

    bool key_ok = false;
    if (strlen(key_input) > 0) {
        key_ok = lic::set_key(key_input);
    } else if (lic::is_valid()) {
        key_ok = true;
    }

    if (!key_ok || !lic::is_valid()) {
        printf("[-] Authentication failed: Invalid or expired token.\n");
        fflush(stdout);
        return false;
    }

    printf("[+] Key validated successfully. Overlay authorized.\n");

    if (isatty(fileno(stdin))) {
        printf("[+] Interactive terminal detected. Launching overlay...\n\n");
        fflush(stdout);
        return true;
    }

    printf("[?] Ready to load into memory. Awaiting START signal...\n");
    fflush(stdout);

    char start_input[64] = {0};
    if (fgets(start_input, sizeof(start_input), stdin)) {
    }

    printf("[+] Launching overlay...\n\n");
    fflush(stdout);
    return true;
}

static int start_overlay() {
    LOGI("=== ninmods-kernel starting ===");
    printf("[*] Probing display dimensions and screen orientation...\n");
    fflush(stdout);

    display_safe::probe_orientation();
    screen_config();

    int max_size = (displayInfo.height > displayInfo.width)
                   ? displayInfo.height : displayInfo.width;
    int min_size = (displayInfo.height < displayInfo.width)
                   ? displayInfo.height : displayInfo.width;

    g_sw = static_cast<float>(max_size);
    g_sh = static_cast<float>(min_size);
    native_window_screen_x = max_size;
    native_window_screen_y = min_size;

    printf("[*] Screen dimensions: %dx%d (Orientation: %u)\n", native_window_screen_x, native_window_screen_y, displayInfo.orientation);
    printf("[*] Configuring Android system touch security policies...\n");
    fflush(stdout);

    system("settings put global block_untrusted_touches 0 2>/dev/null; settings put system block_untrusted_touches 0 2>/dev/null");

    printf("[*] Creating SurfaceComposerClient Native Window and initializing EGL...\n");
    fflush(stdout);

    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true, false)) {
        printf("[-] Failed to initialize overlay GUI (NativeWindow or EGL error)\n");
        fflush(stdout);
        return -1;
    }
    printf("[+] Floating overlay engine initialized successfully!\n");
    printf("[*] Attaching Linux input touch listener...\n");
    fflush(stdout);

    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, true);

    if (driver && driver->connect_driver()) {
        printf("[+] Kernel driver connected successfully.\n");
    } else {
        printf("[*] Kernel driver node not found yet. Auto-probe armed for attach.\n");
    }
    fflush(stdout);

    std::thread attach_worker([]() {
        LOG_INFO("Kernel target attach daemon thread launched. Polling game process and orientation.");
        while (main_thread_flag) {
            display_safe::probe_orientation();
            proc::pid = get_pid();
            if (proc::pid > 0) {
                if (driver && driver->is_driver_loaded()) {
                    driver->initialize(static_cast<pid_t>(proc::pid));
                    proc::lib = get_lib();
                    if (proc::lib > 0) {
                        uint32_t elfMagic = rpm<uint32_t>(proc::lib);
                        if (elfMagic == 0x464C457F) {
                            data::thread_start();
                            LOG_INFO("Kernel subsystem attached (PID: %d, Lib: 0x%lx)", proc::pid, (unsigned long)proc::lib);
                            printf("[+] Kernel subsystem attached (PID: %d, Lib: 0x%lx)\n", proc::pid, (unsigned long)proc::lib);
                            fflush(stdout);

                            while (main_thread_flag) {
                                if (get_pid() <= 0) {
                                    data::thread_stop();
                                    LOG_INFO("Session detached (PID exited). Waiting for reconnect...");
                                    printf("[*] Session detached. Waiting for reconnect...\n");
                                    fflush(stdout);
                                    break;
                                }
                                display_safe::probe_orientation();
                                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            }
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
    attach_worker.detach();

    printf("[+] NinMods Kernel Overlay active in RAM. Processing render stream.\n\n");
    fflush(stdout);

    int dbgCounter = 0;
    while (main_thread_flag) {
        global_anim_time += 0.016f;
        drawBegin();
        ui::menu::render();
        drawEnd();
        usleep(16000);

        if (++dbgCounter >= 300) {
            dbgCounter = 0;
            bool inM = data::inMatch.load(std::memory_order_relaxed);
            bool drvOk = (driver && driver->is_driver_loaded());
            printf("[kernel-overlay] driver: %s | daemon status: %s | objects: active\n",
                   drvOk ? "connected" : "waiting",
                   inM ? "in-match" : "idle");
            fflush(stdout);
        }
    }

    shutdown();
    Touch_Close();
    data::thread_stop();
    return 0;
}

static void mask_process_name(int argc, char** argv, const char* fake_name) {
    if (!argv || !argv[0] || !fake_name) return;
    prctl(PR_SET_NAME, fake_name, 0, 0, 0);
    size_t len = strlen(argv[0]);
    if (len > 0) {
        memset(argv[0], 0, len);
        strncpy(argv[0], fake_name, len);
    }
}

int main(int argc, char** argv) {
    crash_guard::install();

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    mask_process_name(argc, argv, "ninmods-kernel");

    if (argc >= 2) {
        if (strcmp(argv[1], "ffmax") == 0) {
            g_targetGame = GAME_FF_MAX;
        }
    }

    if (!check_license_gate(argc, argv)) {
        return 1;
    }

    print_dashboard();

    return start_overlay();
}
