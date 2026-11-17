#pragma once
#include "common.hpp"

namespace aimbot {

    // Accurate Free Fire sniper weapon IDs (AWM, Kar98k, M82B, M24, VSK94, etc.)
    inline bool is_sniper_id(int id) {
        return (id == 4 || id == 30060 || id == 65 || id == 21 || id == 64 || id == 128 || id == 129 || id == 45 || id == 75 || id == 197);
    }

    // Pure safe state - no memory modification on entities needed (matches pewpew safety model)
    inline void restore_headshot_override() {
        // Zero entity memory footprint
    }

    // Evaluates headshot rate roll (0% - 100%)
    // Pure logic calculation without modifying enemy entity colliders (zero desync/kick)
    inline bool evaluate_headshot(const PlayerData& target, bool isScoping) {
        float rate = cfg::aim::headshot_rate;
        bool isSniperShot = (cfg::aim::sniper_aim && (isScoping || is_sniper_id(target.weaponType)));

        if (cfg::aim::auto_headshot || isSniperShot || rate >= 100.0f) {
            return true;
        }
        if (rate <= 0.0f) {
            return false;
        }

        // Independent xorshift RNG roll against user's headshot_rate
        static uint32_t s_rollRng =
            0x9E3779B9u ^ static_cast<uint32_t>(time(nullptr)) ^
            (static_cast<uint32_t>(getpid()) * 2654435761u);
        s_rollRng ^= s_rollRng << 13;
        s_rollRng ^= s_rollRng >> 17;
        s_rollRng ^= s_rollRng << 5;
        return ((s_rollRng % 100u) < static_cast<uint32_t>(rate));
    }

} // namespace aimbot
