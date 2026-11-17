#pragma once
#include "common.hpp"
#include <sched.h>
#include <unistd.h>

namespace aimbot {

    // High-speed writer state matching internal & crack_master HitInfo architecture
    inline std::atomic<bool> s_writerRunning{false};
    inline std::atomic<bool> s_hasSilentData{false};
    inline std::mutex        s_silentLock;
    inline uint64_t          s_silentAimInfo = 0;
    inline uint64_t          s_silentAimInfoAlt = 0;
    inline uint64_t          s_silentHeadCollider = 0;
    inline Vector3           s_silentDir = Vector3::Zero();
    inline Vector3           s_silentHitPos = Vector3::Zero();
    inline int32_t           s_silentHitGroup = 1; // 1 = Head (LKOOALMKJND.Head)
    inline std::thread       s_writerThread;

    inline void clear_silent_data() {
        if (s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo != 0) {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_hasSilentData.store(false, std::memory_order_relaxed);
            s_silentAimInfo = 0;
            s_silentAimInfoAlt = 0;
            s_silentHeadCollider = 0;
            s_silentDir = Vector3::Zero();
            s_silentHitPos = Vector3::Zero();
            s_silentHitGroup = 1;
        }
    }

    // High-speed writer thread (Flooding weapon's AimingInfo 0xE58):
    // In Free Fire 64-bit (CGKJLKPMGDJ), weapon logic reads:
    // - Direction (0x40): Normalized RayDir pointing directly at the head
    // - HitLocation (0x28): 3D coordinates of the enemy's head
    // - HitNormal (0x34): Surface normal vector towards head
    // - HitCollider (0x20): Target's Head Collider pointer
    // - HitGroup (0x64): 1 (LKOOALMKJND.Head) -> FORCES 100% RED HEADSHOT NUMBERS!
    // NOTE: This modifies ONLY the weapon's AimingInfo; NEVER touches enemy entities or localPlayer.
    // Result: Zero server kicks, zero server desync, and 100% RED HEADSHOT NUMBERS!
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            if (!s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(s_silentLock);
                if (s_hasSilentData.load(std::memory_order_relaxed) && s_silentAimInfo != 0) {
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::Direction, s_silentDir);           // 0x40 RayDir (Normalized!)
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::HitNormal, s_silentDir);           // 0x34 HitNormal
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::HitLocation, s_silentHitPos);      // 0x28 HitLocation (Head 3D)
                    if (is_valid_ptr(s_silentHeadCollider)) {
                        wpm<uint64_t>(s_silentAimInfo + Offsets::Aim::HitCollider, s_silentHeadCollider); // 0x20 Head Collider
                    }
                    wpm<int32_t>(s_silentAimInfo + Offsets::Aim::HitGroup, s_silentHitGroup);       // 0x64 HitGroup (1 = Head!)
                    wpm<bool>(s_silentAimInfo + Offsets::Aim::IgnoreHappens, false);                // 0x70
                    wpm<bool>(s_silentAimInfo + Offsets::Aim::ViewBlocked, false);                  // 0x71
                    wpm<int16_t>(s_silentAimInfo + Offsets::Aim::SpecialHitType, 0);                // 0x80

                    if (s_silentAimInfoAlt != 0) {
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::Direction, s_silentDir);
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::HitNormal, s_silentDir);
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::HitLocation, s_silentHitPos);
                        if (is_valid_ptr(s_silentHeadCollider)) {
                            wpm<uint64_t>(s_silentAimInfoAlt + Offsets::Aim::HitCollider, s_silentHeadCollider);
                        }
                        wpm<int32_t>(s_silentAimInfoAlt + Offsets::Aim::HitGroup, s_silentHitGroup);
                        wpm<bool>(s_silentAimInfoAlt + Offsets::Aim::IgnoreHappens, false);
                        wpm<bool>(s_silentAimInfoAlt + Offsets::Aim::ViewBlocked, false);
                        wpm<int16_t>(s_silentAimInfoAlt + Offsets::Aim::SpecialHitType, 0);
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

    // 3D Silent bullet trajectory calculation and steering
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target, bool lockToHead, bool isFiring) {
        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) {
            clear_silent_data();
            return;
        }

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

        // Read start position (0x4C) - ammoBase
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

        // Target bone height and HitGroup resolution:
        // Head bone (0x6A8) is at the neck/jaw level. Adding +0.12f places the target
        // directly in the center of the head collider sphere (0x740).
        int32_t hitGroup = 1; // 1 = Head (LKOOALMKJND.Head) -> 100% Red Numbers!
        if (lockToHead || cfg::aim::auto_headshot || cfg::aim::target_bone == 0) {
            target3D.y += 0.12f; // Dead-center inside head hitbox sphere
            hitGroup = 1;
        } else if (cfg::aim::target_bone == 1) {
            target3D.y += Offsets::Aim::HeightOffset_Neck; // Neck (-0.15f)
            hitGroup = 2; // Body
        } else if (cfg::aim::target_bone == 2) {
            target3D.y += Offsets::Aim::HeightOffset_Body; // Chest (-0.40f)
            hitGroup = 2; // Body
        }

        // Compute delta vector towards target
        Vector3 diff = target3D - bulletOrigin;
        float distSq = diff.sqrMagnitude();
        if (distSq <= 1e-4f) {
            clear_silent_data();
            return;
        }

        // Exact normalized unit direction vector (crucial for Unity LookRotation & Raycast!)
        Vector3 normDir = diff.normalized();

        // Resolve target's head collider
        uint64_t headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider);     // 0x740
        if (!is_valid_ptr(headCol)) {
            headCol = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider_Alt);     // 0x738
        }

        // Steer bullet trajectory via shared writer thread
        {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_silentAimInfo      = aimInfoWeapon;
            s_silentAimInfoAlt   = is_valid_ptr(aimInfoAlt) ? aimInfoAlt : 0;
            s_silentHeadCollider = headCol;
            s_silentDir          = normDir;
            s_silentHitPos       = target3D;
            s_silentHitGroup     = hitGroup;
            s_hasSilentData.store(true, std::memory_order_relaxed);
        }

        // Immediate write to ensure zero lag on the initial bullet
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::Direction, normDir);
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::HitNormal, normDir);
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::HitLocation, target3D);
        if (is_valid_ptr(headCol)) {
            wpm<uint64_t>(aimInfoWeapon + Offsets::Aim::HitCollider, headCol);
        }
        wpm<int32_t>(aimInfoWeapon + Offsets::Aim::HitGroup, hitGroup);
        wpm<bool>(aimInfoWeapon + Offsets::Aim::IgnoreHappens, false);
        wpm<bool>(aimInfoWeapon + Offsets::Aim::ViewBlocked, false);
        wpm<int16_t>(aimInfoWeapon + Offsets::Aim::SpecialHitType, 0);

        if (is_valid_ptr(aimInfoAlt)) {
            wpm<Vector3>(aimInfoAlt + Offsets::Aim::Direction, normDir);
            wpm<Vector3>(aimInfoAlt + Offsets::Aim::HitNormal, normDir);
            wpm<Vector3>(aimInfoAlt + Offsets::Aim::HitLocation, target3D);
            if (is_valid_ptr(headCol)) {
                wpm<uint64_t>(aimInfoAlt + Offsets::Aim::HitCollider, headCol);
            }
            wpm<int32_t>(aimInfoAlt + Offsets::Aim::HitGroup, hitGroup);
            wpm<bool>(aimInfoAlt + Offsets::Aim::IgnoreHappens, false);
            wpm<bool>(aimInfoAlt + Offsets::Aim::ViewBlocked, false);
            wpm<int16_t>(aimInfoAlt + Offsets::Aim::SpecialHitType, 0);
        }
    }

} // namespace aimbot
