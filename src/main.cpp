#include "Android_draw/draw.h"
#include "Android_touch/Touch.hpp"
#include "ui/menu.hpp"
#include "protect/license.hpp"
#include "other/memory.hpp"
#include "other/crash_handler.hpp"
#include "other/display_manager.hpp"
#include "other/logger.hpp"
#include "driver.h"
#include "FONTS/MAIN_FONT.h"
#include "FONTS/ICONS.h"
#include "Scarecrow/hook.h"
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
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

ImFont* g_IconFont = nullptr;
ImFont* g_BoldFont = nullptr;

static void LoadCustomIconFonts(ImGuiIO& io) {
    io.Fonts->AddFontFromMemoryTTF((void*)inter_medium, sizeof(inter_medium), 20.f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    g_BoldFont = io.Fonts->AddFontFromMemoryTTF((void*)inter_bold, sizeof(inter_bold), 23.f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    g_IconFont = io.Fonts->AddFontFromMemoryTTF((void*)F107_data, F107_size, 25.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    io.Fonts->Build();
}

static void print_dashboard() {
    time_t exp = lic::expiry_date();
    struct tm* tm_info = (exp > 0) ? gmtime(&exp) : nullptr;
    char exp_buf[64] = {0};
    if (tm_info) {
        strftime(exp_buf, sizeof(exp_buf), "%Y-%m-%d %H:%M:%S UTC", tm_info);
    } else {
        snprintf(exp_buf, sizeof(exp_buf), "Active / Dynamic Subscription");
    }

    const char* dNames[] = { "NONE", "RT HOOK", "RT DEV", "DEV COMPAT", "QX 10-11", "KPM NEO" };
    const char* dName = (选择值 >= 1 && 选择值 <= 5) ? dNames[选择值] : "ARMED";

    printf("====================================================\n");
    printf("   NinMods Kernel Overlay Engine v2.0 (ARM64)\n");
    printf("====================================================\n");
    printf("Environment    : Android Linux aarch64 (Kernel Driver Safe)\n");
    printf("API Key Status : Valid / Active (%s)\n", lic::status_text());
    printf("Token Expiry   : %s\n", exp_buf);
    printf("Driver Mode    : %s (Option %d)\n", dName, 选择值);
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

    screen_config();

    ::abs_ScreenX = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::abs_ScreenY = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_x = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_y = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true, false)) {
        printf("[-] Failed to initialize native surface overlay.\n");
        return -1;
    }

    ImGuiIO& io = ImGui::GetIO();
    LoadCustomIconFonts(io);

    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, true);
    InitTouchAimbot();
    ui::menu::load_logo_pos();

    printf("[+] Overlay engine started successfully.\n");
    printf("[*] Running interactive render loop...\n");
    fflush(stdout);

    while (main_thread_flag) {
        static auto lastProcessCheck = std::chrono::steady_clock::now();
        auto currentCheckTime = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(currentCheckTime - lastProcessCheck).count() >= 1000) {
            lastProcessCheck = currentCheckTime;
            if (game_pid <= 0) {
                game_pid = getProcessID(packageName);
                if (game_pid > 0) {
                    Memory::g_pid = (pid_t)game_pid;
                    if (driver) driver->initialize((pid_t)game_pid);
                    lib_base = 0;
                    lib_base2 = 0;
                }
            } else {
                char pPath[64];
                snprintf(pPath, sizeof(pPath), "/proc/%d", game_pid);
                if (access(pPath, F_OK) != 0) {
                    game_pid = 0;
                    lib_base = 0;
                    lib_base2 = 0;
                }
            }

            if (game_pid > 0 && lib_base == 0) {
                lib_base = get_module_base(targetaLibName, PERM_EXEC);
                if (lib_base > 0) {
                    lib_base2 = get_module_base(targetaLibName2, PERM_EXEC);
                }
            }
        }

        drawBegin();
        {
            ui::menu::render();

            if (game_pid > 0 && lib_base > 0) {
                DrawESP(abs_ScreenX, abs_ScreenY, lib_base);
                AimbotMenu(abs_ScreenX, abs_ScreenY, lib_base);
                TouchAimbotTick(g_ShowMenu);
            }
        }
        drawEnd();
        usleep(16000);
    }

    ui::menu::save_logo_pos();
    shutdown();
    Touch_Close();
    return 0;
}

int main(int argc, char* argv[]) {
    prctl(PR_SET_NAME, "ninmods-kernel", 0, 0, 0);

    crash_guard::install();

    if (!check_license_gate(argc, argv)) {
        return 1;
    }

    print_dashboard();

    return start_overlay();
}
