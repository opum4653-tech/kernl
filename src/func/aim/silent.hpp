#pragma once
#include "common.hpp"
#include <sched.h>
#include <unistd.h>

namespace aimbot {

    // High-speed writer state matching pewpew architecture
    inline std::atomic<bool> s_writerRunning{false};
    inline std::atomic<bool> s_hasSilentData{false};
    inline std::mutex        s_silentLock;
    inline uint64_t          s_silentAimInfo = 0;
    inline uint64_t          s_silentAimInfoAlt = 0;
    inline Vector3           s_silentDir = Vector3::Zero();
    inline std::thread       s_writerThread;

    inline void clear_silent_data() {
        if (s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo != 0) {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_hasSilentData.store(false, std::memory_order_relaxed);
            s_silentAimInfo = 0;
            s_silentAimInfoAlt = 0;
            s_silentDir = Vector3::Zero();
        }
    }

    // High-speed writer thread (pewpew Producer/Consumer architecture):
    // Continuously writes the raycast direction vector (targetPos - ammoBase) to AimInfo + 0x40.
    // Zero entity corruption, zero collider modification, zero unaligned writes.
    // Unity's internal weapon raycast uses this direction, colliding naturally with the enemy's
    // head collider and delivering 100% red numbers without server desync or match disconnection.
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            if (!s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(s_silentLock);
                if (s_hasSilentData.load(std::memory_order_relaxed) && s_silentAimInfo != 0) {
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::Direction, s_silentDir); // 0x40 Raycast Vector
                    if (s_silentAimInfoAlt != 0) {
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::Direction, s_silentDir);
                    }
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

    // 3D Silent bullet trajectory calculation and steering (pewpew delta-ray method)
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target, bool lockToHead, bool isFiring) {
        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) {
            clear_silent_data();
            return;
        }

        // Active when silent aim or auto headshot is enabled
        if (!cfg::aim::silent && !cfg::aim::auto_headshot && !cfg::aim::enable) {
            clear_silent_data();
            return;
        }

        // Primed when firing or actively acquired inside FOV
        if (!isFiring && cfg::aim::trigger_mode == 0 && s_lockTimeoutTicks == 0) {
            clear_silent_data();
            return;
        }

        // Resolve Weapon AimingInfo (0xE58 primary, 0xE50 auxiliary)
        uint64_t aimInfoWeapon = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo);
        uint64_t aimInfoAlt    = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo_Alt);
        if (!is_valid_ptr(aimInfoWeapon)) {
            aimInfoWeapon = aimInfoAlt;
            aimInfoAlt = 0;
        }
        if (!is_valid_ptr(aimInfoWeapon)) {
            clear_silent_data();
            return;
        }

        // Read start position (0x4C) - ammoBase in pewpew
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

        // Apply target bone height adjustments (matching pewpew head_bias architecture)
        if (lockToHead || cfg::aim::auto_headshot) {
            target3D.y += 0.05f; // Head center bias for reliable 100% red numbers
        } else if (cfg::aim::target_bone == 1) {
            target3D.y += Offsets::Aim::HeightOffset_Neck; // Neck (-0.15f)
        } else if (cfg::aim::target_bone == 2) {
            target3D.y += Offsets::Aim::HeightOffset_Body; // Body / Chest (-0.40f)
        } else {
            target3D.y += 0.05f; // Default head bias
        }

        // Compute full delta displacement ray (targetWorld - ammoBase) as in pewpew
        Vector3 dir;
        dir.x = target3D.x - bulletOrigin.x;
        dir.y = target3D.y - bulletOrigin.y;
        dir.z = target3D.z - bulletOrigin.z;

        float distSq = dir.sqrMagnitude();
        if (distSq <= 1e-4f) {
            clear_silent_data();
            return;
        }

        // Steer bullet trajectory via shared writer thread
        {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_silentAimInfo      = aimInfoWeapon;
            s_silentAimInfoAlt   = is_valid_ptr(aimInfoAlt) ? aimInfoAlt : 0;
            s_silentDir          = dir;
            s_hasSilentData.store(true, std::memory_order_relaxed);
        }

        // Immediate write to ensure zero lag on the initial bullet
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::Direction, dir);
        if (is_valid_ptr(aimInfoAlt)) {
            wpm<Vector3>(aimInfoAlt + Offsets::Aim::Direction, dir);
        }
    }

} // namespace aimbot
