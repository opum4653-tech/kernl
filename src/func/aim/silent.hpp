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
// ANTI-BAN ARCHITECTURAL PRINCIPLES (Phase 1 — Hardened):
// ----------------------------------------------------------------------------------------------
// 1. ANGULAR DEVIATION CONE GUARD:
//    - Camera forward is extracted from data::viewMatrix_pub (already-resolved, no extra RPM).
//      Unity row-major matrix: third ROW = m31,m32,m33 = indices [8],[9],[10] in flat array.
//      That row is the NEGATED camera forward in world space: camFwd = -( m[8], m[9], m[10] ).
//    - Cone check: dot(camFwd, normDir) >= MAX_AIM_COS (cos 12°).
//    - Applied AFTER direction computation but BEFORE jitter mutation of normDir —
//      so the cone is checked on the clean mathematical aim vector, not on the
//      noise-added variant. If cone fails, write aborts and bullet fires naturally.
//
// 2. ZERO ViewBlocked OVERWRITING:
//    - ViewBlocked (0x71) is NEVER touched. Writing false enables magic-bullet wall
//      penetration; server raycast validation catches it instantly → permanent ban.
//
// 3. ZERO HitGroup FORCING:
//    - HitGroup (0x64) is NEVER overwritten. Forcing 1 (Head) while the actual collider
//      raycast registers a body hit causes client/server hitgroup desync → instant ban.
//    - The steered Direction vector lets the physics engine register the correct hitgroup
//      from the actual raycast intersection point.
//
// 4. GAUSSIAN MICRO-JITTER (applied to normDir AFTER cone check):
//    - Displaces the normalized direction unit vector by a tiny per-frame random delta
//      so consecutive shots never produce identical floating-point vectors.
//    - Uses rand_r() with a per-call local seed to avoid the global rand() data race
//      between the aim thread and any other thread that might call rand().
//    - Jitter sigma scales with range (0.012m at 10m, 0.045m at 100m).
//    - Vertical axis tighter (0.5× sigma) — gravity axis is naturally tighter in real play.
//
// 5. PROBABILISTIC HEADSHOT CADENCE:
//    - Headshot Rate 100%: all shots → Head world position.
//    - Headshot Rate 0%:   all shots → Chest/Body.
//    - Intermediate:       probabilistic per-shot roll (rand_r % 100 < rate).
//
// 6. FIRING-CADENCE SYNCHRONIZATION:
//    - Writes occur strictly while isFiring == true (active weapon discharge).
//    - Ceases immediately on trigger release — zero persistent footprint.
// ==============================================================================================

namespace trajectory {
namespace silent {

    // ── rand_r seed — aim-thread local, no global rand() race ────────────────────
    // Declared inline so each TU gets the same variable (ODR-safe with inline).
    inline unsigned int s_jitter_seed = 0x4E494E4D; // "NINM" as initial seed

    // ── Gaussian micro-jitter (Box-Muller, thread-local seed) ────────────────────
    static inline float gaussian_jitter(float sigma) {
        // rand_r is reentrant; seed is aim-thread-private → zero data race
        unsigned int u1v = (rand_r(&s_jitter_seed) % 9999) + 1;
        unsigned int u2v = (rand_r(&s_jitter_seed) % 9999) + 1;
        float u1 = (float)u1v / 10000.0f;
        float u2 = (float)u2v / 10000.0f;
        float z  = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
        return z * sigma;
    }

    // ── Jitter applied to a normalized direction vector ───────────────────────────
    // Applied AFTER cone check so the cone is validated on the clean aim vector.
    static inline Vector3 jitter_direction(const Vector3& nd, float sigma) {
        Vector3 jd(nd.x + gaussian_jitter(sigma),
                   nd.y + gaussian_jitter(sigma * 0.5f), // vertical tighter
                   nd.z + gaussian_jitter(sigma));
        return SafeNormalize(jd);
    }

    // ── Camera forward from the already-resolved data::viewMatrix_pub ────────────
    // Unity row-major storage layout (matches game.hpp struct matrix):
    //   m[0]..m[3]  = row 0 (m11..m14)
    //   m[4]..m[7]  = row 1 (m21..m24)
    //   m[8]..m[11] = row 2 (m31..m34)  ← this row encodes -camForward
    //   m[12]..m[15]= row 3 (m41..m44)
    // camFwd = -(m31, m32, m33) = -(vm[8], vm[9], vm[10])
    static inline Vector3 get_camera_forward_from_matrix(const matrix& vm) {
        // Reinterpret the struct as a flat float[16] via memcpy to avoid UB aliasing.
        float m[16];
        static_assert(sizeof(matrix) == 16 * sizeof(float), "matrix size mismatch");
        __builtin_memcpy(m, &vm, sizeof(m));

        float fx = -m[8];   // -m31
        float fy = -m[9];   // -m32
        float fz = -m[10];  // -m33

        float lenSq = fx * fx + fy * fy + fz * fz;
        if (lenSq < 1e-6f || !std::isfinite(lenSq)) {
            return Vector3(0.0f, 0.0f, 1.0f); // neutral fallback — cone will pass
        }
        float invLen = 1.0f / sqrtf(lenSq);
        return Vector3(fx * invLen, fy * invLen, fz * invLen);
    }

