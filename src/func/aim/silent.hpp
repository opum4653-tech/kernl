#pragma once
#include "common.hpp"
#include "../../other/logger.hpp"
#include <sched.h>
#include <unistd.h>

// ==============================================================================================
// FILE: src/func/aim/silent.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 3D Silent Aim & Bullet Trajectory Steering Engine (100% Red Auto Headshot).
//
// CROSS-FILE CALL GRAPH & DATA FLOW:
// ----------------------------------------------------------------------------------------------
// 1. CALLERS & LIFECYCLE:
//    - src/func/aim/aim.hpp -> aimbot::update():
//      * Calls apply_trajectory() whenever Silent Aim, Auto Headshot, or Scope Aim is active.
//    - src/func/aim/aim.hpp -> aimbot::thread_start() / thread_stop():
//      * Starts/stops the high-frequency writer thread (start_silent_writer() / stop_silent_writer()).
//
// 2. INPUT DATA SOURCES:
//    - Local player entity pointer: uint64_t localPlayer (from data::localPlayerAddr).
//    - Target enemy data: const PlayerData& target (from src/func/aim/target.hpp).
//    - Configuration switches: cfg::aim::silent, cfg::aim::auto_headshot, cfg::aim::head_bias.
//
// 3. ZERO-FOOTPRINT MEMORY SAFETY MODEL:
//    - ZERO writes to enemy entity memory (zero targetAddr writes, 100% anti-ban safe).
//    - Reads enemy HeadCollider (0x738 / 0x740 / 0x80) strictly read-only.
//    - Modifies ONLY the local player weapon raycast AimInfo buffer (localPlayer + 0xE58) and
//      localPlayer aiming authorization flags (0xE60 / 0xE68).
//    - When trigger is released (isFiring == false), restores AimingAdjusted to false and clears.
//
// 4. 100% RED HEADSHOT MATHEMATICAL INJECTION (OB55 / 1.132.1):
//    - AimInfo + 0x40 (Direction)           = 3D trajectory direction vector (targetHead - ammoBase)
//    - AimInfo + 0x28 (HitPoint)            = 3D world coordinates of target head
//    - AimInfo + 0x34 (HitNormal)           = Normalized surface normal (-dir.normalized())
//    - AimInfo + 0x5C (Distance)            = Euclidean distance to target head
//    - AimInfo + 0x64 (HitGroup)            = 1 (HitGroup_Head -> triggers red headshot multiplier)
//    - AimInfo + 0x71 (ViewBlocked)         = false (bypasses obstruction drop)
//    - AimInfo + 0x20 (HitCollider)         = targetHeadCollider (validates head hitbox collision)
//    - AimInfo + 0x18 (HitObject)           = target.addr (binds hit entity)
//    - LocalPlayer + 0xE60 (AimingAdjusted) = true (forces engine to apply weapon AimInfo)
//    - LocalPlayer + 0xE68 (AimingTarget)   = target.addr (binds targeted combatant)
// ==============================================================================================

namespace aimbot {

    // Atomic coordination primitives for the asynchronous memory writer thread
    inline std::atomic<bool>     s_writerRunning{false};
    inline std::atomic<bool>     s_hasTarget{false};
    inline std::atomic<bool>     s_hasData{false};
    inline std::atomic<uint64_t> s_localPlayer{0};
    inline std::atomic<uint64_t> s_hitObj{0};
    inline std::atomic<uint64_t> s_targetAddr{0};
    inline std::atomic<uint64_t> s_targetCollider{0};
    inline std::atomic<float>    s_dirX{0.f};
    inline std::atomic<float>    s_dirY{0.f};
    inline std::atomic<float>    s_dirZ{0.f};
    inline std::atomic<float>    s_pointX{0.f};
    inline std::atomic<float>    s_pointY{0.f};
    inline std::atomic<float>    s_pointZ{0.f};
    inline std::atomic<float>    s_normX{0.f};
    inline std::atomic<float>    s_normY{0.f};
    inline std::atomic<float>    s_normZ{0.f};
    inline std::atomic<float>    s_dist{0.f};
    inline std::thread           s_writerThread;

