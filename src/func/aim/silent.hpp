#pragma once
#include "common.hpp"
#include <atomic>
#include <cstdlib>
#include <cmath>
#include <chrono>

// ==============================================================================================
// FILE: src/func/aim/silent.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 3D Silent Aim & Bullet Trajectory Steering Engine with Humanized Telemetry.
//
// ANTI-BAN ARCHITECTURAL PRINCIPLES (Phase 2 — Hardened):
// ----------------------------------------------------------------------------------------------
// 0. NEW IN PHASE 2:
//    - Jitter applied to ALL shots (head included) with a tighter head sigma.
//    - ~12% of bullet windows skipped entirely so bursts are not 100% perfect.
//    - Cadence jitter (±3ms) on the aim loop breaks machine-periodic timing.
//    - Cone tightened 12° -> 8°, plus a stale-camera guard on viewMatrix_stamp.
//    - Final jittered vector re-validated against the cone before the write.
//
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

    // ── rand_r seed — thread_local for complete thread isolation ─────────────────
    inline thread_local unsigned int s_jitter_seed = 0x4E494E4D; // "NINM" as initial seed

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
            return Vector3::Zero();
        }
        float invLen = 1.0f / sqrtf(lenSq);
        return Vector3(fx * invLen, fy * invLen, fz * invLen);
    }

    // ── Maximum safe angular deviation ───────────────────────────────────────────
    // 8.0° — tightened from 12°. A tighter cone means the steered bullet stays
    // close to where the camera actually points, so server-side trajectory
    // validation sees a natural shot. cos(8°) = 0.99027 (precomputed).
    static constexpr float MAX_AIM_COS = 0.99027f;

    // ── Per-shot steering skip probability ───────────────────────────────────────
    // ~12% of bullet windows are deliberately left unsteered so a full-auto burst
    // is not 100% perfect. A human misses; an always-perfect hit ratio is a flag.
    // NOTE: the ~12% per-bullet steering skip lives in aim.hpp update(), beside
    // the held-trigger steering gate (keeps the skip policy next to its trigger).

    // ── Fresh-seed helper (per-shot, thread-local) ───────────────────────────────
    // Mixes a monotonic counter into the seed so consecutive shots never share
    // the same PRNG stream state even if timing is identical.
    static inline unsigned int fresh_seed() {
        static thread_local uint64_t call_counter = 0;
        call_counter++;
        unsigned int mixed = s_jitter_seed ^
            (unsigned int)(call_counter * 2654435761ULL); // Knuth multiplicative hash
        return mixed ? mixed : 1u;
    }

    // ── Stub lifecycle functions ──────────────────────────────────────────────────
    inline void clear_silent_data() {}
    inline void start_silent_writer() {}
    inline void stop_silent_writer() { clear_silent_data(); }

    // ── Core trajectory steering ──────────────────────────────────────────────────
    // headRate = effective headshot probability [0..100] from the rate controller.
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target,
                                 float headRate, bool shouldApply) {
        if (!shouldApply) return;
        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) return;

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
                                            Offsets::Player::AimInfo_Alt,
                                            0xE50,
                                            0xE58 };
        for (uintptr_t off : aimCandidates) {
            uint64_t cand = rpm<uint64_t>(localPlayer + off);
            if (is_valid_ptr(cand)) { hitObjAddress = cand; break; }
        }
        if (!is_valid_ptr(hitObjAddress)) return;

        // ── 2. Read muzzle start position (0x4C) ─────────────────────────────────
        Vector3 ammoBase = rpm<Vector3>(hitObjAddress + Offsets::Aim::StartPosition);
        auto is_valid_vec3 = [](const Vector3& v) -> bool {
            return (v != Vector3::Zero() &&
                    std::isfinite(v.x) &&
                    std::isfinite(v.y) &&
                    std::isfinite(v.z));
        };

        if (!is_valid_vec3(ammoBase)) {
            uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone::Head);
            if (localHead) ammoBase = GetNodePosition(localHead);
        }
        if (!is_valid_vec3(ammoBase)) return;

        // ── 3. Rate-controller headshot decision ──────────────────────────────────
        // headRate is the DISTANCE-AWARE effective rate from evaluate_headshot_rate().
        // One 0-100 roll decides head vs body — the user controls frequency via the
        // slider, and distance naturally reduces headshots at range.
        // NO hard 100% force remains: even Auto Headshot ON obeys the slider.
        // rate 0 -> targetIsHead=false -> steers to CHEST (body), never skips the shot.
        bool targetIsHead;
        if (headRate >= 100.0f) {
            targetIsHead = true;
        } else if (headRate <= 0.0f) {
            targetIsHead = false;
        } else {
            // Sub-percentage precision using 0..1000 roll for fine rate control.
            unsigned int roll = (unsigned int)(rand_r(&s_jitter_seed) % 1000);
            targetIsHead = (roll < (unsigned int)(headRate * 10.0f));
        }

        float headBias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (headBias < -0.30f || headBias > 0.30f) headBias = 0.00f;

        // Clean target world position
        Vector3 targetWorld = targetIsHead
            ? target.getTargetWorld(0, headBias)
            : target.getTargetWorld(1, 0.0f);

        if (!is_valid_vec3(targetWorld)) return;

        // ── 4. Compute normalized direction unit vector ───────────────────────────
        Vector3 dir = targetWorld - ammoBase;
        float dist3D = sqrtf(dir.sqrMagnitude());
        if (dist3D < 0.001f || !std::isfinite(dist3D)) return;

        Vector3 normDir = SafeNormalize(dir);
        if (!std::isfinite(normDir.x) || !std::isfinite(normDir.y) || !std::isfinite(normDir.z) || normDir == Vector3::Zero()) return;

        matrix vm{};
        uint64_t stamp = 0;
        {
            std::lock_guard<std::mutex> lk(data::viewMatrix_mutex);
            vm = data::viewMatrix_pub;
            stamp = data::viewMatrix_stamp.load(std::memory_order_acquire);
        }
        // ── Staleness guard (time-based) ──────────────────────────────────────────
        // Abort when the camera matrix stamp has not advanced for 150ms. A
        // repeat count would be cadence-dependent: at an 8ms aim tick three
        // repeats are only 24ms (normal, data thread runs at 33ms), but at a
        // slower tick they could mean a genuine stall. Elapsed wall time is
        // the same signal at every polling rate.
        static thread_local uint64_t s_lastStamp = 0;
        static thread_local std::chrono::steady_clock::time_point s_stampChangedAt{};
        if (stamp != 0 && stamp == s_lastStamp) {
            auto staleMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - s_stampChangedAt).count();
            if (staleMs > 150) return;
        } else {
            s_lastStamp = stamp;
            s_stampChangedAt = std::chrono::steady_clock::now();
        }

        Vector3 camFwd = get_camera_forward_from_matrix(vm);
        if (camFwd == Vector3::Zero()) return;
        float align = Vector3::Dot(camFwd, normDir);
        if (!std::isfinite(align) || align < MAX_AIM_COS) return;

        // ── Jitter on ALL shots (headshots included) ──────────────────────────────
        // A 100% headshot config previously wrote identical perfect vectors.
        // Headshots get a TIGHTER sigma so they still land on the head node,
        // while body shots use slightly wider natural-spread-scale noise.
        // Sigma is an ANGULAR offset on the unit vector (radians-ish), kept small
        // on purpose: large values at long range caused lateral misses.
        float span = dist3D - 10.0f;
        if (span < 0.0f) span = 0.0f;
        if (span > 90.0f) span = 90.0f;
        float t = span / 90.0f; // 0..1

        float jitterSigma = targetIsHead
            ? (0.0015f + t * 0.0015f)  // head: ~0.09°..0.17° — stays on head node
            : (0.0050f + t * 0.0050f); // body: ~0.29°..0.57° — natural spread

        // Per-shot seed refresh so consecutive vectors always differ.
        unsigned int shotSeed = fresh_seed();
        unsigned int savedSeed = s_jitter_seed;
        s_jitter_seed = shotSeed;
        Vector3 finalDir = jitter_direction(normDir, jitterSigma);
        s_jitter_seed = savedSeed;

        if (!std::isfinite(finalDir.x) || !std::isfinite(finalDir.y) ||
            !std::isfinite(finalDir.z) || finalDir == Vector3::Zero()) {
            finalDir = normDir; // safe fallback — never write garbage
        }

        // Re-validate cone on the JITTERED vector — jitter must not push the
        // final shot outside the safe deviation cone.
        float finalAlign = Vector3::Dot(camFwd, finalDir);
        if (!std::isfinite(finalAlign) || finalAlign < MAX_AIM_COS) {
            finalDir = normDir; // fall back to clean vector (already cone-validated)
        }

        wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, finalDir);
    }

} // namespace silent

    using silent::clear_silent_data;
    using silent::start_silent_writer;
    using silent::stop_silent_writer;
    using silent::apply_trajectory;

} // namespace trajectory
