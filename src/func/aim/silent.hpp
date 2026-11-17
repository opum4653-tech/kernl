#pragma once
#include "common.hpp"
#include <sched.h>
#include <unistd.h>

namespace aimbot {

    // Shared high-speed writer state matching NXL & crack_master HitInfo architecture
    inline std::atomic<bool> s_writerRunning{false};
    inline std::atomic<bool> s_hasSilentData{false};
    inline std::mutex        s_silentLock;
    inline uint64_t          s_silentAimInfo = 0;
    inline uint64_t          s_silentAimInfoAlt = 0;
    inline uint64_t          s_silentTargetAddr = 0;
    inline uint64_t          s_silentHeadCollider = 0;
    inline Vector3           s_silentDir = Vector3::Zero();
    inline Vector3           s_silentHitPos = Vector3::Zero();
    inline Vector3           s_silentOrigin = Vector3::Zero();
    inline int32_t           s_silentHitGroup = 1;
    inline std::thread       s_writerThread;

    inline void clear_silent_data() {
        if (s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo != 0) {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_hasSilentData.store(false, std::memory_order_relaxed);
            s_silentAimInfo = 0;
            s_silentAimInfoAlt = 0;
            s_silentTargetAddr = 0;
            s_silentHeadCollider = 0;
            s_silentDir = Vector3::Zero();
            s_silentHitPos = Vector3::Zero();
            s_silentOrigin = Vector3::Zero();
            s_silentHitGroup = 1;
        }
    }

