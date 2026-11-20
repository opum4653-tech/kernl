#pragma once
#include "common.hpp"
#include <atomic>
#include <cstdlib>
#include <cmath>

// ==============================================================================================
// FILE: src/func/aim/silent.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 3D Silent Aim & Bullet Trajectory Steering Engine with Humanized Telemetry.
//
// ANTI-BAN ARCHITECTURAL PRINCIPLES (Updated Phase 1 Hardening):
// ----------------------------------------------------------------------------------------------
// 1. ANGULAR DEVIATION CONE GUARD:
//    - Camera forward vector is extracted from the live viewMatrix (m31,m32,m33 = -cam_forward).
//    - Bullet trajectory is validated: dot(CameraForward, BulletDir) must be >= cos(MAX_AIM_ANGLE).
//    - If deviation exceeds the safe weapon cone, write is aborted — bullet fires naturally.
//    - This prevents server-side vector dot-product anomaly detection.
//
// 2. ZERO ViewBlocked OVERWRITING:
//    - ViewBlocked flag (0x71) is NEVER touched. Writing false here allows bullets through walls,
//      which triggers immediate server-side physics raycast validation and permanent ban.
//    - Trajectory is steered only when target is in authentic forward line-of-sight.
//
// 3. ZERO HitGroup FORCING:
//    - HitGroup (0x64) is NEVER overwritten. Forcing HitGroup=1 (Head) while the actual
//      collider raycast registers a body hit causes client/server hitgroup desync — instant ban.
//    - The steered Direction vector causes the physics engine to naturally register the
//      correct hitgroup from the actual raycast intersection.
//
// 4. GAUSSIAN MICRO-JITTER:
//    - Target point receives a tiny pseudorandom Gaussian displacement each shot cycle.
//    - Breaks the perfect mathematical precision signature that flags statistical aimbot detection.
//    - Jitter magnitude scales with distance (tighter at close range, slightly wider at range).
//
// 5. PROBABILISTIC HEADSHOT CADENCE:
//    - Headshot Rate == 100%: ALL shots strictly aim at Head world position. Zero body shots.
//    - Headshot Rate < 100%: Probabilistic roll (rand() % 100 < headshot_rate).
//      * Successful roll: Targets Head position (HeadWorld + head_bias).
//      * Failed roll: Targets Chest/Body (Spine level), producing natural spray telemetry.
//    - Headshot Rate == 0%: ALL shots strictly to Body.
//
// 6. FIRING-CADENCE SYNCHRONIZATION:
//    - Writes occur strictly while weapon is actively discharging (isFiring == true).
//    - Ceases immediately on trigger release — zero persistent footprint.
// ==============================================================================================

namespace trajectory {
namespace silent {

    // ── Jitter Engine ────────────────────────────────────────────────────────────
    // Gaussian micro-drift: Box-Muller transform with stable per-frame seed.
    // Breaks the perfectly flat precision signature that statistical ACs flag.
    static inline float gaussian_jitter(float sigma) {
        // Box-Muller: two uniform random numbers -> one Gaussian sample
        float u1 = (float)(rand() % 10000 + 1) / 10000.0f;
        float u2 = (float)(rand() % 10000 + 1) / 10000.0f;
        float z  = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
        return z * sigma;
    }

