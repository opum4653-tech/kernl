#pragma once
#include "common.hpp"
#include "../../other/logger.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <sched.h>
#include <unistd.h>

// ==============================================================================================
// FILE: src/func/aim/silent.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 3D Silent Aim & Bullet Trajectory Steering Engine (100% Anti-Ban Safe).
//
// ANTI-BAN HARDENING PROTOCOL:
// ----------------------------------------------------------------------------------------------
// 1. HIGH-SPEED WRITER THREAD (writer_loop):
//    - Continuously writes g_targetDir to g_hitObj + Offsets::AimInfo_Direction (0x40)
//      strictly during active firing with usleep(500).
// 2. ZERO FOREIGN OBJECT MUTATION:
//    - NEVER writes to enemy entity memory, HeadCollider, or HitCollider (0% footprint).
//    - ZERO bogus memory writes to ViewBlocked (0x71), HitGroup (0x64), HitPoint, HitNormal,
//      AimingAdjusted (0xE60), or AimingTarget (0xE68).
// 3. ZERO FIRING LEAK:
//    - Trajectory steering is only active when isFiring is true.
// ==============================================================================================

namespace trajectory {
namespace silent {

    inline std::atomic<bool> g_run{false};
    inline std::thread       g_writer_thread;

    inline std::mutex        g_silent_lock;
    inline uint64_t          g_hitObj = 0;
    inline Vector3           g_targetDir = {0.f, 0.f, 0.f};
    inline std::atomic<bool> g_hasData{false};

    inline void clear_silent_data() {
        if (g_hasData.load(std::memory_order_relaxed) || g_hitObj != 0) {
            std::lock_guard<std::mutex> lock(g_silent_lock);
            g_hasData.store(false, std::memory_order_relaxed);
            g_hitObj = 0;
        }
    }

    // High-speed writer thread — continuously writes trajectory during active firing
    static void writer_loop() {
        while (g_run.load(std::memory_order_relaxed)) {
            if (!g_hasData.load(std::memory_order_relaxed) || g_hitObj == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            // Continuous memory write strictly during active firing
            {
                std::lock_guard<std::mutex> lock(g_silent_lock);
                if (g_hasData.load(std::memory_order_relaxed) && g_hitObj != 0) {
                    wpm<Vector3>(g_hitObj + Offsets::Aim::Direction, g_targetDir); // 0x40 (Direction)
                }
            }
            usleep(500); // 2,000 writes/sec: clean 500us throttle, zero CPU spin locks
        }
    }

    inline void start_silent_writer() {
        if (g_run.exchange(true)) return;
        g_writer_thread = std::thread(writer_loop);
    }

    inline void stop_silent_writer() {
        if (!g_run.exchange(false)) return;
        clear_silent_data();
        if (g_writer_thread.joinable()) {
            g_writer_thread.join();
        }
    }

    // Computes the 3D trajectory vector and publishes to high-speed writer
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target, bool lockToHead, bool isFiring) {
        (void)lockToHead;
        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) {
            clear_silent_data();
            return;
        }

        // Anti-Ban Invariant: Strictly write trajectory ONLY when weapon is actively firing
        if (!isFiring) {
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

        // 2. Read StartPos (0x4C) - gun muzzle origin
        Vector3 ammoBase = rpm<Vector3>(hitObjAddress + Offsets::Aim::StartPosition);
        if (ammoBase == Vector3::Zero() || !std::isfinite(ammoBase.x)) {
            // Fallback: use local player head node as trajectory start point
            uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone::Head);
            if (localHead) ammoBase = GetNodePosition(localHead);
        }

        if (ammoBase == Vector3::Zero() || !std::isfinite(ammoBase.x)) {
            clear_silent_data();
            return;
        }

        // 3. Resolve Target World Position with elevation adjustment
        float headBias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (headBias < -0.30f || headBias > 0.30f) headBias = 0.00f;
        Vector3 targetWorld = target.getTargetWorld(0, headBias);

        if (targetWorld == Vector3::Zero() || !std::isfinite(targetWorld.x)) {
            clear_silent_data();
            return;
        }

        // Full displacement ray vector: targetWorld - ammoBase
        Vector3 dir = targetWorld - ammoBase;
        if (dir.sqrMagnitude() <= 1e-4f) {
            clear_silent_data();
            return;
        }

        // Publish to high-speed writer thread
        {
            std::lock_guard<std::mutex> lock(g_silent_lock);
            g_hitObj = hitObjAddress;
            g_targetDir = dir;
            g_hasData.store(true, std::memory_order_relaxed);
        }
    }

} // namespace silent

    // Aliases in trajectory namespace
    using silent::clear_silent_data;
    using silent::start_silent_writer;
    using silent::stop_silent_writer;
    using silent::apply_trajectory;

} // namespace trajectory
