#include "Android_draw/draw.h"
#include "Android_touch/TouchHelperA.h"
#include "globals.hpp"
#include "ui/menu.hpp"
#include "protect/license.hpp"
#include "protect/oxorany.hpp"
#include "other/memory.hpp"
#include "other/logger.hpp"
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

// ==============================================================================================
// FILE: src/main.cpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Engine lifecycle entry point, process masquerade, watchdog coordinator, and render tick.
//
// SYSTEM LIFECYCLE & WORKFLOW OVERVIEW:
// ----------------------------------------------------------------------------------------------
// 1. ENTRY (main()):
//    - Disguises process comm name to "kworker/u16:1" via prctl(PR_SET_NAME) to bypass basic ps scans.
//    - Validates access credentials through check_license_gate() in src/protect/license.hpp.
//    - Enters start_overlay().
//
// 2. OVERLAY INITIALIZATION:
//    - Resolves display resolution & orientation via display_manager.cpp.
//    - Creates native SurfaceFlinger overlay window via initGUI_draw() (includes/draw/Android_draw).
//    - Initializes multi-touch event capture thread via Touch_Init() (includes/internal/Android_touch).
//    - Builds ImGui font atlas with Roboto and Inter Bold/Medium TTF fonts.
//
// 3. TARGET PROCESS WATCHDOG (1000ms periodic tick):
//    - Invokes get_pid() (in memory.hpp) using raw direct assembly syscalls (/proc scan).
//    - Attaches kernel driver module (c_driver in driver.h) to target game PID.
//    - Resolves libil2cpp.so module base address via get_lib().
//    - Launches data::thread_start() (in data.cpp) and trajectory::thread_start() (in aim.hpp).
//    - Detects game exit/crash via /proc/<pid> access test; cleans up and halts workers gracefully.
//
// 4. MAIN RENDER LOOP (~60 FPS / 16ms sleep):
//    - Rebuilds touchable UI rectangle hit-boxes (Touch_ClearUiRects / Touch_CommitUiRects).
//    - Calls ui::menu::render() (in menu.cpp) which renders ESP visuals and ImGui interactive menus.
//    - Calls speed_system::tick(libBase) (in speed.hpp) for Velocity Modifier and Recoil Compensation patches.
//
// 5. CLEAN REVERSE SHUTDOWN:
//    - Restores patched game memory via speed_system::shutdown().
//    - Halts and joins background worker threads (data::thread_stop(), trajectory::thread_stop()).
//    - Closes touch device handles and native surface window.
// ==============================================================================================

#define LOGI(...) do {} while(0)

// Global font handle for high-contrast bold typography
ImFont* g_BoldFont = nullptr;

