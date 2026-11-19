#pragma once
#include "common.hpp"
#include "../../other/logger.hpp"
#include <sched.h>
#include <unistd.h>

// ==============================================================================================
// FILE: src/func/aim/silent.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 3D Silent Aim & Bullet Trajectory Steering Engine (Matching pewpew 1:1 Architecture).
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
//    - Local player entity pointer: data::localPlayerAddr (from src/game/data.hpp).
//    - Target enemy data: const PlayerData& target (from src/func/aim/target.hpp).
//    - Configuration switches: cfg::aim::silent, cfg::aim::auto_headshot, cfg::aim::head_bias.
//
// 3. ZERO-FOOTPRINT MEMORY SAFETY MODEL:
//    - NEVER touches or writes to enemy entity memory (zero targetAddr writes).
//    - Modifies ONLY the local weapon raycast aim buffer (hitObj + 0x40 = Offsets::Aim::Direction).
//    - When trigger is released (isFiring == false), memory flooding halts instantly and clears.
//
// 4. MATHEMATICAL VECTOR STEERING:
//    - ammoBase    = Weapon muzzle / shot origin (hitObjAddress + 0x4C).
//    - targetWorld = Enemy skull center (target.headWorld + SkullElevation + head_bias).
//    - dir         = targetWorld - ammoBase (Raw unnormalized delta direction vector).
//    - Memory WPM  = wpm<Vector3>(hitObjAddress + 0x40, dir).
// ==============================================================================================

namespace aimbot {

    // Atomic coordination primitives for the asynchronous memory writer thread
    inline std::atomic<bool>     s_writerRunning{false};
    inline std::atomic<bool>     s_hasTarget{false};
    inline std::atomic<bool>     s_hasData{false};
    inline std::atomic<uint64_t> s_hitObj{0};
    inline std::atomic<float>    s_dirX{0.f};
    inline std::atomic<float>    s_dirY{0.f};
    inline std::atomic<float>    s_dirZ{0.f};
    inline std::thread           s_writerThread;

    // Resets atomic direction and target state flags to neutral
    inline void clear_silent_data() {
        s_hasData.store(false, std::memory_order_relaxed);
        s_hasTarget.store(false, std::memory_order_relaxed);
        s_hitObj.store(0, std::memory_order_relaxed);
        s_dirX.store(0.f, std::memory_order_relaxed);
        s_dirY.store(0.f, std::memory_order_relaxed);
        s_dirZ.store(0.f, std::memory_order_relaxed);
    }

    // High-speed writer thread loop — floods hitObj + 0x40 while shooting for zero-latency bullet steering
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            // If weapon is not firing or hit object address is missing, sleep to conserve CPU
            if (!s_hasData.load(std::memory_order_relaxed) || s_hitObj.load(std::memory_order_relaxed) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint64_t hitObj = s_hitObj.load(std::memory_order_relaxed);
            if (is_valid_ptr(hitObj)) {
                Vector3 dir(
                    s_dirX.load(std::memory_order_relaxed),
                    s_dirY.load(std::memory_order_relaxed),
                    s_dirZ.load(std::memory_order_relaxed)
                );
                // Flood weapon raycast direction buffer (Offsets::Aim::Direction = 0x40)
                wpm<Vector3>(hitObj + Offsets::Aim::Direction, dir);
            }
            sched_yield(); // Yield CPU time slice to prevent thread starvation
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

        // 3. Resolve Bullet Origin / StartPosition (hitObj + 0x4C in pewpew)
        Vector3 ammoBase = rpm<Vector3>(hitObjAddress + Offsets::Aim::StartPosition);
        if (ammoBase == Vector3::Zero() || !std::isfinite(ammoBase.x)) {
            // Fallback: use local player head node as trajectory start point
            uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone_Head);
            if (localHead) ammoBase = GetNodePosition(localHead);
        }

        // 4. Direction Calculation & Memory Flooding (pewpew lines 148-158)
        if (ammoBase != Vector3::Zero() && std::isfinite(ammoBase.x)) {
            Vector3 dir;
            dir.x = targetWorld.x - ammoBase.x;
            dir.y = targetWorld.y - ammoBase.y;
            dir.z = targetWorld.z - ammoBase.z;

            if (dir.sqrMagnitude() > 0.001f) {
                // Publish coordinates to atomic registers for writer_loop()
                s_dirX.store(dir.x, std::memory_order_relaxed);
                s_dirY.store(dir.y, std::memory_order_relaxed);
                s_dirZ.store(dir.z, std::memory_order_relaxed);
                s_hitObj.store(hitObjAddress, std::memory_order_relaxed);
                s_hasData.store(isFiring, std::memory_order_release);
                s_hasTarget.store(true, std::memory_order_release);

                // Direct immediate write for instant first-shot hit registration
                if (isFiring) {
                    wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, dir);
                }
            } else {
                clear_silent_data();
            }
        } else {
            clear_silent_data();
        }
    }

} // namespace aimbot
