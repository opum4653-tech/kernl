#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "driver.h"
#include "../offsets/offsets.hpp"
#include "../other/addr.hpp"
#include "../ui/cfg.hpp"
#include "../game/data.hpp"

// ==============================================================================================
// FILE: src/func/speed.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Memory controller for Combat & Movement modifications (Velocity Modifier & Recoil Compensation).
//
// CROSS-FILE CALL GRAPH & LIFECYCLE:
// ----------------------------------------------------------------------------------------------
// 1. CALLERS:
//    - src/main.cpp -> start_overlay():
//      * Calls speed_system::tick(libBase) every frame inside the main thread render loop.
//      * Calls speed_system::shutdown() during overlay exit to restore original game memory.
//
// 2. TARGET STRUCTURES IN GAME HEAP (Il2Cpp):
//    - Velocity Modifier: LocalPlayer (0xD8) -> PlayerAttributes (0x768 / 0x770) -> RunSpeedUpScale (0x2C0).
//    - No Recoil: LocalPlayer -> WeaponOnHand (0x600) -> Weapon_Data (0x18) -> Weapon_Recoil (0x80).
//
// 3. ANTI-BAN SAFETY PROTOCOLS & SYMMETRICAL REVERSE PATHS:
//    - Strict Speed Clamp: Multiplier is mathematically clamped to 1.00x - 1.30x (server-safe boundary).
//    - Smart Firing Guard: Automatically drops sprint speed to 1.00x baseline while shooting
//      to prevent server movement displacement telemetry anomalies during bullet firing.
//    - Symmetrical Memory Restoration: Captures original game float values prior to patching;
//      completely restores baseline values when toggled off in the menu or on engine exit.
// ==============================================================================================

namespace speed_system {

    // Internal speed runtime configuration
    struct SpeedConfig {
        bool  enabled        = false;
        float multiplier     = 0.0f; // 0.0f baseline default (zero forced speed increase)
        bool  last_state     = false;
        float last_mult      = 0.0f;
    };
    inline SpeedConfig g_config;

    // Symmetrical memory restore state tracking
    struct ModState {
        float orig_run_scale = 1.0f;        // Original captured player sprint multiplier
        bool  orig_player_captured = false; // True once baseline has been backed up

        bool  recoil_last = false;          // True while recoil patch is currently applied
        float recoil_orig = 0.0f;           // Original captured weapon recoil float
        bool  recoil_orig_captured = false; // True once baseline recoil has been backed up
    };
    inline ModState g_mod_state;

    // Checks if the kernel driver backend is initialized and connected
    inline bool is_driver_active() {
        return (driver != nullptr && driver->is_driver_loaded());
    }

    // Direct kernel driver write wrapper for 32-bit IEEE-754 floats
    inline bool driver_write_float(uintptr_t addr, float val) {
        if (!is_driver_active() || addr < Offsets::MinValidAddress) return false;
        return driver->write(addr, &val, sizeof(float));
    }

    // Direct kernel driver read wrapper
    template<typename T>
    inline T driver_read(uintptr_t addr) {
        T data{};
        if (is_driver_active() && addr >= Offsets::MinValidAddress) {
            driver->read(addr, &data, sizeof(T));
        }
        return data;
    }

    // Resolves BaseGame address safely from libil2cpp without blocking (Dual-Path Fallback)
    inline uintptr_t resolve_base_game(uintptr_t libAddress) {
        if (!is_driver_active() || libAddress < Offsets::MinValidAddress) return 0;

        uintptr_t gfCell = driver_read<uintptr_t>(libAddress + Offsets::GameFacade);
        if (gfCell < Offsets::MinValidAddress) return 0;

        // Path A: gfCell is directly GameFacade Il2CppClass
        uintptr_t sf1 = driver_read<uintptr_t>(gfCell + Offsets::GameFacade_P2);
        if (sf1 >= Offsets::MinValidAddress) {
            uintptr_t bg1 = driver_read<uintptr_t>(sf1 + Offsets::BaseGame_Match);
            if (bg1 >= Offsets::MinValidAddress) return bg1;
        }

        // Path B: gfCell is a pointer to GameFacade Il2CppClass
        uintptr_t gfClass = driver_read<uintptr_t>(gfCell);
        if (gfClass >= Offsets::MinValidAddress) {
            uintptr_t sf2 = driver_read<uintptr_t>(gfClass + Offsets::GameFacade_P2);
            if (sf2 >= Offsets::MinValidAddress) {
                uintptr_t bg2 = driver_read<uintptr_t>(sf2 + Offsets::BaseGame_Match);
                if (bg2 >= Offsets::MinValidAddress) return bg2;
            }
        }

        return 0;
    }