    // High-speed writer thread (Producer/Consumer Flooding):
    // In Free Fire 64-bit (CGKJLKPMGDJ), weapon logic in Update() continuously reads
    // Direction (0x40), HitNormal (0x34), HitLocation (0x28), HitCollider (0x20), and HitGroup (0x64: 1=Head).
    // Flooding during lock & firing guarantees that when the game fires and calculates bullet damage,
    // it registers an instant 100% red-number Headshot hit.
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            if (!s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(s_silentLock);
                if (s_hasSilentData.load(std::memory_order_relaxed) && s_silentAimInfo != 0) {
                    // 1. Primary AimInfo (0xE58: LastAimingInfoFromWeapon)
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::Direction, s_silentDir);           // 0x40 RayDir (Normalized!)
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::HitNormal, s_silentHitPos);        // 0x34 HitNormal (Target head location as internal)
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::HitLocation, s_silentHitPos);      // 0x28 HitLocation (Head 3D)
                    wpm<Vector3>(s_silentAimInfo + 0x74, s_silentOrigin);                           // 0x74 OrigStartPosition
                    if (s_silentHeadCollider != 0) {
                        wpm<uint64_t>(s_silentAimInfo + Offsets::Aim::HitObject, s_silentHeadCollider);   // 0x18 HitObject
                        wpm<uint64_t>(s_silentAimInfo + Offsets::Aim::HitCollider, s_silentHeadCollider); // 0x20 Head Collider
                    }
                    wpm<int32_t>(s_silentAimInfo + Offsets::Aim::HitGroup, s_silentHitGroup);       // 0x64 HitGroup (1 = Head!)
                    wpm<bool>(s_silentAimInfo + Offsets::Aim::IgnoreHappens, false);                // 0x70 IgnoreHappens = false
                    wpm<bool>(s_silentAimInfo + Offsets::Aim::ViewBlocked, false);                  // 0x71 ViewBlocked = false
                    wpm<int16_t>(s_silentAimInfo + Offsets::Aim::SpecialHitType, 0);                // 0x80 SpecialHitType = 0

                    // 2. Auxiliary AimInfo (0xE50) if present
                    if (s_silentAimInfoAlt != 0) {
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::Direction, s_silentDir);
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::HitNormal, s_silentHitPos);
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::HitLocation, s_silentHitPos);
                        wpm<Vector3>(s_silentAimInfoAlt + 0x74, s_silentOrigin);
                        if (s_silentHeadCollider != 0) {
                            wpm<uint64_t>(s_silentAimInfoAlt + Offsets::Aim::HitObject, s_silentHeadCollider);
                            wpm<uint64_t>(s_silentAimInfoAlt + Offsets::Aim::HitCollider, s_silentHeadCollider);
                        }
                        wpm<int32_t>(s_silentAimInfoAlt + Offsets::Aim::HitGroup, s_silentHitGroup);
                        wpm<bool>(s_silentAimInfoAlt + Offsets::Aim::IgnoreHappens, false);
                        wpm<bool>(s_silentAimInfoAlt + Offsets::Aim::ViewBlocked, false);
                        wpm<int16_t>(s_silentAimInfoAlt + Offsets::Aim::SpecialHitType, 0);
                    }

                    // 3. Keep target's & localPlayer's LockedAimingCollider locked to head collider
                    if (s_silentHeadCollider != 0) {
                        if (s_silentTargetAddr != 0) {
                            wpm<uint64_t>(s_silentTargetAddr + Offsets::Player::LockedAimingCollider, s_silentHeadCollider);     // 0x80
                            wpm<uint64_t>(s_silentTargetAddr + Offsets::Player::LockedAimingCollider_Alt, s_silentHeadCollider); // 0x78
                        }
                        uint64_t lp = data::localPlayerAddr.load(std::memory_order_relaxed);
                        if (lp != 0) {
                            wpm<uint64_t>(lp + Offsets::Player::LockedAimingCollider, s_silentHeadCollider);
                            wpm<uint64_t>(lp + Offsets::Player::LockedAimingCollider_Alt, s_silentHeadCollider);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
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

        // Silent aim & Auto Headshot stay primed whenever a target is in FOV or weapon is firing.
        // If neither auto_headshot nor silent is active, or if trigger_mode requires firing and no grace period:
        if (!cfg::aim::auto_headshot && !cfg::aim::silent && !isFiring && cfg::aim::trigger_mode == 0 && s_lockTimeoutTicks == 0) {
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

        // Apply target bone height adjustments and HitGroup classification
        int32_t hitGroup = 1; // 1 = Head, 2 = Body (LKOOALMKJND enum)
        if (cfg::aim::auto_headshot || lockToHead) {
            hitGroup = 1; // Strict 100% Headshot (HitGroup = 1)
        } else if (cfg::aim::target_bone == 1) {
            target3D.y += Offsets::Aim::HeightOffset_Neck; // Neck / Upper Chest (-0.15f)
            hitGroup = 2;
        } else if (cfg::aim::target_bone == 2) {
            target3D.y += Offsets::Aim::HeightOffset_Body; // Body / Pelvis (-0.40f)
            hitGroup = 2;
        } else {
            // Target bone 0 (Head):
            hitGroup = 1; // Strict Headshot!
        }

        // Compute delta vector towards target
        Vector3 diff = target3D - bulletOrigin;
        float distSq = diff.sqrMagnitude();
        if (distSq <= 1e-4f) {
            clear_silent_data();
            return;
        }

        // Exact normalized unit direction vector (crucial for Unity Raycast!)
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
            s_silentTargetAddr   = target.addr;
            s_silentHeadCollider = headCol;
            s_silentDir          = normDir;
            s_silentHitPos       = target3D;
            s_silentOrigin       = bulletOrigin;
            s_silentHitGroup     = hitGroup;
            s_hasSilentData.store(true, std::memory_order_relaxed);
        }

        // Immediate write to ensure zero lag on initial bullet
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::Direction, normDir);
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::HitNormal, target3D); // Target Head 3D as internal
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::HitLocation, target3D);
        wpm<Vector3>(aimInfoWeapon + 0x74, bulletOrigin); // OrigStartPosition
        if (is_valid_ptr(headCol)) {
            wpm<uint64_t>(aimInfoWeapon + Offsets::Aim::HitObject, headCol);
            wpm<uint64_t>(aimInfoWeapon + Offsets::Aim::HitCollider, headCol);
        }
        wpm<int32_t>(aimInfoWeapon + Offsets::Aim::HitGroup, hitGroup);
        wpm<bool>(aimInfoWeapon + Offsets::Aim::IgnoreHappens, false);
        wpm<bool>(aimInfoWeapon + Offsets::Aim::ViewBlocked, false);
        wpm<int16_t>(aimInfoWeapon + Offsets::Aim::SpecialHitType, 0);

        // Reset game adjustment flag so Unity engine doesn't overwrite our trajectory
        wpm<bool>(localPlayer + Offsets::Player::AimAdjustmentFlag, false);
    }

} // namespace aimbot
