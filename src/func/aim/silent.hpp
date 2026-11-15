#pragma once
#include "common.hpp"
#include <sched.h>
#include <unistd.h>

namespace aimbot {

    // Shared high-speed writer state matching pewpew & crack_master hook architecture
    inline std::atomic<bool> s_writerRunning{false};
    inline std::atomic<bool> s_hasSilentData{false};
    inline std::mutex        s_silentLock;
    inline uint64_t          s_silentAimInfo = 0;
    inline Vector3           s_silentDir = Vector3::Zero();
    inline std::thread       s_writerThread;

    inline void clear_silent_data() {
        if (s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo != 0) {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_hasSilentData.store(false, std::memory_order_relaxed);
            s_silentAimInfo = 0;
        }
    }

    // High-speed writer thread (Producer/Consumer Flooding):
    // In Free Fire 64-bit, weapon logic in Update() continuously writes camera aim into AimingInfo+0x40.
    // Flooding during firing guarantees the game's bullet spawn code reads our target direction vector.
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            if (!s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(s_silentLock);
                if (s_hasSilentData.load(std::memory_order_relaxed) && s_silentAimInfo != 0) {
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::Direction, s_silentDir);
                    wpm<bool>(s_silentAimInfo + 0x71, false);
                }
            }
            sched_yield();
        }
    }

    inline void start_silent_writer() {
        if (s_writerRunning.exchange(true)) return;
        s_writerThread = std::thread(writer_loop);
    }

    inline void stop_silent_writer() {
        if (!s_writerRunning.exchange(false)) return;
        clear_silent_data();
        if (s_writerThread.joinable()) {
            s_writerThread.join();
        }
    }

    // 3D Silent bullet trajectory calculation and steering
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target, bool lockToHead, bool isFiring) {
        if (!is_valid_ptr(localPlayer)) {
            clear_silent_data();
            return;
        }

        // Trajectory write should only execute when weapon is actively firing (or when trigger mode is Always In FOV)
        if (!isFiring && cfg::aim::trigger_mode != 1) {
            clear_silent_data();
            return;
        }

        // Resolve Weapon AimingInfo (0xE58 primary, 0xE50 auxiliary)
        uint64_t aimInfoWeapon = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo);
        if (!is_valid_ptr(aimInfoWeapon)) {
            aimInfoWeapon = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo_Alt);
        }
        if (!is_valid_ptr(aimInfoWeapon)) {
            clear_silent_data();
            return;
        }

        // Read start position (0x4C). Must be a valid world coordinate.
        Vector3 bulletOrigin = rpm<Vector3>(aimInfoWeapon + Offsets::Aim::StartPosition);
        if (bulletOrigin == Vector3::Zero() || bulletOrigin.sqrMagnitude() < 0.01f || !std::isfinite(bulletOrigin.x)) {
            uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone_Head);
            if (localHead) bulletOrigin = GetNodePosition(localHead);
        }

        if (bulletOrigin == Vector3::Zero() || bulletOrigin.sqrMagnitude() < 0.01f ||
            !std::isfinite(bulletOrigin.x) || !std::isfinite(bulletOrigin.y) || !std::isfinite(bulletOrigin.z)) {
            clear_silent_data();
            return;
        }

        Vector3 target3D = target.headWorld;
        if (target3D == Vector3::Zero() || !std::isfinite(target3D.x) || !std::isfinite(target3D.y) || !std::isfinite(target3D.z)) {
            clear_silent_data();
            return;
        }

        // Apply target bone height adjustments
        if (cfg::aim::target_bone == 1) {
            target3D.y -= 0.15f; // Neck / Upper Chest
        } else if (cfg::aim::target_bone == 2) {
            target3D.y -= 0.40f; // Body / Pelvis
        } else {
            // Target bone 0 (Head):
            if (!lockToHead) {
                target3D.y -= 0.28f; // Chest on non-headshot rolls
            }
        }

        // Compute delta vector towards target
        Vector3 diff = target3D - bulletOrigin;
        if (diff.sqrMagnitude() <= 1e-4f) {
            clear_silent_data();
            return;
        }

        // Steer bullet trajectory
        {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_silentAimInfo = aimInfoWeapon;
            s_silentDir = diff;
            s_hasSilentData.store(true, std::memory_order_relaxed);
        }

        // Immediate write to ensure zero lag on initial bullet
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::Direction, diff);
        wpm<bool>(aimInfoWeapon + 0x71, false);
    }

} // namespace aimbot
