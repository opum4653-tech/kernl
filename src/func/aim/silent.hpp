#pragma once
#include "common.hpp"
#include <atomic>

// ==============================================================================================
// FILE: src/func/aim/silent.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 3D Silent Aim & Bullet Trajectory Steering Engine (Natural Game Cadence).
//
// ANTI-BAN HARDENING PROTOCOL:
// ----------------------------------------------------------------------------------------------
// 1. NATURAL GAME CADENCE:
//    - Operates synchronously with the natural game frame cadence.
//    - Redundant background writer thread and artificial tight-loop sleep (usleep(500)) eliminated.
// 2. STRICT NORMALIZATION & ZERO FORCED WRITES:
//    - Writes strictly normalized unit direction vector (||v|| == 1.0f) to AimInfo + 0x40.
//    - Strictly active ONLY while weapon is actively discharging (isFiring == true) on alive targets.
// 3. ZERO PERSISTENT FOOTPRINT:
//    - Once trigger is released or target is dead, writing ceases immediately.
//    - Game's native weapon controller takes over memory naturally on next frame tick.
// ==============================================================================================

namespace trajectory {
namespace silent {

    inline void clear_silent_data() {
        // Natural release: ceases external writes, allowing game's native
        // weapon controller to take over memory naturally on the next frame tick.
    }

    inline void start_silent_writer() {
        // Zero-op: redundant background writer thread eliminated for pure smooth execution
    }

    inline void stop_silent_writer() {
        clear_silent_data();
    }

    // Smooth trajectory steering: synchronized directly with game frame cadence
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target, bool lockToHead, bool isFiring) {
        (void)lockToHead;
        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) {
            return;
        }

        // Anti-Ban Invariant: Strictly write trajectory ONLY when weapon is actively firing
        if (!isFiring) {
            return;
        }

        // Target alive check
        if (target.curHP <= 0) {
            return;
        }

        // Verify at least one trajectory-altering mod feature is enabled
        if (!cfg::aim::silent.load(std::memory_order_relaxed) &&
            !cfg::aim::auto_headshot.load(std::memory_order_relaxed) &&
            !cfg::aim::scope_aim.load(std::memory_order_relaxed)) {
            return;
        }

        // 1. Resolve Weapon AimInfo buffer pointer (Multi-Candidate: 0xE58, 0xE50)
        uint64_t hitObjAddress = 0;
        const uintptr_t aimCandidates[] = { Offsets::Player::AimInfo, Offsets::Player_AimInfo_Alt };
        for (uintptr_t off : aimCandidates) {
            uint64_t cand = rpm<uint64_t>(localPlayer + off);
            if (is_valid_ptr(cand)) {
                hitObjAddress = cand;
                break;
            }
        }
        if (!is_valid_ptr(hitObjAddress)) {
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
            return;
        }

        // 3. Resolve Target World Position with elevation adjustment
        float headBias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (headBias < -0.30f || headBias > 0.30f) headBias = 0.00f;
        Vector3 targetWorld = target.getTargetWorld(0, headBias);

        if (targetWorld == Vector3::Zero() || !std::isfinite(targetWorld.x)) {
            return;
        }

        // 4. Compute and strictly normalize 3D trajectory unit vector
        Vector3 dir = targetWorld - ammoBase;
        if (dir.sqrMagnitude() <= 1e-4f) {
            return;
        }

        Vector3 normDir = SafeNormalize(dir);
        if (!std::isfinite(normDir.x) || normDir == Vector3::Zero()) {
            return;
        }

        // Smooth single-pass write on natural game frame tick:
        // No redundant tight loops, no multi-thread mutex contention.
        wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, normDir); // 0x40 (Direction)
        if (cfg::aim::auto_headshot.load(std::memory_order_relaxed)) {
            wpm<int32_t>(hitObjAddress + Offsets::Aim::HitGroup, Offsets::Aim::HitGroup_Head); // 0x64 (Headshot HitGroup)
        }
    }

} // namespace silent

    // Aliases in trajectory namespace
    using silent::clear_silent_data;
    using silent::start_silent_writer;
    using silent::stop_silent_writer;
    using silent::apply_trajectory;

} // namespace trajectory
