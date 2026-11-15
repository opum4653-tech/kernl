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

        // AIM COLLIDER OVERRIDE (Reference-aligned hit registration from STORM_MODS & internal)
        // Reads HeadCollider (0x740 primary, 0x738 fallback) and writes it to BOTH:
        // - LockedAimingCollider (0x80)
        // - LockedAimingCollider_Alt (0x78)
        // Ensures the game engine's internal raycast registers bullet impacts as Headshots.
        if (lockToHead && is_valid_ptr(target.addr)) {
            uint64_t headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider);     // 0x740
            if (!is_valid_ptr(headCol)) {
                headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider_Alt); // 0x738
            }
            if (is_valid_ptr(headCol)) {
                wpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider, headCol);     // 0x80
                wpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider_Alt, headCol); // 0x78
            }
        }

        return lockToHead;
    }

} // namespace aimbot