// Loads custom TrueType vector fonts into ImGui's font atlas
static void LoadCustomFonts(ImGuiIO& io) {
    // initGUI_draw() already pushed Roboto @32px first, so it stays Fonts[0] and
    // ImGui::GetFont() keeps returning it - the menu layout was tuned against
    // that face plus ScaleAllSizes(3.0f). These two are additive faces only.
    //
    // FontDataOwnedByAtlas must be false so ImFontAtlas copies the blob to the
    // heap and owns the copy. inter_medium/inter_bold live in MAIN_FONT.h as
    // static arrays in .rodata; with the default flag (true) the atlas keeps
    // that .rodata pointer and ~ImFontAtlas later IM_FREE()s it — a free() on a
    // non-heap address.
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;

    cfg.SizePixels = 22.0f;
    io.Fonts->AddFontFromMemoryTTF((void*)inter_medium, sizeof(inter_medium), 22.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    cfg.SizePixels = 25.0f;
    g_BoldFont = io.Fonts->AddFontFromMemoryTTF((void*)inter_bold, sizeof(inter_bold), 25.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    io.Fonts->Build();
}

// Prints console dashboard telemetry detailing driver, license, and runtime status
static void print_dashboard() {
    time_t exp = lic::expiry_date();
    struct tm* tm_info = (exp > 0) ? gmtime(&exp) : nullptr;
    char exp_buf[64] = {0};
    if (tm_info) {
        strftime(exp_buf, sizeof(exp_buf), "%Y-%m-%d %H:%M:%S UTC", tm_info);
    } else {
        snprintf(exp_buf, sizeof(exp_buf), "Active / Dynamic Subscription");
    }

    const char* dName = driver ? driver->get_driver_name() : "ARMED";
    const int   dOpt  = driver ? driver->getDriverSelectionCached() : 0;

    printf("====================================================\n");
    printf("   Kernel Service Overlay Engine v2.0 (ARM64)\n");
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

// Security gate: authenticates runtime authorization keys via CLI args, environment variables, or stdin
static bool check_license_gate(int argc, char** argv) {
    lic::init();

    printf("[*] Kernel Service Overlay Engine v2.0\n");
    printf("[*] Verifying authorization...\n");
    fflush(stdout);

    // 1. Scan command-line arguments for pre-passed license key
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

    // 2. Scan environment session variables (checks KERNEL_AUTH_KEY first, then NIN_LICENSE_KEY fallback)
    const char* env_key = getenv("KERNEL_AUTH_KEY");
    if (!env_key || strlen(env_key) == 0) {
        env_key = getenv("NIN_LICENSE_KEY");
    }
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

    // 3. Interactive prompt fallback via standard input
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

    // Daemon mode: block until the launcher writes a line (newline is go-ahead)
    char start_input[64] = {0};
    (void)fgets(start_input, sizeof(start_input), stdin);

    printf("[+] Launching overlay...\n\n");
    fflush(stdout);
    return true;
}

// Master overlay coordinator routine
static int start_overlay() {
    // 1. Query display geometry and orientation from display_manager
    screen_config();

    ::native_window_screen_x = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_y = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    // Synchronize global viewport dimensions for accurate WorldToScreen projection
    ::g_sw = (float)::native_window_screen_x;
    ::g_sh = (float)::native_window_screen_y;

    // 2. Initialize native SurfaceFlinger overlay window
    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true, false)) {
        printf("[-] Failed to initialize native surface overlay.\n");
        return -1;
    }

    ImGuiIO& io = ImGui::GetIO();
    LoadCustomFonts(io);

    // 3. Initialize low-level touch event interception pump
    Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, false);
    ui::menu::load_logo_pos();

    logger::init();

    printf("[+] Overlay engine started successfully.\n");
    printf("[*] Running interactive render loop...\n");
    fflush(stdout);

    // 4. MAIN RENDER LOOP (~60 FPS)
    while (main_thread_flag) {
        static auto lastProcessCheck = std::chrono::steady_clock::now();
        auto currentCheckTime = std::chrono::steady_clock::now();

        // Target Process Watchdog Tick (Executes once per second)
        if (std::chrono::duration_cast<std::chrono::milliseconds>(currentCheckTime - lastProcessCheck).count() >= 1000) {
            lastProcessCheck = currentCheckTime;
            int curPid = proc::pid.load(std::memory_order_acquire);

            // Phase A: Search for Game Process PID if not currently attached
            if (curPid <= 0) {
                curPid = get_pid();
                if (curPid > 0) {
                    proc::lib.store(0, std::memory_order_release);
                    proc::pid.store(curPid, std::memory_order_release);
                    logger::log_found("Game Process PID: %d", curPid);
                    if (driver) {
                        driver->initialize((pid_t)curPid);
                        driver->init_key((char*)oxorany("ninmods_kernel"));
                        driver->hide_process();
                        logger::log_info("Kernel Driver attached to PID %d (%s)", curPid, driver->get_driver_name());
                    }
                } else {
                    if (logger::should_log("pid_search", 3000)) {
                        logger::log_not_found("Game Process PID (com.dts.freefiremax not running)");
                    }
                }
            }
            // Phase B: Watchdog check: verify process remains alive
            else {
                char pPath[64];
                snprintf(pPath, sizeof(pPath), "/proc/%d", curPid);
                if (access(pPath, F_OK) != 0) {
                    logger::log_not_found("Game Process PID %d exited (Process terminated)", curPid);
                    // Clear lib before pid so worker threads never pair live pid with dead base
                    proc::lib.store(0, std::memory_order_release);
                    proc::pid.store(0, std::memory_order_release);
                    data::thread_stop();
                    trajectory::thread_stop();
                    curPid = 0;
                }
            }

            // Phase C: Resolve libil2cpp.so base address and spawn worker threads
            if (curPid > 0 && proc::lib.load(std::memory_order_acquire) == 0) {
                uint64_t base = get_lib("libil2cpp.so");
                if (base > 0) {
                    proc::lib.store(base, std::memory_order_release);
                    logger::log_found("libil2cpp.so Base Address: 0x%lX", (unsigned long)base);
                    data::thread_start();
                    trajectory::thread_start();
                } else {
                    if (logger::should_log("lib_search", 3000)) {
                        logger::log_not_found("libil2cpp.so Base Address (Library not loaded into memory yet)");
                    }
                }
            }
        }

        // Frame rendering cycle
        drawBegin();
        {
            // UI touch regions are rebuilt from scratch each frame
            Touch_ClearUiRects();
            ui::menu::render(); // Renders ESP drawings, floating logo, and ImGui menu
            Touch_CommitUiRects();

            // Execute Velocity Modifier and Recoil Compensation memory patches
            uint64_t libBase = proc::lib.load(std::memory_order_acquire);
            if (proc::pid.load(std::memory_order_acquire) > 0 && libBase > 0) {
                speed_system::tick(libBase);
            }
        }
        drawEnd();
        usleep(16000); // Frame cadence sleep (~60 FPS)
    }

    // 5. Clean symmetrical shutdown sequence
    speed_system::shutdown();
    data::thread_stop();
    trajectory::thread_stop();
    ui::menu::save_logo_pos();
    shutdown();
    Touch_Close();
    return 0;
}

// Executable entry point
int main(int argc, char* argv[]) {
    // Mask process name to mimic standard Linux kernel worker thread to avoid userland process enumeration detection
    prctl(PR_SET_NAME, "kworker/u16:1", 0, 0, 0);

    // Verify license credentials before proceeding
    if (!check_license_gate(argc, argv)) {
        return 1;
    }

    // Launch overlay engine
    return start_overlay();
}
