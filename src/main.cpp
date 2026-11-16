#include "Android_draw/draw.h"
#include "Android_touch/TouchHelperA.h"
#include "globals.hpp"
#include "ui/menu.hpp"
#include "protect/license.hpp"
#include "other/memory.hpp"
#include "other/display_manager.hpp"
#include "driver.h"
#include "game/data.hpp"
#include "func/esp.hpp"
#include "func/aim/aim.hpp"
#include "func/speed.hpp"
#include "FONTS/MAIN_FONT.h"
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

#define LOGI(...) do {} while(0)

ImFont* g_BoldFont = nullptr;

static void LoadCustomFonts(ImGuiIO& io) {
    // initGUI_draw() already pushed Roboto @32px first, so it stays Fonts[0] and
    // ImGui::GetFont() keeps returning it - the menu layout was tuned against
    // that face plus ScaleAllSizes(3.0f). These two are additive faces only.
    //
    // FontDataOwnedByAtlas must be false so ImFontAtlas copies the blob to the
    // heap and owns the copy. inter_medium/inter_bold live in MAIN_FONT.h as
    // static arrays in .rodata; with the default flag (true) the atlas keeps
    // that .rodata pointer and ~ImFontAtlas later IM_FREE()s it — a free() on a
    // non-heap address. See the same note in draw.cpp initGUI_draw().
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;

    cfg.SizePixels = 22.0f;
    io.Fonts->AddFontFromMemoryTTF((void*)inter_medium, sizeof(inter_medium), 22.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    cfg.SizePixels = 25.0f;
    g_BoldFont = io.Fonts->AddFontFromMemoryTTF((void*)inter_bold, sizeof(inter_bold), 25.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

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

    // Driver name is owned by c_driver; reading a separate global here meant
    // the dashboard printed ARMED even when a backend was loaded. The name is
    // reported even while the node is still unloaded — status is a separate
    // line — so a selected-but-not-yet-connected backend is visible.
    const char* dName = driver ? driver->get_driver_name() : "ARMED";
    const int   dOpt  = driver ? driver->getDriverSelectionCached() : 0;

    printf("====================================================\n");
    printf("   NinMods Kernel Overlay Engine v2.0 (ARM64)\n");
    printf("====================================================\n");
    printf("Environment    : Android Linux aarch64 (Kernel Driver Safe)\n");
    printf("API Key Status : Valid / Active (%s)\n", lic::status_text());
    printf("Token Expiry   : %s\n", exp_buf);
    printf("Driver Mode    : %s (Option %d)\n", dName, dOpt);
    printf("Driver Status  : %s\n", (driver && driver->is_driver_loaded()) ? "Loaded / Active" : "Armed (Awaiting Node)");
    printf("Runtime Status : Ready | Kernel DMA Pipeline\n");
    printf("====================================================\n\n");
    fflush(stdout);
}

static bool check_license_gate(int argc, char** argv) {
    lic::init();

    printf("[*] NinMods Kernel Overlay Engine v2.0\n");
    printf("[*] Verifying authorization...\n");
    fflush(stdout);

    for (int i = 1; i < argc; i++) {
        if (argv[i] && strlen(argv[i]) >= 8 && strcmp(argv[i], "ffmax") != 0 && strcmp(argv[i], "START") != 0) {
            if (lic::set_key(argv[i]) && lic::is_valid()) {
                printf("[+] Pre-authorized via CLI argument.\n");
                printf("[+] Key validated. Overlay authorized.\n\n");
                print_dashboard();
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
            print_dashboard();
            fflush(stdout);
            return true;
        } else {
            printf("[-] Saved session key is invalid or expired.\n");
            fflush(stdout);
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
    print_dashboard();
    fflush(stdout);

    if (isatty(fileno(stdin))) {
        printf("[+] Interactive terminal detected. Launching overlay...\n\n");
        fflush(stdout);
        return true;
    }

    printf("[?] Ready to load into memory. Awaiting START signal...\n");
    fflush(stdout);

    // Daemon mode: block until the launcher writes a line. Anything typed is
    // ignored; the newline itself is the go-ahead.
    char start_input[64] = {0};
    (void)fgets(start_input, sizeof(start_input), stdin);

    printf("[+] Launching overlay...\n\n");
    fflush(stdout);
    return true;
}

static int start_overlay() {
    screen_config();

    ::native_window_screen_x = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_y = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    // Synchronize global viewport dimensions for accurate WorldToScreen projection
    ::g_sw = (float)::native_window_screen_x;
    ::g_sh = (float)::native_window_screen_y;

    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true, false)) {
        printf("[-] Failed to initialize native surface overlay.\n");
        return -1;
    }

    ImGuiIO& io = ImGui::GetIO();
    LoadCustomFonts(io);

    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, false);
    ui::menu::load_logo_pos();

    printf("[+] Overlay engine started successfully.\n");
    printf("[*] Running interactive render loop...\n");
    fflush(stdout);

    while (main_thread_flag) {
        static auto lastProcessCheck = std::chrono::steady_clock::now();
        auto currentCheckTime = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(currentCheckTime - lastProcessCheck).count() >= 1000) {
            lastProcessCheck = currentCheckTime;
            int curPid = proc::pid.load(std::memory_order_acquire);
            if (curPid <= 0) {
                curPid = get_pid();
                if (curPid > 0) {
                    proc::lib.store(0, std::memory_order_release);
                    proc::pid.store(curPid, std::memory_order_release);
                    if (driver) {
                        driver->initialize((pid_t)curPid);
                        driver->init_key((char*)"ninmods_kernel");
                        driver->hide_process();
                    }
                }
            } else {
                char pPath[64];
                snprintf(pPath, sizeof(pPath), "/proc/%d", curPid);
                if (access(pPath, F_OK) != 0) {
                    // Clear lib before pid so a worker never pairs a live pid
                    // with the previous module base.
                    proc::lib.store(0, std::memory_order_release);
                    proc::pid.store(0, std::memory_order_release);
                    data::thread_stop();
                    aimbot::thread_stop();
                    curPid = 0;
                }
            }

            if (curPid > 0 && proc::lib.load(std::memory_order_acquire) == 0) {
                uint64_t base = get_lib("libil2cpp.so");
                if (base > 0) {
                    proc::lib.store(base, std::memory_order_release);
                    data::thread_start();
                    aimbot::thread_start();
                }
            }
        }

        drawBegin();
        {
            // UI touch regions are rebuilt from scratch each frame: clear the
            // staged list, let each panel append its own rect, then publish.
            Touch_ClearUiRects();
            ui::menu::render();
            Touch_CommitUiRects();

            uint64_t libBase = proc::lib.load(std::memory_order_acquire);
            if (proc::pid.load(std::memory_order_acquire) > 0 && libBase > 0) {
                speed_system::tick(libBase);
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
    // Mask process name to mimic standard kernel worker to avoid userland process enumeration detection
    prctl(PR_SET_NAME, "kworker/u16:1", 0, 0, 0);

    if (!check_license_gate(argc, argv)) {
        return 1;
    }

    return start_overlay();
}
