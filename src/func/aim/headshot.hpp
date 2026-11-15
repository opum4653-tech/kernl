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

        // AIM COLLIDER OVERRIDE:
        // Set LockedAimingCollider (0x80 on AttackableEntity) to the enemy's Head Collider (0x740 / 0x738)
        // Verified from ff-global-dump (1).cs:
        // Line 1051012: Boolean CGJLEBOGBNP; // 0x7c (IsDead)
        // Line 1051013: Collider <AONPKGBGHHI>k__BackingField; // 0x80 (LockedAimingCollider)
        // Line 1036437: Collider BANPEGJEAKE; // 0x738
        // Line 1036438: GMOCOOEIFMK BKEPAKCAGNJ; // 0x740 (Head Collider)
        // Writing an 8-byte pointer to 0x80 is perfectly aligned (0x80..0x87) and preserves 0x7C!
        if (lockToHead && is_valid_ptr(target.addr)) {
            uint64_t headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider);     // 0x740
            if (!is_valid_ptr(headCol)) {
                headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider_Alt); // 0x738
            }
            if (is_valid_ptr(headCol)) {
                wpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider, headCol);     // 0x80
            }
        }

        return lockToHead;
    }

} // namespace aimbot
