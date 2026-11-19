#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "driver.h"
#include "../offsets/offsets.hpp"
#include "../other/addr.hpp"
#include "../ui/cfg.hpp"
#include "../game/data.hpp"

namespace speed_system {

    struct SpeedConfig {
        bool  enabled        = false;
        float multiplier     = 1.20f;
        bool  last_state     = false;
        float last_mult      = 1.0f;
    };

    inline SpeedConfig g_config;

    struct ModState {
        float orig_run_scale = 1.0f;
        bool  orig_player_captured = false;

        bool  recoil_last = false;
        float recoil_orig = 0.0f;
        bool  recoil_orig_captured = false;
    };
    inline ModState g_mod_state;

    // Checks if the kernel driver is active and ready
    inline bool is_driver_active() {
        return (driver != nullptr && driver->is_driver_loaded());
    }

    // Direct kernel driver write wrappers
    inline bool driver_write_float(uintptr_t addr, float val) {
        if (!is_driver_active() || addr < Offsets::MinValidAddress) return false;
        return driver->write(addr, &val, sizeof(float));
    }

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

        // Path A: gfCell is directly GameFacade Il2CppClass (pewpew style)
        uintptr_t sf1 = driver_read<uintptr_t>(gfCell + Offsets::GameFacade_P2);
        if (sf1 >= Offsets::MinValidAddress) {
            uintptr_t cg1 = driver_read<uintptr_t>(sf1 + 0x0);
            uintptr_t cm1 = driver_read<uintptr_t>(sf1 + 0x8);
            if (cg1 >= Offsets::MinValidAddress) return cg1;
            if (cm1 >= Offsets::MinValidAddress) return cm1;
        }

        // Path B: gfCell is a pointer to GameFacade Il2CppClass
        uintptr_t gfClass = driver_read<uintptr_t>(gfCell);
        if (gfClass >= Offsets::MinValidAddress) {
            uintptr_t sf2 = driver_read<uintptr_t>(gfClass + Offsets::GameFacade_P2);
            if (sf2 >= Offsets::MinValidAddress) {
                uintptr_t cg2 = driver_read<uintptr_t>(sf2 + 0x0);
                uintptr_t cm2 = driver_read<uintptr_t>(sf2 + 0x8);
                if (cg2 >= Offsets::MinValidAddress) return cg2;
                if (cm2 >= Offsets::MinValidAddress) return cm2;
            }
        }

        return 0;
    }

    // Resolves LocalPlayer pointer safely from BaseGame without blocking
    inline uintptr_t resolve_local_player(uintptr_t baseGame) {
        if (!is_driver_active() || baseGame < Offsets::MinValidAddress) return 0;

        uintptr_t match = driver_read<uintptr_t>(baseGame + Offsets::BaseGame_Match);
        if (match < Offsets::MinValidAddress) return 0;

        return driver_read<uintptr_t>(match + Offsets::Match_LocalPlayer);
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

    // Master unblocked tick function called every frame
    inline void tick(uintptr_t libAddress) {
        if (!is_driver_active()) return;

        const bool  ui_speed_on = cfg::motion::speed_hack.load(std::memory_order_relaxed);
        const float ui_speed_mx = cfg::motion::speed_mult.load(std::memory_order_relaxed);
        const bool  ui_recoil_on = cfg::motion::no_recoil.load(std::memory_order_relaxed);

        bool is_speed_enabled = cfg::master_enabled && (ui_speed_on || g_config.enabled);
        bool is_recoil_enabled = cfg::master_enabled && ui_recoil_on;

        // If neither feature is active and no restoration is pending, return immediately (zero IOCTL overhead)
        if (!is_speed_enabled && !g_config.last_state && !is_recoil_enabled && !g_mod_state.recoil_last) {
            return;
        }

        float mult = (ui_speed_on && ui_speed_mx >= 1.0f) ? ui_speed_mx : g_config.multiplier;
        mult = std::clamp(mult, 1.0f, 1.30f); // Strict anti-ban clamp: 1.00x - 1.30x

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

        // 1. SPEED HACK (ANTI-BAN SAFE)
        if (is_speed_enabled) {
            uintptr_t attrs = resolve_player_attributes(localPlayer);
            if (attrs >= Offsets::MinValidAddress) {
                if (!g_mod_state.orig_player_captured) {
                    float curRun = driver_read<float>(attrs + Offsets::RunSpeedUpScale);
                    if (curRun >= 0.5f && curRun <= 2.5f) g_mod_state.orig_run_scale = curRun;
                    g_mod_state.orig_player_captured = true;
                }

                // Smart Firing Guard: drops to 1.0x baseline while shooting for 100% spray stability
                int fireVal = driver_read<int>(localPlayer + Offsets::Player::IsFiring);
                if (fireVal <= 0) fireVal = driver_read<int>(localPlayer + Offsets::Player::IsFiring_Alt);
                float activeMult = (fireVal > 0) ? 1.0f : mult;

                driver_write_float(attrs + Offsets::RunSpeedUpScale, activeMult);
            }

            g_config.last_state = true;
            g_config.last_mult = mult;
        }
        // Speed restoration to baseline
        else if (g_config.last_state) {
            uintptr_t attrs = resolve_player_attributes(localPlayer);
            if (attrs >= Offsets::MinValidAddress) {
                driver_write_float(attrs + Offsets::RunSpeedUpScale, g_mod_state.orig_run_scale);
            }

            g_mod_state.orig_player_captured = false;
            g_config.last_state = false;
            g_config.last_mult = 1.0f;
        }

        // 2. NO RECOIL
        if (is_recoil_enabled) {
            uintptr_t weapon = driver_read<uintptr_t>(localPlayer + Offsets::Player_Weapon);
            if (weapon >= Offsets::MinValidAddress) {
                uintptr_t wdata = driver_read<uintptr_t>(weapon + Offsets::Weapon_Data);
                if (wdata >= Offsets::MinValidAddress) {
                    if (!g_mod_state.recoil_orig_captured) {
                        float curRecoil = driver_read<float>(wdata + Offsets::Weapon_Recoil);
                        if (std::isfinite(curRecoil) && curRecoil >= 0.0f && curRecoil <= 100.0f) {
                            g_mod_state.recoil_orig = curRecoil;
                            g_mod_state.recoil_orig_captured = true;
                        }
                    }
                    float zero = 0.0f;
                    driver->write(wdata + Offsets::Weapon_Recoil, &zero, sizeof(float));
                    g_mod_state.recoil_last = true;
                }
            }
        } else if (g_mod_state.recoil_last) {
            uintptr_t weapon = driver_read<uintptr_t>(localPlayer + Offsets::Player_Weapon);
            if (weapon >= Offsets::MinValidAddress) {
                uintptr_t wdata = driver_read<uintptr_t>(weapon + Offsets::Weapon_Data);
                if (wdata >= Offsets::MinValidAddress && g_mod_state.recoil_orig_captured) {
                    float orig = g_mod_state.recoil_orig;
                    driver->write(wdata + Offsets::Weapon_Recoil, &orig, sizeof(float));
                    g_mod_state.recoil_last = false;
                    g_mod_state.recoil_orig_captured = false;
                }
            }
        }
    }

} // namespace speed_system
