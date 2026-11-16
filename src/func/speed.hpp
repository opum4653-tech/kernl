#pragma once
#include <cstdint>
#include <cmath>
#include "driver.h"
#include "../offsets/offsets.hpp"
#include "../other/addr.hpp"
#include "../ui/cfg.hpp"

namespace speed_system {

    // Advanced & simple common global structure for player speed modification
    struct SpeedConfig {
        bool  enabled        = false;  // Speed toggle
        float multiplier     = 4.8f;   // 1.0x to 5.0x
        bool  last_state     = false;  // Tracking previous state for clean restore
        float last_mult      = 1.0f;
    };

    inline SpeedConfig g_config;

    struct ModState {
        bool  speed_last = false;
        float orig_time_delta = 0.033333f;
        bool  orig_time_captured = false;

        float orig_player_speed = 1.0f;
        float orig_run_scale = 1.0f;
        float orig_fall_scale = 1.0f;
        bool  orig_player_captured = false;

        bool  ipad_last = false;
        float ipad_orig = 1.0f;
        bool  ipad_orig_captured = false;

        bool  pickup_last = false;
        float pickup_orig = 2.0f;
        bool  pickup_orig_captured = false;

        bool  hitbox_last = false;
        float hitbox_orig_x = 1.0f;
        float hitbox_orig_y = 1.0f;
        float hitbox_orig_z = 1.0f;
        bool  hitbox_orig_captured = false;

        bool  jump_last = false;
        float jump_orig = 1.0f;
        bool  jump_orig_captured = false;
    };
    inline ModState g_mod_state;

    // Checks if the kernel driver is active and ready
    // MANDATORY CONSTRAINT: Speed operations ONLY run through the kernel driver.
    inline bool is_driver_active() {
        return (driver != nullptr && driver->is_driver_loaded());
    }

    // Direct kernel driver write wrappers
    inline bool driver_write_float(uintptr_t addr, float val) {
        if (!is_driver_active() || addr < 0x10000) return false;
        return driver->write(addr, &val, sizeof(float));
    }

    template<typename T>
    inline T driver_read(uintptr_t addr) {
        T data{};
        if (is_driver_active() && addr >= 0x10000) {
            driver->read(addr, &data, sizeof(T));
        }
        return data;
    }

    // Resolves BaseGame address safely from libil2cpp without blocking
    inline uintptr_t resolve_base_game(uintptr_t libAddress) {
        if (!is_driver_active() || libAddress < 0x10000) return 0;

        uintptr_t gfCell = driver_read<uintptr_t>(libAddress + Offsets::GameFacade);
        if (gfCell < 0x10000) return 0;

        uintptr_t gfClass = driver_read<uintptr_t>(gfCell);
        if (gfClass < 0x10000) return 0;

        uintptr_t staticFields = driver_read<uintptr_t>(gfClass + Offsets::GameFacade_P2);
        if (staticFields < 0x10000) return 0;

        return driver_read<uintptr_t>(staticFields + Offsets::StaticFields_Instance);
    }

    // Resolves LocalPlayer pointer safely from BaseGame without blocking
    inline uintptr_t resolve_local_player(uintptr_t baseGame) {
        if (!is_driver_active() || baseGame < 0x10000) return 0;

        uintptr_t match = driver_read<uintptr_t>(baseGame + Offsets::BaseGame_Match);
        if (match < 0x10000) return 0;

        return driver_read<uintptr_t>(match + Offsets::Match_LocalPlayer);
    }

    // Resolves PlayerAttributes safely (0x768 Free Fire Global Primary, 0x770 Max Fallback)
    inline uintptr_t resolve_player_attributes(uintptr_t localPlayer) {
        if (!is_driver_active() || localPlayer < 0x10000) return 0;
        uintptr_t attrs = driver_read<uintptr_t>(localPlayer + Offsets::PlayerAttributes); // 0x768 Global
        if (attrs < 0x10000) {
            attrs = driver_read<uintptr_t>(localPlayer + Offsets::PlayerAttributes_Max);     // 0x770 Max
        }
        return attrs;
    }