    // ── Camera Forward Extraction ─────────────────────────────────────────────────
    // Unity viewMatrix row 3 (m31, m32, m33) is the camera's -Z forward axis in world space.
    // Camera world-space forward = -(m31, m32, m33) normalized.
    static inline Vector3 get_camera_forward(uint64_t localPlayer) {
        uint64_t followCam = rpm<uint64_t>(localPlayer + Offsets::Player::Camera);
        if (!is_valid_ptr(followCam))
            followCam = rpm<uint64_t>(localPlayer + Offsets::Player::Camera_Alt);
        if (!is_valid_ptr(followCam)) return Vector3(0.0f, 0.0f, 1.0f);

        // Probe the same displacement candidates as data.cpp
        const uintptr_t followOffsets[] = { 0x20, 0x30, 0x28 };
        for (uintptr_t fOff : followOffsets) {
            uint64_t cam = rpm<uint64_t>(followCam + fOff);
            if (!is_valid_ptr(cam)) continue;
            uint64_t intPtr = rpm<uint64_t>(cam + 0x10); // Offsets::Camera::IntPtr
            if (!is_valid_ptr(intPtr)) continue;

            // Read 4x4 view matrix at +0x100
            struct RawMatrix { float m[16]; };
            RawMatrix vm = rpm<RawMatrix>(intPtr + 0x100);

            // Row 3 of the matrix (index 8,9,10 in column-major storage) = world-space -Z column
            // Unity GL uses column-major: col0={m00,m10,m20,m30}, col1=..., col2={m02,m12,m22,m32}
            // The third column (col2) = camera right cross up = -forward
            // In Unity row-major notation stored as: m31=vm[2], m32=vm[6], m33=vm[10]
            float fx = -vm.m[2];
            float fy = -vm.m[6];
            float fz = -vm.m[10];

            float lenSq = fx * fx + fy * fy + fz * fz;
            if (lenSq < 1e-6f || !std::isfinite(lenSq)) continue;
            float invLen = 1.0f / sqrtf(lenSq);
            return Vector3(fx * invLen, fy * invLen, fz * invLen);
        }
        return Vector3(0.0f, 0.0f, 1.0f);
    }

    // ── Maximum safe angular deviation constant ───────────────────────────────────
    // 12.0° — conservative threshold matching weapon cone spread from physical game calibration.
    // Exceeding this means the trajectory deviates too far from crosshair to look authentic.
    static constexpr float MAX_AIM_ANGLE_DEG = 12.0f;
    static constexpr float MAX_AIM_COS       = 0.9781f; // cosf(12.0 * PI / 180.0)

    // ── Stub lifecycle functions ──────────────────────────────────────────────────
    inline void clear_silent_data() {
        // Natural release: writing ceases when firing stops.
        // No persistent state to clear.
    }

    inline void start_silent_writer() {
        // Zero-op: redundant background writer thread eliminated.
        // Trajectory writes are synchronous with the natural game frame tick.
    }

    inline void stop_silent_writer() {
        clear_silent_data();
    }

    // ── Core trajectory steering ──────────────────────────────────────────────────
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target,
                                 bool lockToHead, bool isFiring) {
        (void)lockToHead;

        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) return;

        // Anti-Ban Invariant: write trajectory ONLY during active weapon discharge.
        if (!isFiring) return;

        // Discard dead targets.
        if (target.curHP <= 0) return;

        // Verify at least one trajectory feature is enabled.
        if (!cfg::aim::silent.load(std::memory_order_relaxed) &&
            !cfg::aim::auto_headshot.load(std::memory_order_relaxed) &&
            !cfg::aim::scope_aim.load(std::memory_order_relaxed)) {
            return;
        }

        // ── 1. Resolve Weapon AimInfo buffer (Multi-Candidate: 0xE58 / 0xE50) ──────
        uint64_t hitObjAddress = 0;
        const uintptr_t aimCandidates[] = { Offsets::Player::AimInfo,
                                             Offsets::Player::AimInfo_Alt };
        for (uintptr_t off : aimCandidates) {
            uint64_t cand = rpm<uint64_t>(localPlayer + off);
            if (is_valid_ptr(cand)) { hitObjAddress = cand; break; }
        }
        if (!is_valid_ptr(hitObjAddress)) return;