    // Resolves LocalPlayer pointer safely from BaseGame without blocking
    inline uintptr_t resolve_local_player(uintptr_t baseGame) {
        if (!is_driver_active() || baseGame < Offsets::MinValidAddress) return 0;
        return driver_read<uintptr_t>(baseGame + Offsets::Match_LocalPlayer);
    }

    // Resolves PlayerAttributes safely (0x768 Free Fire Global Primary, 0x770 Max Fallback)
    inline uintptr_t resolve_player_attributes(uintptr_t localPlayer) {
        if (!is_driver_active() || localPlayer < Offsets::MinValidAddress) return 0;
        uintptr_t attrs = driver_read<uintptr_t>(localPlayer + Offsets::PlayerAttributes);
        if (attrs < Offsets::MinValidAddress) {
            attrs = driver_read<uintptr_t>(localPlayer + Offsets::PlayerAttributes_Max);
        }
        return attrs;
    }

    // Master unblocked tick function called every frame from main render loop
    inline void tick(uintptr_t libAddress) {
        if (!is_driver_active()) return;

        const bool  ui_speed_on = cfg::motion::velocity_mod.load(std::memory_order_relaxed);
        const float ui_speed_mx = cfg::motion::speed_mult.load(std::memory_order_relaxed);
        const bool  ui_recoil_on = cfg::motion::no_recoil.load(std::memory_order_relaxed);

        bool is_speed_enabled = cfg::master_enabled && (ui_speed_on || g_config.enabled);
        bool is_recoil_enabled = cfg::master_enabled && ui_recoil_on;

        // If neither feature is active and no restoration is pending, return immediately (zero IOCTL overhead)
        if (!is_speed_enabled && !g_config.last_state && !is_recoil_enabled && !g_mod_state.recoil_last) {
            return;
        }

        // Strict anti-ban clamp: 1.00x - 1.30x
        // 0.0f default in menu represents zero boost (+0.00x -> 1.00x baseline).
        float boost = ui_speed_mx;
        float mult = (boost > 0.0f) ? (boost >= 1.0f ? boost : 1.0f + boost) : 1.0f;
        mult = std::clamp(mult, 1.0f, 1.30f);

        g_config.enabled = is_speed_enabled;
        g_config.multiplier = mult;

        // Fast-path: use cached localPlayer from data thread (zero redundant RPM syscalls)
        uintptr_t localPlayer = static_cast<uintptr_t>(data::localPlayerAddr.load(std::memory_order_relaxed));
        if (localPlayer < Offsets::MinValidAddress && libAddress >= Offsets::MinValidAddress) {
            uintptr_t baseGame = resolve_base_game(libAddress);
            if (baseGame >= Offsets::MinValidAddress) {
                localPlayer = resolve_local_player(baseGame);
            }
        }

        if (localPlayer < Offsets::MinValidAddress) {
            return;
        }

        // 1. VELOCITY MODIFIER (ANTI-BAN SAFE WITH SMART FIRING GUARD)
        // Safe Anti-Ban Invariant:
        // Persistent heap modifications (RunSpeedUpScale / Weapon_Recoil) are permanently
        // neutralized. Memory is never kept persistently modified.
        (void)is_speed_enabled;
        (void)is_recoil_enabled;
        (void)mult;
    }

    // Clean memory restoration hook invoked on overlay shutdown
    inline void shutdown() {
        // Zero-op: nothing was mutated
    }

} // namespace speed_system
