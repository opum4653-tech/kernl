#pragma once
#include "common.hpp"
#include <atomic>
#include <cstdlib>

// ==============================================================================================
// FILE: src/func/aim/silent.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 3D Silent Aim & Bullet Trajectory Steering Engine with Humanized Headshot Rate.
//
// ANTI-BAN & HUMANIZED TELEMETRY PROTOCOL:
// ----------------------------------------------------------------------------------------------
// 1. PROBABILISTIC HITGROUP & BONE DECISION:
//    - Headshot Rate == 100%: 100% of bullets strictly target Head (HitGroup = 1), zero to body.
//    - Headshot Rate < 100%: Probabilistic roll (rand() % 100 < headshot_rate).
//      * Successful roll: Targets Head (HitGroup = 1, HeadWorld + head_bias).
//      * Failed roll: Targets Chest / Body (HitGroup = 2, Spine level), producing natural spray telemetry.
//    - Headshot Rate == 0%: All bullets target Body (HitGroup = 2).
// 2. NATURAL GAME CADENCE:
//    - Operates synchronously with natural game frame cadence.
//    - Redundant background writer thread and artificial tight loops eliminated.
// 3. ZERO PERSISTENT FOOTPRINT:
//    - Active strictly while weapon is actively discharging (isFiring == true) on alive targets.
//    - Once trigger is released, writing ceases immediately and naturally.
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

        // 3. Humanized Headshot Rate & Target Selection Decision
        int hsRate = cfg::aim::headshot_rate.load(std::memory_order_relaxed);
        if (hsRate < 0) hsRate = 0;
        if (hsRate > 100) hsRate = 100;

        bool targetIsHead = true;
        if (hsRate >= 100) {
            // 100% Rate: ALL shots strictly to head, zero to body
            targetIsHead = true;
        } else if (hsRate <= 0) {
            // 0% Rate: ALL shots strictly to body
            targetIsHead = false;
        } else {
            // Probabilistic evaluation: roll between 0 and 99
            int roll = rand() % 100;
            targetIsHead = (roll < hsRate);
        }

        float headBias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (headBias < -0.30f || headBias > 0.30f) headBias = 0.00f;

        Vector3 targetWorld = Vector3::Zero();
        int32_t targetHitGroup = Offsets::Aim::HitGroup_Body; // 2 (Body)

        if (targetIsHead) {
            // Target Head (Skull center + user vertical elevation trim)
            targetWorld = target.getTargetWorld(0, headBias);
            targetHitGroup = Offsets::Aim::HitGroup_Head; // 1 (Head)
        } else {
            // Target Body (Chest / Spine center)
            targetWorld = target.getTargetWorld(1, 0.0f);
            targetHitGroup = Offsets::Aim::HitGroup_Body; // 2 (Body)
        }

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
        // Direct raycast direction vector steering (0x40)
        wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, normDir);

        // Inject HitGroup: 1 for Headshot, 2 for Body
        wpm<int32_t>(hitObjAddress + Offsets::Aim::HitGroup, targetHitGroup);

        // Reset ViewBlocked flag (0x71) to ensure valid physics raycast hit registration
        wpm<bool>(hitObjAddress + Offsets::Aim::ViewBlocked, false);
    }

} // namespace silent

    // Aliases in trajectory namespace
    using silent::clear_silent_data;
    using silent::start_silent_writer;
    using silent::stop_silent_writer;
    using silent::apply_trajectory;

} // namespace trajectory
