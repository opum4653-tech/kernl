#pragma once
#include "common.hpp"

namespace aimbot {

    // Accurate Free Fire sniper weapon IDs (AWM, Kar98k, M82B, M24, VSK94, etc.)
    inline bool is_sniper_id(int id) {
        return (id == 4 || id == 30060 || id == 65 || id == 21 || id == 64 || id == 128 || id == 129 || id == 45 || id == 75 || id == 197);
    }

    // Evaluates headshot rate roll (0% - 100%) and configures head collider overrides
    inline bool evaluate_headshot(const PlayerData& target, bool isScoping) {
        float rate = cfg::aim::headshot_rate;
        bool isSniperShot = (cfg::aim::sniper_aim && (isScoping || is_sniper_id(target.weaponType)));
        bool lockToHead = false;

        if (isSniperShot || rate >= 100.0f) {
            lockToHead = true;
        } else if (rate <= 0.0f) {
            lockToHead = false;
        } else {
            // Independent xorshift RNG roll against user's headshot_rate
            static uint32_t s_rollRng =
                0x9E3779B9u ^ static_cast<uint32_t>(time(nullptr)) ^
                (static_cast<uint32_t>(getpid()) * 2654435761u);
            s_rollRng ^= s_rollRng << 13;
            s_rollRng ^= s_rollRng >> 17;
            s_rollRng ^= s_rollRng << 5;
            lockToHead = ((s_rollRng % 100u) < static_cast<uint32_t>(rate));
        }

        // NOTE: In Free Fire 64-bit (Global), writing to target.addr + 0x78 or 0x80
        // directly corrupts target.addr + 0x7C (Player::IsDead flag) and component pointers,
        // causing data.cpp to immediately flag the target as DEAD and wipe it from ESP.
        // True Silent Aim steering occurs via client-side trajectory calculation (apply_trajectory)
        // targeting target.headWorld without mutating enemy memory.
        return lockToHead;
    }

} // namespace aimbot
