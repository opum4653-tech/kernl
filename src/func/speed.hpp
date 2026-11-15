#pragma once
#include <cstdint>
#include <cmath>
#include "driver.h"
#include "../Scarecrow/offsets.h"

// Forward-declare the legacy UI variables so they link seamlessly
extern bool SpeedHackV2;
extern float SpeedMultiplier;

namespace speed_system {

    // Advanced & simple common global structure for player speed modification
    struct SpeedConfig {
        bool  enabled        = false;  // Speed toggle
        float multiplier     = 4.8f;   // 1.0x to 5.0x
        bool  last_state     = false;  // Tracking previous state for clean restore
        float last_mult      = 1.0f;
    };

    inline SpeedConfig g_config;

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

    // Master unblocked tick function called every frame
    inline void tick(uintptr_t libAddress) {
        // STRICT KERNEL DRIVER ONLY:
        // Refuses execution if kernel driver is not loaded or uninitialized.
        if (!is_driver_active() || libAddress < 0x10000) return;

        // Sync with UI variables
        bool is_enabled = SpeedHackV2 || g_config.enabled;
        float mult = (SpeedMultiplier >= 1.0f) ? SpeedMultiplier : g_config.multiplier;
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

                // Player Attributes (Player + 0x770 or 0x768)
                uintptr_t attrs = driver_read<uintptr_t>(localPlayer + Offsets::PlayerAttributes);
                if (attrs >= 0x10000) {
                    driver_write_float(attrs + Offsets::RunSpeedUpScale, mult); // 0x2C0: RunSpeedUpScale
                    driver_write_float(attrs + 0x2BC, mult);                   // 0x2BC: FallingSpeedUpScale
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

                uintptr_t attrs = driver_read<uintptr_t>(localPlayer + Offsets::PlayerAttributes);
                if (attrs >= 0x10000) {
                    driver_write_float(attrs + Offsets::RunSpeedUpScale, 1.0f);
                    driver_write_float(attrs + 0x2BC, 1.0f);
                }
            }

            g_config.last_state = false;
            g_config.last_mult = 1.0f;
        }
    }

} // namespace speed_system
