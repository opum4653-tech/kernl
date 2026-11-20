#pragma once
#include "common.hpp"

// ==============================================================================================
// FILE: src/func/aim/target.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Target acquisition, FOV boundary enforcement, and anti-jitter target hysteresis engine.
//
// CROSS-FILE CALL GRAPH & CONTRACTS:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/func/aim/aim.hpp -> trajectory::update():
//      * Calls acquire_target(localPlayer) on every aim worker cycle.
//
// 2. DATA SOURCES & DEPENDENCIES:
//    - Local player memory: reads IsDead (0x7C), IsFiring (0x850), Camera FOVOffset (0x84).
//    - Parsed enemy repository: data::players (from src/game/data.hpp).
//    - User settings: cfg::aim::get_fov_radius(), cfg::aim::ignore_knocked, cfg::aim::scope_aim.
//
// 3. TARGET HYSTERESIS (STICKINESS ALGORITHM):
//    - Problem: In high-density combat, two enemies crossing each other causes the targeting to flick
//      rapidly back and forth, resulting in erratic screen shaking and missed shots.
//    - Solution: Once a target is acquired (s_lockedTargetAddr), the engine maintains a sticky lock
//      on that specific entity with an expanded 1.35x FOV boundary until they die, leave the FOV,
//      or the trigger is released past the grace period (s_lockTimeoutTicks).
// ==============================================================================================

namespace trajectory {

    // Output contract containing target evaluation metadata
    struct TargetAcquisitionResult {
        PlayerData target;                      // Selected enemy entity record
        Vector3 bestScreenPos = Vector3::Zero();// 2D screen coordinate of lock point
        bool isScoping = false;                 // True if player has weapon scoped in
        bool isFiring = false;                  // True if player is holding down fire button
        bool triggerActive = false;             // True if aim conditions (firing or scoping) are met
        bool valid = false;                     // True if a valid locked target was acquired
    };

