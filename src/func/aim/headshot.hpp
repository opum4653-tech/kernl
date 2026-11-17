#pragma once
#include "common.hpp"

namespace aimbot {

    // Accurate Free Fire sniper weapon IDs (AWM, Kar98k, M82B, M24, VSK94, etc.)
    inline bool is_sniper_id(int id) {
        return (id == 4 || id == 30060 || id == 65 || id == 21 || id == 64 || id == 128 || id == 129 || id == 45 || id == 75 || id == 197);
    }

    inline uint64_t& get_last_headshot_target() {
        static uint64_t s_target = 0;
        return s_target;
    }
    inline uint64_t& get_orig_headshot_col() {
        static uint64_t s_orig = 0;
        return s_orig;
    }
    inline bool& get_orig_headshot_captured() {
        static bool s_captured = false;
        return s_captured;
    }

    // Restores original LockedAimingCollider pointer on previously modified target
    inline void restore_headshot_override() {
        uint64_t& lastTarget = get_last_headshot_target();
        uint64_t& origCol    = get_orig_headshot_col();
        bool& captured       = get_orig_headshot_captured();

        if (captured && is_valid_ptr(lastTarget)) {
            wpm<uint64_t>(lastTarget + Offsets::Player::LockedAimingCollider, origCol);
            wpm<uint64_t>(lastTarget + Offsets::Player::LockedAimingCollider_Alt, origCol);
        }
        lastTarget = 0;
        origCol = 0;
        captured = false;
    }

    // Evaluates headshot rate roll (0% - 100%) and configures head collider overrides
    inline bool evaluate_headshot(const PlayerData& target, bool isScoping) {
        float rate = cfg::aim::headshot_rate;
        bool isSniperShot = (cfg::aim::sniper_aim && (isScoping || is_sniper_id(target.weaponType)));
        bool lockToHead = false;

        if (cfg::aim::auto_headshot || isSniperShot || rate >= 100.0f) {
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

        // AIM COLLIDER OVERRIDE & REVERSE PATH:
        // Set LockedAimingCollider (0x80 & 0x78 on AttackableEntity) to the enemy's Head Collider (0x740 / 0x738)
        // With strict original value capture and restoration when unlocked or switching targets.
        uint64_t& lastTarget = get_last_headshot_target();
        uint64_t& origCol    = get_orig_headshot_col();
        bool& captured       = get_orig_headshot_captured();

        if (lockToHead && is_valid_ptr(target.addr)) {
            if (lastTarget != target.addr) {
                if (captured && is_valid_ptr(lastTarget)) {
                    wpm<uint64_t>(lastTarget + Offsets::Player::LockedAimingCollider, origCol);
                    wpm<uint64_t>(lastTarget + Offsets::Player::LockedAimingCollider_Alt, origCol);
                }
                origCol = rpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider);
                captured = true;
                lastTarget = target.addr;
            }

            uint64_t headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider);     // 0x740
            if (!is_valid_ptr(headCol)) {
                headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider_Alt); // 0x738
            }
            if (is_valid_ptr(headCol)) {
                wpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider, headCol);     // 0x80
                wpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider_Alt, headCol); // 0x78

                uint64_t localPlayer = data::localPlayerAddr.load(std::memory_order_relaxed);
                if (is_valid_ptr(localPlayer)) {
                    wpm<uint64_t>(localPlayer + Offsets::Player::LockedAimingCollider, headCol);
                    wpm<uint64_t>(localPlayer + Offsets::Player::LockedAimingCollider_Alt, headCol);
                }
            }
        } else {
            if (captured && is_valid_ptr(lastTarget)) {
                wpm<uint64_t>(lastTarget + Offsets::Player::LockedAimingCollider, origCol);
                wpm<uint64_t>(lastTarget + Offsets::Player::LockedAimingCollider_Alt, origCol);
                lastTarget = 0;
                origCol = 0;
                captured = false;
            }
        }

        return lockToHead;
    }

} // namespace aimbot
