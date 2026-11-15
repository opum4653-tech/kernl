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
        bool  ipad_last = false;
        float ipad_last_val = 1.0f;

        bool  pickup_last = false;
        float pickup_last_val = 2.0f;

        bool  hitbox_last = false;
        float hitbox_last_val = 1.0f;

        bool  jump_last = false;
        float jump_last_val = 1.0f;
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

        return driver_read<uintptr_t>(staticFields + 0x8);
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

        // Sync with UI variables
        bool is_enabled = cfg::master_enabled && (cfg::motion::speed_hack || g_config.enabled);
        float mult = (cfg::motion::speed_hack && cfg::motion::speed_mult >= 1.0f) ? cfg::motion::speed_mult : g_config.multiplier;
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
                float mappedSpeed = 0.033f + ((mult - 1.0f) / 4.0f) * 0.026f;
                driver_write_float(timeService + Offsets::Engine_TimeServiceSpeed, mappedSpeed);
            }

            // 2. LocalPlayer Entity Speed & PlayerAttributes Speed Scales
            if (localPlayer >= 0x10000) {
                // Entity Move Speed (Player + 0x560)
                driver_write_float(localPlayer + Offsets::Player_Speed, mult);

                // Player Attributes (Player + 0x768 Global / 0x770 Max)
                uintptr_t attrs = resolve_player_attributes(localPlayer);
                if (attrs >= 0x10000) {
                    driver_write_float(attrs + Offsets::RunSpeedUpScale, mult);
                    driver_write_float(attrs + Offsets::FallingSpeedUpScale, mult);
                }
            }

            g_config.last_state = true;
            g_config.last_mult = mult;
        }
        // When Speed was active but is now DISABLED -> CLEAN RESTORE
        else if (g_config.last_state) {
            // Restore default TimeService Delta (0.033333f)
            uintptr_t timeService = driver_read<uintptr_t>(baseGame + Offsets::Engine_TimeService);
            if (timeService >= 0x10000) {
                driver_write_float(timeService + Offsets::Engine_TimeServiceSpeed, 0.033333f);
            }

            // Restore LocalPlayer Speed to 1.0f
            if (localPlayer >= 0x10000) {
                driver_write_float(localPlayer + Offsets::Player_Speed, 1.0f);

                uintptr_t attrs = resolve_player_attributes(localPlayer);
                if (attrs >= 0x10000) {
                    driver_write_float(attrs + Offsets::RunSpeedUpScale, 1.0f);
                    driver_write_float(attrs + Offsets::FallingSpeedUpScale, 1.0f);
                }
            }

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

        // 1. iPad View / Wide FOV (0x36C9BD0)
        if (libAddress >= 0x10000) {
            static constexpr uint64_t IPAD_OFFSET = 0x36C9BD0;
            if (cfg::master_enabled && cfg::motion::ipad_view) {
                float targetIpad = cfg::motion::ipad_val;
                driver_write_float(libAddress + IPAD_OFFSET, targetIpad);
                g_mod_state.ipad_last = true;
                g_mod_state.ipad_last_val = targetIpad;
            } else if (g_mod_state.ipad_last) {
                driver_write_float(libAddress + IPAD_OFFSET, 1.0f);
                g_mod_state.ipad_last = false;
                g_mod_state.ipad_last_val = 1.0f;
            }
        }

        // 2. Auto-Pickup Range (0xF1D5F70 -> 0x30, 0x4B8, 0x1EB4)
        static constexpr uint64_t PR_BASE = 0xF1D5F70;
        static constexpr uint64_t PR_OFFSETS[] = { 0x30, 0x4B8, 0x1EB4 };
        if (cfg::master_enabled && cfg::motion::pickup_range) {
            uint64_t prAddr = func::find_addr(PR_BASE, PR_OFFSETS, 3);
            if (prAddr >= 0x10000) {
                driver_write_float(prAddr, cfg::motion::pickup_val);
                g_mod_state.pickup_last = true;
            }
        } else if (g_mod_state.pickup_last) {
            uint64_t prAddr = func::find_addr(PR_BASE, PR_OFFSETS, 3);
            if (prAddr >= 0x10000) {
                driver_write_float(prAddr, 2.0f);
                g_mod_state.pickup_last = false;
            }
        }

        // 3. Hitbox / Player Size Scaler (0xF1D5F70 -> 0x30, 0x4B8, 0x510, 0x1FC/0x200/0x204)
        static constexpr uint64_t PS_BASE = 0xF1D5F70;
        static constexpr uint64_t PSX_OFF[] = { 0x30, 0x4B8, 0x510, 0x1FC };
        static constexpr uint64_t PSY_OFF[] = { 0x30, 0x4B8, 0x510, 0x200 };
        static constexpr uint64_t PSZ_OFF[] = { 0x30, 0x4B8, 0x510, 0x204 };
        if (cfg::master_enabled && cfg::motion::hitbox_scale) {
            float scale = cfg::motion::hitbox_size;
            uint64_t ax = func::find_addr(PS_BASE, PSX_OFF, 4);
            uint64_t ay = func::find_addr(PS_BASE, PSY_OFF, 4);
            uint64_t az = func::find_addr(PS_BASE, PSZ_OFF, 4);
            if (ax >= 0x10000) driver_write_float(ax, scale);
            if (ay >= 0x10000) driver_write_float(ay, scale);
            if (az >= 0x10000) driver_write_float(az, scale);
            g_mod_state.hitbox_last = true;
        } else if (g_mod_state.hitbox_last) {
            uint64_t ax = func::find_addr(PS_BASE, PSX_OFF, 4);
            uint64_t ay = func::find_addr(PS_BASE, PSY_OFF, 4);
            uint64_t az = func::find_addr(PS_BASE, PSZ_OFF, 4);
            if (ax >= 0x10000) driver_write_float(ax, 1.0f);
            if (ay >= 0x10000) driver_write_float(ay, 1.0f);
            if (az >= 0x10000) driver_write_float(az, 1.0f);
            g_mod_state.hitbox_last = false;
        }

        // 4. High Jump (0xF1D5F70 -> 0x30, 0x4B8, 0x518, 0x21C)
        static constexpr uint64_t HJ_BASE = 0xF1D5F70;
        static constexpr uint64_t HJ_OFF[] = { 0x30, 0x4B8, 0x518, 0x21C };
        if (cfg::master_enabled && cfg::motion::high_jump) {
            uint64_t hjAddr = func::find_addr(HJ_BASE, HJ_OFF, 4);
            if (hjAddr >= 0x10000) {
                driver_write_float(hjAddr, cfg::motion::high_jump_val);
                g_mod_state.jump_last = true;
            }
        } else if (g_mod_state.jump_last) {
            uint64_t hjAddr = func::find_addr(HJ_BASE, HJ_OFF, 4);
            if (hjAddr >= 0x10000) {
                driver_write_float(hjAddr, 1.0f);
                g_mod_state.jump_last = false;
            }
        }
    }

} // namespace speed_system