    // Evaluates the closest, most optimal enemy entity for trajectory engagement
    inline TargetAcquisitionResult acquire_target(uint64_t localPlayer) {
        TargetAcquisitionResult result;
        if (!is_valid_ptr(localPlayer)) return result;

        // 1. Verify local player is alive
        if (rpm<bool>(localPlayer + Offsets::Player::IsDead)) {
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            return result;
        }

        // 2. Firing state evaluation (Checks bool flag at 0x850 / 0x848)
        bool isFiring = rpm<bool>(localPlayer + Offsets::Player::IsFiring);
        if (!isFiring) {
            isFiring = rpm<bool>(localPlayer + Offsets::Player::IsFiring_Alt);
        }
        if (!isFiring) {
            int fireVal = rpm<int>(localPlayer + Offsets::Player::IsFiring);
            if (fireVal > 0) isFiring = true;
        }
        result.isFiring = isFiring;

        // 3. Scoping state evaluation (FollowCamera 0x698 / 0x690 -> FOVOffset 0x84)
        uint64_t followCam = rpm<uint64_t>(localPlayer + Offsets::Player::Camera);
        if (!is_valid_ptr(followCam)) {
            followCam = rpm<uint64_t>(localPlayer + Offsets::Player::Camera_Alt);
        }
        float camFovOffset = 0.0f;
        if (is_valid_ptr(followCam)) {
            camFovOffset = rpm<float>(followCam + Offsets::Player::FOVOffset);
        }
        result.isScoping = (camFovOffset < -0.5f || camFovOffset > 0.5f);
        if (!result.isScoping) {
            uint64_t weapon = rpm<uint64_t>(localPlayer + Offsets::Player::Weapon);
            if (!is_valid_ptr(weapon)) weapon = rpm<uint64_t>(localPlayer + Offsets::Player::Weapon_Alt);
            if (is_valid_ptr(weapon)) {
                uint64_t sightPtr = rpm<uint64_t>(weapon + Offsets::Weapon::IsSight);
                if (sightPtr != 0) result.isScoping = true;
            }
        }

        // 4. Trigger state coordination (Scope Aim triggers trajectory on scope without firing)
        bool triggerActive = isFiring;
        if (cfg::aim::scope_aim && result.isScoping) {
            triggerActive = true;
        }
        result.triggerActive = triggerActive;

        // Lease management: grant 30 grace ticks during firing bursts
        if (triggerActive) {
            s_lockTimeoutTicks = 30; // Active trigger: refresh lock lease
        } else if (s_lockTimeoutTicks > 0) {
            s_lockTimeoutTicks--;    // Grace period across rapid-fire intervals
        }

        // 5. Snapshot thread-safe enemy list
        std::vector<PlayerData> enemies;
        {
            std::lock_guard<std::mutex> lk(data::players_mutex);
            enemies = data::players;
        }

        if (enemies.empty()) {
            s_lockedTargetAddr = 0;
            return result;
        }

        float screenCx = g_sw * 0.5f;
        float screenCy = g_sh * 0.5f;
        float fovLimit = cfg::aim::get_fov_radius();
        bool foundLocked = false;

        // 6. PHASE 1: Target Hysteresis (Sticky lock while inside expanded 1.35x FOV circle)
        if (s_lockedTargetAddr != 0) {
            for (const auto& enemy : enemies) {
                if (enemy.addr == s_lockedTargetAddr) {
                    if (enemy.curHP > 0 && enemy.Head.z >= 0.05f &&
                        (!cfg::aim::ignore_knocked || (!enemy.isKnocked && enemy.curHP > 1))) {
                        float dxHead = enemy.Head.x - screenCx;
                        float dyHead = enemy.Head.y - screenCy;
                        float distHead = sqrtf(dxHead * dxHead + dyHead * dyHead);

                        bool hasToe = (enemy.Toe.z >= 0.05f);
                        float bodyX = hasToe ? ((enemy.Head.x + enemy.Toe.x) * 0.5f) : enemy.Head.x;
                        float bodyY = hasToe ? ((enemy.Head.y + enemy.Toe.y) * 0.5f) : (enemy.Head.y + 30.0f);
                        float dxBody = bodyX - screenCx;
                        float dyBody = bodyY - screenCy;
                        float distBody = sqrtf(dxBody * dxBody + dyBody * dyBody);

                        float dist = std::min(distHead, distBody);
                        if (fovLimit <= 0.0f || dist <= fovLimit * 1.35f) {
                            result.target = enemy;
                            result.bestScreenPos = enemy.Head;
                            result.valid = true;
                            foundLocked = true;
                        }
                    }
                    break;
                }
            }

            if (!foundLocked) {
                s_lockedTargetAddr = 0;
            }
        }

        // 7. PHASE 2: New Target Acquisition (Closest to screen center within FOV circle)
        if (!foundLocked) {
            float bestScore = 9999999.0f;
            const PlayerData* bestEnemy = nullptr;

            for (const auto& enemy : enemies) {
                // Reject invalid screen depths or dead enemies
                if (enemy.Head.z < 0.05f || enemy.curHP <= 0) continue;

                // Filter knocked players (isKnocked flag or HP <= 1) if toggle is enabled
                if (cfg::aim::ignore_knocked && (enemy.isKnocked || enemy.curHP <= 1)) continue;

                float dxHead = enemy.Head.x - screenCx;
                float dyHead = enemy.Head.y - screenCy;
                float distHead = sqrtf(dxHead * dxHead + dyHead * dyHead);

                bool hasToe = (enemy.Toe.z >= 0.05f);
                float bodyX = hasToe ? ((enemy.Head.x + enemy.Toe.x) * 0.5f) : enemy.Head.x;
                float bodyY = hasToe ? ((enemy.Head.y + enemy.Toe.y) * 0.5f) : (enemy.Head.y + 30.0f);
                float dxBody = bodyX - screenCx;
                float dyBody = bodyY - screenCy;
                float distBody = sqrtf(dxBody * dxBody + dyBody * dyBody);

                float crosshairDist = std::min(distHead, distBody);

                // Reject targets outside configured FOV circle limit
                if (fovLimit > 0.0f && crosshairDist > fovLimit) continue;

                if (crosshairDist < bestScore) {
                    bestScore = crosshairDist;
                    bestEnemy = &enemy;
                    result.bestScreenPos = enemy.Head;
                }
            }

            if (bestEnemy) {
                result.target = *bestEnemy;
                result.valid = true;
                s_lockedTargetAddr = result.target.addr;
                s_lockTimeoutTicks = 30;
            }
        }

        if (!result.valid) {
            s_lockedTargetAddr = 0;
            s_smoothDir = Vector3::Zero();
        }

        return result;
    }

} // namespace trajectory