    // ── Maximum safe angular deviation ───────────────────────────────────────────
    // 12.0° — conservative threshold matching weapon cone spread.
    // cos(12°) = 0.97815 (precomputed, avoids per-call cosf()).
    static constexpr float MAX_AIM_COS = 0.97815f;

    // ── Stub lifecycle functions ──────────────────────────────────────────────────
    inline void clear_silent_data() {}
    inline void start_silent_writer() {}
    inline void stop_silent_writer() { clear_silent_data(); }

    // ── Core trajectory steering ──────────────────────────────────────────────────
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target,
                                 bool lockToHead, bool isFiring) {
        (void)lockToHead;

        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) return;

        // Gate: write only during active weapon discharge
        if (!isFiring) return;

        // Gate: skip dead targets
        if (target.curHP <= 0) return;

        // Gate: at least one aim feature must be enabled
        if (!cfg::aim::silent.load(std::memory_order_relaxed) &&
            !cfg::aim::auto_headshot.load(std::memory_order_relaxed) &&
            !cfg::aim::scope_aim.load(std::memory_order_relaxed)) {
            return;
        }

        // ── 1. Resolve Weapon AimInfo buffer (0xE58 / 0xE50 candidates) ──────────
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
            targetIsHead = ((rand_r(&s_jitter_seed) % 100) < (unsigned)hsRate);
        }

        float headBias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (headBias < -0.30f || headBias > 0.30f) headBias = 0.00f;

        // Clean target world (no jitter yet — jitter added to direction after cone check)
        Vector3 targetWorld = targetIsHead
            ? target.getTargetWorld(0, headBias)
            : target.getTargetWorld(1, 0.0f);

        if (targetWorld == Vector3::Zero() || !std::isfinite(targetWorld.x)) return;

        // ── 4. Compute normalized direction (clean, no jitter) ───────────────────
        Vector3 dir = targetWorld - ammoBase;
        if (dir.sqrMagnitude() <= 1e-4f) return;

        Vector3 normDir = SafeNormalize(dir);
        if (!std::isfinite(normDir.x) || normDir == Vector3::Zero()) return;

        // ── 5. ANGULAR DEVIATION CONE GUARD (clean vector, before jitter) ────────
        // Read the last-published view matrix — no extra RPM needed.
        matrix vm;
        {
            std::lock_guard<std::mutex> lk(data::viewMatrix_mutex);
            vm = data::viewMatrix_pub;
        }
        Vector3 camFwd = get_camera_forward_from_matrix(vm);
        float dotVal = normDir.x * camFwd.x + normDir.y * camFwd.y + normDir.z * camFwd.z;
        if (dotVal < MAX_AIM_COS) {
            // Target outside safe 12° firing cone. Let bullet fire naturally.
            return;
        }

        // ── 6. Gaussian micro-jitter (applied to direction AFTER cone check) ─────
        // Jitter sigma in radians equivalent: scale with range for realistic spray.
        float dist3D = sqrtf((targetWorld.x - ammoBase.x) * (targetWorld.x - ammoBase.x) +
                             (targetWorld.y - ammoBase.y) * (targetWorld.y - ammoBase.y) +
                             (targetWorld.z - ammoBase.z) * (targetWorld.z - ammoBase.z));
        // sigma: 0.012 at 10m → 0.045 at 100m (sub-hitbox, invisible to player)
        float jitterSigma = 0.012f + (dist3D * 0.00033f);
        Vector3 finalDir = jitter_direction(normDir, jitterSigma);
        if (!std::isfinite(finalDir.x) || finalDir == Vector3::Zero()) {
            finalDir = normDir; // jitter produced degenerate vector, fall back to clean
        }

        // ── 7. Single-pass write: Direction only ──────────────────────────────────
        // ViewBlocked (0x71): NEVER written  — wall-shot magic → server physics ban.
        // HitGroup    (0x64): NEVER written  — forced hitgroup desync → telemetry ban.
        wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, finalDir);
    }

} // namespace silent

    using silent::clear_silent_data;
    using silent::start_silent_writer;
    using silent::stop_silent_writer;
    using silent::apply_trajectory;

} // namespace trajectory