    // Resets atomic direction and target state flags to neutral, restoring local player memory
    inline void clear_silent_data() {
        uint64_t lp = s_localPlayer.load(std::memory_order_relaxed);
        if (is_valid_ptr(lp)) {
            // Restore weapon aim adjustment flag and target to neutral state
            wpm<bool>(lp + Offsets::Player::AimingAdjusted, false);
            wpm<uint64_t>(lp + Offsets::Player::AimingTarget, 0);
        }

        s_hasData.store(false, std::memory_order_relaxed);
        s_hasTarget.store(false, std::memory_order_relaxed);
        s_hitObj.store(0, std::memory_order_relaxed);
        s_localPlayer.store(0, std::memory_order_relaxed);
        s_targetAddr.store(0, std::memory_order_relaxed);
        s_targetCollider.store(0, std::memory_order_relaxed);
        s_dirX.store(0.f, std::memory_order_relaxed);
        s_dirY.store(0.f, std::memory_order_relaxed);
        s_dirZ.store(0.f, std::memory_order_relaxed);
        s_pointX.store(0.f, std::memory_order_relaxed);
        s_pointY.store(0.f, std::memory_order_relaxed);
        s_pointZ.store(0.f, std::memory_order_relaxed);
        s_normX.store(0.f, std::memory_order_relaxed);
        s_normY.store(0.f, std::memory_order_relaxed);
        s_normZ.store(0.f, std::memory_order_relaxed);
        s_dist.store(0.f, std::memory_order_relaxed);
    }

    // High-speed writer thread loop — floods AimInfo fields while shooting for 100% Red Headshots
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            // If weapon is not firing or hit object address is missing, sleep to conserve CPU
            if (!s_hasData.load(std::memory_order_relaxed) || s_hitObj.load(std::memory_order_relaxed) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint64_t hitObj     = s_hitObj.load(std::memory_order_relaxed);
            uint64_t lp         = s_localPlayer.load(std::memory_order_relaxed);
            uint64_t targetAddr = s_targetAddr.load(std::memory_order_relaxed);
            uint64_t collider   = s_targetCollider.load(std::memory_order_relaxed);

            if (is_valid_ptr(hitObj)) {
                Vector3 dir(
                    s_dirX.load(std::memory_order_relaxed),
                    s_dirY.load(std::memory_order_relaxed),
                    s_dirZ.load(std::memory_order_relaxed)
                );
                Vector3 point(
                    s_pointX.load(std::memory_order_relaxed),
                    s_pointY.load(std::memory_order_relaxed),
                    s_pointZ.load(std::memory_order_relaxed)
                );
                Vector3 norm(
                    s_normX.load(std::memory_order_relaxed),
                    s_normY.load(std::memory_order_relaxed),
                    s_normZ.load(std::memory_order_relaxed)
                );
                float dist = s_dist.load(std::memory_order_relaxed);

                // 1. Flood weapon raycast direction buffer (Offsets::Aim::Direction = 0x40)
                wpm<Vector3>(hitObj + Offsets::Aim::Direction, dir);

                // 2. Flood HitPoint & HitNormal
                wpm<Vector3>(hitObj + Offsets::Aim::HitPoint, point);
                wpm<Vector3>(hitObj + Offsets::Aim::HitNormal, norm);

                // 3. Flood HitGroup = 1 (Offsets::Aim::HitGroup_Head = 1 for 100% Red Headshots)
                wpm<int32_t>(hitObj + Offsets::Aim::HitGroup, Offsets::Aim::HitGroup_Head);

                // 4. Flood Distance & ViewBlocked
                wpm<float>(hitObj + Offsets::Aim::Distance, dist);
                wpm<bool>(hitObj + Offsets::Aim::ViewBlocked, false);

                // 5. Flood target entity and head collider pointers
                if (is_valid_ptr(targetAddr)) {
                    wpm<uint64_t>(hitObj + Offsets::Aim::HitObject, targetAddr);
                }
                if (is_valid_ptr(collider)) {
                    wpm<uint64_t>(hitObj + Offsets::Aim::HitCollider, collider);
                }

                // 6. Flood LocalPlayer aiming authorization registers
                if (is_valid_ptr(lp)) {
                    wpm<bool>(lp + Offsets::Player::AimingAdjusted, true);
                    if (is_valid_ptr(targetAddr)) {
                        wpm<uint64_t>(lp + Offsets::Player::AimingTarget, targetAddr);
                    }
                }
            }

            // High frequency polling (1ms = 1000Hz) ensures all frame bullets hit with zero thermal throttling
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Spawns the dedicated high-speed trajectory writer thread
    inline void start_silent_writer() {
        if (s_writerRunning.exchange(true)) return;
        if (s_writerThread.joinable()) {
            s_writerThread.join();
        }
        s_writerThread = std::thread(writer_loop);
    }

    // Halts and joins the high-speed trajectory writer thread
    inline void stop_silent_writer() {
        if (!s_writerRunning.exchange(false)) return;
        clear_silent_data();
        if (s_writerThread.joinable()) {
            s_writerThread.join();
        }
    }

    // Computes the 3D trajectory vector and registers it with the memory flooding system
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target, bool lockToHead, bool isFiring) {
        (void)lockToHead;
        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) {
            clear_silent_data();
            return;
        }

        // Verify at least one trajectory-altering mod feature is enabled
        if (!cfg::aim::silent.load(std::memory_order_relaxed) &&
            !cfg::aim::auto_headshot.load(std::memory_order_relaxed) &&
            !cfg::aim::scope_aim.load(std::memory_order_relaxed)) {
            clear_silent_data();
            return;
        }

