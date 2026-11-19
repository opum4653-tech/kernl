#pragma once
#include "common.hpp"

namespace aimbot {

    struct TargetAcquisitionResult {
        PlayerData target;
        Vector3 bestScreenPos = Vector3::Zero();
        bool isScoping = false;
        bool isFiring = false;
        bool triggerActive = false;
        bool valid = false;
    };

    inline TargetAcquisitionResult acquire_target(uint64_t localPlayer) {
        TargetAcquisitionResult result;
        if (!is_valid_ptr(localPlayer)) return result;

        // Verify local player is alive
        if (rpm<bool>(localPlayer + Offsets::Player::IsDead)) {
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            return result;
        }

        // Firing & Scoping state (4-byte integer alignment matching pewpew)
        int fireVal = rpm<int>(localPlayer + Offsets::Player::IsFiring);
        if (fireVal <= 0) {
            fireVal = rpm<int>(localPlayer + Offsets::Player::IsFiring_Alt);
        }
        bool isFiring = (fireVal > 0);
        result.isFiring = isFiring;

        // FollowCamera is at localPlayer + 0x690. Inside FollowCamera, FOVOffset is at 0x84
        uint64_t followCam = rpm<uint64_t>(localPlayer + Offsets::Player::Camera);
        float camFovOffset = 0.0f;
        if (is_valid_ptr(followCam)) {
            camFovOffset = rpm<float>(followCam + Offsets::Player::FOVOffset);
        }
        result.isScoping = (camFovOffset < -0.5f || camFovOffset > 0.5f);

        bool triggerActive = isFiring;
        if (cfg::aim::scope_aim && result.isScoping) {
            triggerActive = true;
        }
        result.triggerActive = triggerActive;

        if (triggerActive) {
            s_lockTimeoutTicks = 30; // Active trigger: refresh lock lease
        } else if (s_lockTimeoutTicks > 0) {
            s_lockTimeoutTicks--;    // Grace period across rapid-fire intervals
        }

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

        // 1. Target Hysteresis (Sticky lock while inside FOV circle)
        if (s_lockedTargetAddr != 0) {
            for (const auto& enemy : enemies) {
                if (enemy.addr == s_lockedTargetAddr) {
                    if (enemy.curHP > 0 && enemy.Head.z >= 0.05f &&
                        (!cfg::aim::ignore_knocked || enemy.curHP > 1)) {
                        float dxHead = enemy.Head.x - screenCx;
                        float dyHead = enemy.Head.y - screenCy;
                        float distHead = sqrtf(dxHead * dxHead + dyHead * dyHead);

                        float bodyX = (enemy.Head.x + enemy.Toe.x) * 0.5f;
                        float bodyY = (enemy.Head.y + enemy.Toe.y) * 0.5f;
                        float dxBody = bodyX - screenCx;
                        float dyBody = bodyY - screenCy;
                        float distBody = sqrtf(dxBody * dxBody + dyBody * dyBody);

                        float dist = std::min(distHead, distBody);
                        if (dist <= fovLimit * 1.35f) {
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

        // 2. New Target Acquisition (Closest to screen center within FOV circle)
        if (!foundLocked) {
            float bestScore = 9999999.0f;
            const PlayerData* bestEnemy = nullptr;

            for (const auto& enemy : enemies) {
                if (enemy.Head.z < 0.05f || enemy.curHP <= 0) continue;
                if (cfg::aim::ignore_knocked && enemy.curHP <= 1) continue;

                float dxHead = enemy.Head.x - screenCx;
                float dyHead = enemy.Head.y - screenCy;
                float distHead = sqrtf(dxHead * dxHead + dyHead * dyHead);

                float bodyX = (enemy.Head.x + enemy.Toe.x) * 0.5f;
                float bodyY = (enemy.Head.y + enemy.Toe.y) * 0.5f;
                float dxBody = bodyX - screenCx;
                float dyBody = bodyY - screenCy;
                float distBody = sqrtf(dxBody * dxBody + dyBody * dyBody);

                float crosshairDist = std::min(distHead, distBody);
                if (crosshairDist > fovLimit) continue;

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

} // namespace aimbot
