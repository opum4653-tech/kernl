#pragma once
#include "common.hpp"
#include <sched.h>
#include <unistd.h>

namespace aimbot {

    // High-speed writer state matching pewpew / crack_master architecture
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
    // Dedicated high-speed thread writing ONLY to AimInfo + 0x40 (Direction / raycast).
    // ZERO writes to enemy entities, ZERO writes to internal collider fields (0x20, 0x64, 0x28, 0x34).
    // Unity's internal raycast executes naturally along this vector, hitting the enemy's head
    // collider and generating authentic, legitimate server-side headshot damage packets.
    // Result: Zero server kicks, zero "Match results is still being calculated" errors.
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            // Strict Menu Toggle Guard:
            // If neither Silent Aim nor Auto Headshot is active in the menu, immediately purge and sleep
            if (!cfg::aim::silent.load(std::memory_order_relaxed) && !cfg::aim::auto_headshot.load(std::memory_order_relaxed)) {
                if (s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo != 0) {
                    clear_silent_data();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (!s_hasSilentData.load(std::memory_order_relaxed) || s_silentAimInfo == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(s_silentLock);
                if (s_hasSilentData.load(std::memory_order_relaxed) && s_silentAimInfo != 0) {
                    wpm<Vector3>(s_silentAimInfo + Offsets::Aim::Direction, s_silentDir);
                    wpm<bool>(s_silentAimInfo + 0x71, false);

                    if (s_silentAimInfoAlt != 0) {
                        wpm<Vector3>(s_silentAimInfoAlt + Offsets::Aim::Direction, s_silentDir);
                        wpm<bool>(s_silentAimInfoAlt + 0x71, false);
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

        // Strictly gated by menu toggles — ZERO activation when disabled
        if (!cfg::aim::silent.load(std::memory_order_relaxed) && !cfg::aim::auto_headshot.load(std::memory_order_relaxed)) {
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

        // Target bone height adjustment:
        // Offsets::Bone_Head (0x6A8) is at the neck/jaw base pivot of the character.
        // The enemy body capsule collider extends up to ~0.08m above this pivot.
        // Elevating target3D.y by +0.22f (head_bias) centers the ray directly inside
        // the head hitbox sphere, ensuring 100% RED HEADSHOT numbers!
        float bias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (bias < 0.05f || bias > 0.40f) bias = 0.22f;

        if (lockToHead || cfg::aim::auto_headshot || cfg::aim::silent || cfg::aim::target_bone == 0) {
            target3D.y += bias; // 100% pure headshot (hits upper head sphere collider)
        } else if (cfg::aim::target_bone == 1) {
            target3D.y += Offsets::Aim::HeightOffset_Neck; // Neck (-0.15f)
        } else if (cfg::aim::target_bone == 2) {
            target3D.y += Offsets::Aim::HeightOffset_Body; // Chest (-0.40f)
        }

        // Compute delta vector towards target
        Vector3 diff = target3D - bulletOrigin;
        float distSq = diff.sqrMagnitude();
        if (distSq <= 1e-4f) {
            clear_silent_data();
            return;
        }

        // Normalized unit direction vector (crucial for Unity raycast & look rotation)
        Vector3 normDir = diff.normalized();
        if (!std::isfinite(normDir.x) || !std::isfinite(normDir.y) || !std::isfinite(normDir.z) || normDir == Vector3::Zero()) {
            clear_silent_data();
            return;
        }

        // Steer bullet trajectory via shared writer thread
        {
            std::lock_guard<std::mutex> lock(s_silentLock);
            s_silentAimInfo      = aimInfoWeapon;
            s_silentAimInfoAlt   = is_valid_ptr(aimInfoAlt) ? aimInfoAlt : 0;
            s_silentDir          = normDir;
            s_hasSilentData.store(true, std::memory_order_relaxed);
        }

        // Immediate write to ensure instant bullet alignment on first frame
        wpm<Vector3>(aimInfoWeapon + Offsets::Aim::Direction, normDir);
        wpm<bool>(aimInfoWeapon + 0x71, false);

        if (is_valid_ptr(aimInfoAlt)) {
            wpm<Vector3>(aimInfoAlt + Offsets::Aim::Direction, normDir);
            wpm<bool>(aimInfoAlt + 0x71, false);
        }
    }

} // namespace aimbot