        // ── 2. Read muzzle start position (0x4C) ─────────────────────────────────
        Vector3 ammoBase = rpm<Vector3>(hitObjAddress + Offsets::Aim::StartPosition);
        if (ammoBase == Vector3::Zero() || !std::isfinite(ammoBase.x)) {
            // Fallback: local player head node as muzzle origin.
            uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone::Head);
            if (localHead) ammoBase = GetNodePosition(localHead);
        }
        if (ammoBase == Vector3::Zero() || !std::isfinite(ammoBase.x)) return;

        // ── 3. Probabilistic headshot rate decision ───────────────────────────────
        int hsRate = cfg::aim::headshot_rate.load(std::memory_order_relaxed);
        if (hsRate < 0)   hsRate = 0;
        if (hsRate > 100) hsRate = 100;

        bool targetIsHead;
        if (hsRate >= 100) {
            targetIsHead = true;
        } else if (hsRate <= 0) {
            targetIsHead = false;
        } else {
            targetIsHead = ((rand() % 100) < hsRate);
        }

        float headBias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (headBias < -0.30f || headBias > 0.30f) headBias = 0.00f;

        Vector3 targetWorld = targetIsHead
            ? target.getTargetWorld(0, headBias)   // Head / skull center
            : target.getTargetWorld(1, 0.0f);       // Chest / body

        if (targetWorld == Vector3::Zero() || !std::isfinite(targetWorld.x)) return;

        // ── 4. Gaussian micro-jitter ──────────────────────────────────────────────
        // Sigma scales with distance: 0.012m at 10m, 0.045m at 100m — sub-hitbox precision.
        // Breaks identical mathematical signature across shots without meaningful accuracy loss.
        float dist3D = sqrtf((targetWorld.x - ammoBase.x) * (targetWorld.x - ammoBase.x) +
                             (targetWorld.y - ammoBase.y) * (targetWorld.y - ammoBase.y) +
                             (targetWorld.z - ammoBase.z) * (targetWorld.z - ammoBase.z));
        float jitterSigma = 0.012f + (dist3D * 0.00033f); // ~0.012m at 10m, ~0.045m at 100m
        targetWorld.x += gaussian_jitter(jitterSigma);
        targetWorld.y += gaussian_jitter(jitterSigma * 0.5f); // Vertical jitter tighter (gravity axis)
        targetWorld.z += gaussian_jitter(jitterSigma);

        // ── 5. Compute and normalize 3D trajectory unit vector ───────────────────
        Vector3 dir = targetWorld - ammoBase;
        if (dir.sqrMagnitude() <= 1e-4f) return;

        Vector3 normDir = SafeNormalize(dir);
        if (!std::isfinite(normDir.x) || normDir == Vector3::Zero()) return;

        // ── 6. ANGULAR DEVIATION CONE GUARD ──────────────────────────────────────
        // Server-side: validates dot(CameraForward, BulletDir). Values below threshold
        // indicate the bullet was fired at an impossible angle — instant ban flag.
        // We abort the write and let the game fire naturally if deviation is too large.
        Vector3 camFwd = get_camera_forward(localPlayer);
        float dotVal = normDir.x * camFwd.x + normDir.y * camFwd.y + normDir.z * camFwd.z;
        if (dotVal < MAX_AIM_COS) {
            // Target is outside safe firing cone. Bullet fires naturally.
            // This protects against teleport-aimbot telemetry signatures.
            return;
        }

        // ── 7. Single-pass trajectory write ──────────────────────────────────────
        // Write ONLY the normalized direction vector to weapon AimInfo::RayDir (0x40).
        // - ViewBlocked (0x71): NEVER touched. Modifying it allows wall shots -> server ban.
        // - HitGroup (0x64):    NEVER forced. Physics engine registers correct hitgroup
        //                       from the actual raycast impact point. Forcing it causes
        //                       client/server hitgroup desync -> instant telemetry flag.
        wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, normDir);
    }

} // namespace silent

    // Aliases in trajectory namespace
    using silent::clear_silent_data;
    using silent::start_silent_writer;
    using silent::stop_silent_writer;
    using silent::apply_trajectory;

} // namespace trajectory