        // If player is not firing, restore aiming state and sleep
        if (!isFiring) {
            clear_silent_data();
            return;
        }

        // 1. Resolve Weapon AimInfo buffer pointer (Offsets::Player::AimInfo = 0xE58)
        uint64_t hitObjAddress = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo);
        if (!is_valid_ptr(hitObjAddress)) {
            hitObjAddress = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo_Alt);
        }
        if (!is_valid_ptr(hitObjAddress)) {
            clear_silent_data();
            return;
        }

        // 2. Resolve Target World Position with elevation adjustment
        Vector3 targetWorld = target.headWorld;
        if (targetWorld == Vector3::Zero() || !std::isfinite(targetWorld.x)) {
            clear_silent_data();
            return;
        }

        // Apply fine-tuned vertical elevation trim from UI menu slider
        float bias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (bias < -0.30f || bias > 0.30f) bias = 0.00f;
        targetWorld.y += (Offsets::Bone::SkullElevation + bias);

        // 3. Resolve Bullet Origin / StartPosition (hitObj + 0x4C)
        Vector3 ammoBase = rpm<Vector3>(hitObjAddress + Offsets::Aim::StartPosition);
        if (ammoBase == Vector3::Zero() || !std::isfinite(ammoBase.x)) {
            // Fallback: use local player head node as trajectory start point
            uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone_Head);
            if (localHead) ammoBase = GetNodePosition(localHead);
        }

        // 4. Direction Calculation & Parameter Synthesis
        if (ammoBase != Vector3::Zero() && std::isfinite(ammoBase.x)) {
            Vector3 dir;
            dir.x = targetWorld.x - ammoBase.x;
            dir.y = targetWorld.y - ammoBase.y;
            dir.z = targetWorld.z - ammoBase.z;

            float dist = dir.magnitude();
            if (dist > 0.05f) {
                Vector3 norm = dir * (-1.0f / dist);

                // Resolve Target Enemy Head Collider (Zero-Touch Read-Only)
                uint64_t targetCollider = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider_Alt); // 0x738
                if (!is_valid_ptr(targetCollider)) {
                    targetCollider = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider); // 0x740
                }
                if (!is_valid_ptr(targetCollider)) {
                    targetCollider = rpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider); // 0x80
                }

                // Publish coordinates to atomic registers for writer_loop()
                s_localPlayer.store(localPlayer, std::memory_order_relaxed);
                s_hitObj.store(hitObjAddress, std::memory_order_relaxed);
                s_targetAddr.store(target.addr, std::memory_order_relaxed);
                s_targetCollider.store(targetCollider, std::memory_order_relaxed);
                s_dirX.store(dir.x, std::memory_order_relaxed);
                s_dirY.store(dir.y, std::memory_order_relaxed);
                s_dirZ.store(dir.z, std::memory_order_relaxed);
                s_pointX.store(targetWorld.x, std::memory_order_relaxed);
                s_pointY.store(targetWorld.y, std::memory_order_relaxed);
                s_pointZ.store(targetWorld.z, std::memory_order_relaxed);
                s_normX.store(norm.x, std::memory_order_relaxed);
                s_normY.store(norm.y, std::memory_order_relaxed);
                s_normZ.store(norm.z, std::memory_order_relaxed);
                s_dist.store(dist, std::memory_order_relaxed);
                s_hasTarget.store(true, std::memory_order_release);
                s_hasData.store(true, std::memory_order_release);

                // Direct immediate write for instant first-shot hit registration
                wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, dir);
                wpm<Vector3>(hitObjAddress + Offsets::Aim::HitPoint, targetWorld);
                wpm<Vector3>(hitObjAddress + Offsets::Aim::HitNormal, norm);
                wpm<int32_t>(hitObjAddress + Offsets::Aim::HitGroup, Offsets::Aim::HitGroup_Head);
                wpm<float>(hitObjAddress + Offsets::Aim::Distance, dist);
                wpm<bool>(hitObjAddress + Offsets::Aim::ViewBlocked, false);
                wpm<int16_t>(hitObjAddress + Offsets::Aim::SpecialHitType, 0);
                wpm<Vector3>(hitObjAddress + Offsets::Aim::OrigStartPosition, ammoBase);
                if (is_valid_ptr(targetCollider)) {
                    wpm<uint64_t>(hitObjAddress + Offsets::Aim::HitCollider, targetCollider);
                }
                wpm<uint64_t>(hitObjAddress + Offsets::Aim::HitObject, target.addr);

                // Authorize local player weapon aim state
                wpm<bool>(localPlayer + Offsets::Player::AimingAdjusted, true);
                wpm<uint64_t>(localPlayer + Offsets::Player::AimingTarget, target.addr);
            } else {
                clear_silent_data();
            }
        } else {
            clear_silent_data();
        }
    }

} // namespace aimbot