    // Master unblocked tick function called every frame
    inline void tick(uintptr_t libAddress) {
        // STRICT KERNEL DRIVER ONLY:
        // Refuses execution if kernel driver is not loaded or uninitialized.
        if (!is_driver_active() || libAddress < 0x10000) return;

        // Sync with UI variables. Snapshot the knobs once: the render thread
        // owns them and this tick must act on one coherent set of values.
        const bool  ui_speed_on = cfg::motion::speed_hack.load(std::memory_order_relaxed);
        const float ui_speed_mx = cfg::motion::speed_mult.load(std::memory_order_relaxed);

        bool is_enabled = cfg::master_enabled && (ui_speed_on || g_config.enabled);
        float mult = (ui_speed_on && ui_speed_mx >= 1.0f) ? ui_speed_mx : g_config.multiplier;
        if (mult < 1.0f) mult = 1.0f;
        if (mult > 10.0f) mult = 10.0f;

        g_config.enabled = is_enabled;
        g_config.multiplier = mult;

        // Resolve engine root pointers
        uintptr_t baseGame = resolve_base_game(libAddress);
        if (baseGame < 0x10000) return;

        uintptr_t localPlayer = resolve_local_player(baseGame);

        // When Speed is ENABLED
        if (is_enabled) {
            // 1. TimeService Delta Speed (Engine Global Delta Scaling)
            uintptr_t timeService = driver_read<uintptr_t>(baseGame + Offsets::Engine_TimeService);
            if (timeService >= 0x10000) {
                if (!g_mod_state.orig_time_captured) {
                    float curDelta = driver_read<float>(timeService + Offsets::Engine_TimeServiceSpeed);
                    if (curDelta >= 0.005f && curDelta <= 0.2f) {
                        g_mod_state.orig_time_delta = curDelta;
                    }
                    g_mod_state.orig_time_captured = true;
                }
                float mappedSpeed = 0.033f + ((mult - 1.0f) / 4.0f) * 0.026f;
                driver_write_float(timeService + Offsets::Engine_TimeServiceSpeed, mappedSpeed);
            }

            // 2. LocalPlayer Entity Speed & PlayerAttributes Speed Scales
            if (localPlayer >= 0x10000) {
                uintptr_t attrs = resolve_player_attributes(localPlayer);
                if (!g_mod_state.orig_player_captured) {
                    float curSpeed = driver_read<float>(localPlayer + Offsets::Player_Speed);
                    if (curSpeed >= 0.1f && curSpeed <= 5.0f) {
                        g_mod_state.orig_player_speed = curSpeed;
                    }
                    if (attrs >= 0x10000) {
                        float curRun = driver_read<float>(attrs + Offsets::RunSpeedUpScale);
                        float curFall = driver_read<float>(attrs + Offsets::FallingSpeedUpScale);
                        if (curRun >= 0.1f && curRun <= 5.0f) g_mod_state.orig_run_scale = curRun;
                        if (curFall >= 0.1f && curFall <= 5.0f) g_mod_state.orig_fall_scale = curFall;
                    }
                    g_mod_state.orig_player_captured = true;
                }

                // Entity Move Speed (Player + 0x560)
                driver_write_float(localPlayer + Offsets::Player_Speed, mult);

                // Player Attributes (Player + 0x768 Global / 0x770 Max)
                if (attrs >= 0x10000) {
                    driver_write_float(attrs + Offsets::RunSpeedUpScale, mult);
                    driver_write_float(attrs + Offsets::FallingSpeedUpScale, mult);
                }
            }

            g_config.last_state = true;
            g_config.last_mult = mult;
        }
        // When Speed was active but is now DISABLED -> CLEAN RESTORE TO CAPTURED BASELINE
        else if (g_config.last_state) {
            uintptr_t timeService = driver_read<uintptr_t>(baseGame + Offsets::Engine_TimeService);
            if (timeService >= 0x10000) {
                driver_write_float(timeService + Offsets::Engine_TimeServiceSpeed, g_mod_state.orig_time_delta);
            }

            if (localPlayer >= 0x10000) {
                driver_write_float(localPlayer + Offsets::Player_Speed, g_mod_state.orig_player_speed);

                uintptr_t attrs = resolve_player_attributes(localPlayer);
                if (attrs >= 0x10000) {
                    driver_write_float(attrs + Offsets::RunSpeedUpScale, g_mod_state.orig_run_scale);
                    driver_write_float(attrs + Offsets::FallingSpeedUpScale, g_mod_state.orig_fall_scale);
                }
            }

            g_mod_state.orig_time_captured = false;
            g_mod_state.orig_player_captured = false;
            g_config.last_state = false;
            g_config.last_mult = 1.0f;
        }

        // Gameplay mods execution via pure kernel driver
        if (localPlayer >= 0x10000) {
            if (cfg::master_enabled && cfg::motion::no_reload) {
                uintptr_t attrs = resolve_player_attributes(localPlayer);
                if (attrs >= 0x10000) {
                    bool val = true;
                    driver->write(attrs + Offsets::ShootNoReload, &val, sizeof(bool));
                }
            }

            if (cfg::master_enabled && cfg::motion::no_recoil) {
                uintptr_t weapon = driver_read<uintptr_t>(localPlayer + Offsets::Player_Weapon);
                if (weapon >= 0x10000) {
                    uintptr_t wdata = driver_read<uintptr_t>(weapon + Offsets::Weapon_Data);
                    if (wdata >= 0x10000) {
                        float zero = 0.0f;
                        driver->write(wdata + Offsets::Weapon_Recoil, &zero, sizeof(float));
                    }
                }
            }

            if (cfg::master_enabled && cfg::motion::ghost_hack) {
                bool val = true;
                driver->write(localPlayer + Offsets::GhostHack, &val, sizeof(bool));
            }
        }

        // 1. iPad View / Wide FOV
        if (libAddress >= 0x10000) {
            uintptr_t ipadAddr = libAddress + Offsets::iPad_FOV;
            if (cfg::master_enabled && cfg::motion::ipad_view) {
                if (!g_mod_state.ipad_orig_captured) {
                    float curIpad = driver_read<float>(ipadAddr);
                    if (curIpad >= 0.1f && curIpad <= 5.0f) g_mod_state.ipad_orig = curIpad;
                    g_mod_state.ipad_orig_captured = true;
                }
                float targetIpad = cfg::motion::ipad_val;
                driver_write_float(ipadAddr, targetIpad);
                g_mod_state.ipad_last = true;
            } else if (g_mod_state.ipad_last) {
                driver_write_float(ipadAddr, g_mod_state.ipad_orig);
                g_mod_state.ipad_last = false;
                g_mod_state.ipad_orig_captured = false;
            }
        }

        // 2. Auto-Pickup Range
        static constexpr uint64_t PR_OFFSETS[] = { 0x30, 0x4B8, 0x1EB4 };
        if (cfg::master_enabled && cfg::motion::pickup_range) {
            uint64_t prAddr = func::find_addr(Offsets::GameConfig_Base, PR_OFFSETS, 3);
            if (prAddr >= 0x10000) {
                if (!g_mod_state.pickup_orig_captured) {
                    float curPr = driver_read<float>(prAddr);
                    if (curPr >= 0.1f && curPr <= 10.0f) g_mod_state.pickup_orig = curPr;
                    g_mod_state.pickup_orig_captured = true;
                }
                driver_write_float(prAddr, cfg::motion::pickup_val);
                g_mod_state.pickup_last = true;
            }
        } else if (g_mod_state.pickup_last) {
            uint64_t prAddr = func::find_addr(Offsets::GameConfig_Base, PR_OFFSETS, 3);
            if (prAddr >= 0x10000) {
                driver_write_float(prAddr, g_mod_state.pickup_orig);
                g_mod_state.pickup_last = false;
                g_mod_state.pickup_orig_captured = false;
            }
        }

        // 3. Hitbox / Player Size Scaler
        static constexpr uint64_t PSX_OFF[] = { 0x30, 0x4B8, 0x510, 0x1FC };
        static constexpr uint64_t PSY_OFF[] = { 0x30, 0x4B8, 0x510, 0x200 };
        static constexpr uint64_t PSZ_OFF[] = { 0x30, 0x4B8, 0x510, 0x204 };
        if (cfg::master_enabled && cfg::motion::hitbox_scale) {
            float scale = cfg::motion::hitbox_size;
            uint64_t ax = func::find_addr(Offsets::GameConfig_Base, PSX_OFF, 4);
            uint64_t ay = func::find_addr(Offsets::GameConfig_Base, PSY_OFF, 4);
            uint64_t az = func::find_addr(Offsets::GameConfig_Base, PSZ_OFF, 4);
            if (ax >= 0x10000 && ay >= 0x10000 && az >= 0x10000) {
                if (!g_mod_state.hitbox_orig_captured) {
                    g_mod_state.hitbox_orig_x = driver_read<float>(ax);
                    g_mod_state.hitbox_orig_y = driver_read<float>(ay);
                    g_mod_state.hitbox_orig_z = driver_read<float>(az);
                    g_mod_state.hitbox_orig_captured = true;
                }
                driver_write_float(ax, scale);
                driver_write_float(ay, scale);
                driver_write_float(az, scale);
                g_mod_state.hitbox_last = true;
            }
        } else if (g_mod_state.hitbox_last) {
            uint64_t ax = func::find_addr(Offsets::GameConfig_Base, PSX_OFF, 4);
            uint64_t ay = func::find_addr(Offsets::GameConfig_Base, PSY_OFF, 4);
            uint64_t az = func::find_addr(Offsets::GameConfig_Base, PSZ_OFF, 4);
            if (ax >= 0x10000 && ay >= 0x10000 && az >= 0x10000) {
                driver_write_float(ax, g_mod_state.hitbox_orig_x);
                driver_write_float(ay, g_mod_state.hitbox_orig_y);
                driver_write_float(az, g_mod_state.hitbox_orig_z);
                g_mod_state.hitbox_last = false;
                g_mod_state.hitbox_orig_captured = false;
            }
        }

        // 4. High Jump
        static constexpr uint64_t HJ_OFF[] = { 0x30, 0x4B8, 0x518, 0x21C };
        if (cfg::master_enabled && cfg::motion::high_jump) {
            uint64_t hjAddr = func::find_addr(Offsets::GameConfig_Base, HJ_OFF, 4);
            if (hjAddr >= 0x10000) {
                if (!g_mod_state.jump_orig_captured) {
                    float curJ = driver_read<float>(hjAddr);
                    if (curJ >= 0.1f && curJ <= 5.0f) g_mod_state.jump_orig = curJ;
                    g_mod_state.jump_orig_captured = true;
                }
                driver_write_float(hjAddr, cfg::motion::high_jump_val);
                g_mod_state.jump_last = true;
            }
        } else if (g_mod_state.jump_last) {
            uint64_t hjAddr = func::find_addr(Offsets::GameConfig_Base, HJ_OFF, 4);
            if (hjAddr >= 0x10000) {
                driver_write_float(hjAddr, g_mod_state.jump_orig);
                g_mod_state.jump_last = false;
                g_mod_state.jump_orig_captured = false;
            }
        }
    }

} // namespace speed_system
