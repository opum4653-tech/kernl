#pragma once
#include "common.hpp"
#include <algorithm>

// ==============================================================================================
// FILE: src/func/aim/headshot.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Rate-controlled headshot evaluator (distance-aware) and zero-footprint decoupling interface.
//
// RATE-CONTROLLED HEADSHOT (user-configured, Phase 2.6):
// ----------------------------------------------------------------------------------------------
// "Auto Headshot" no longer forces 100% headshots. It now ENABLES the rate controller:
//   - Effective rate = cfg::aim::headshot_rate slider (0..100%)
//   - Distance scaling: beyond 40m the rate gently drops (natural weapon falloff —
//     real players land fewer headshots at range, so an ML detector sees a believable curve)
//   - Scope Aim while scoping adds a precision bonus (uplift toward head), still bounded
//   - When neither Auto Headshot nor Scope Aim is active: 0% (all body shots)
// Result: the user fully controls headshot frequency via one slider, and it behaves
// like a real gun instead of a fixed 100% machine.
//
// ARCHITECTURAL AUTOPSY & SAFETY PROTOCOL:
// ----------------------------------------------------------------------------------------------
// 1. LEGACY VS MODERN ARCHITECTURE:
//    - Legacy Pattern: In older mods, auto headshot swapped LockedAimingCollider (0x80) on the
//      enemy entity to force head hitboxes. This caused entity ESP drops, memory corruption,
//      and instant anti-cheat detection due to unexpected in-memory collider mutation.
//    - Modern Zero-Footprint Pattern: The modern engine steers bullets purely via 3D raycast
//      trajectory vectors in silent.hpp (flooding hitObj + 0x40). Enemy entity memory remains
//      100% untouched and unpatched (zero WPM footprint).
//
// 2. CROSS-FILE CALL GRAPH:
//    - Caller: src/func/aim/aim.hpp -> trajectory::update():
//      * Calls evaluate_headshot(target, isScoping) to authorize head targeting.
//      * Calls restore_headshot_override() upon target loss or overlay shutdown.
// ==============================================================================================

namespace trajectory {

    // Clean restore hook: maintains zero-footprint memory invariant (no entity memory to restore)
    inline void restore_headshot_override() {
        // Zero entity memory footprint — trajectory steering is localized to weapon raycast buffer
    }

    // Distance-aware headshot rate controller.
    // Returns the EFFECTIVE headshot probability [0..100] for this shot:
    //   - Auto Headshot ON  -> use the headshot_rate slider as the base rate
    //   - Scope Aim scoping -> precision uplift toward the head (still slider-bounded)
    //   - Neither           -> 0 (body shots only)
    // Beyond 40m the rate decays smoothly so long-range engagements show a natural
    // head/body mix instead of an impossible all-head pattern.
    inline float evaluate_headshot_rate(const PlayerData& target, bool isScoping) {
        bool autoOn  = cfg::aim::auto_headshot.load(std::memory_order_relaxed);
        bool scopeOn = cfg::aim::scope_aim.load(std::memory_order_relaxed) && isScoping;

        if (!autoOn && !scopeOn) return 0.0f;

        float base = (float)cfg::aim::headshot_rate.load(std::memory_order_relaxed);
        if (base < 0.0f) base = 0.0f;
        if (base > 100.0f) base = 100.0f;

        // Scope precision bonus: scoped shots are naturally more accurate.
        // Uplift = 25% of the remaining gap to 100, capped so it never forces 100%.
        if (scopeOn) {
            base += (100.0f - base) * 0.25f;
            if (base > 95.0f) base = 95.0f;
        }

        // Distance falloff: real weapons land fewer headshots at range.
        // 40m -> full rate; 140m -> 60% of rate. Smooth linear decay.
        // The 5% minimum applies ONLY to a non-zero base rate: a user who sets
        // 0% must still get 0% headshots at any distance (bug fix).
        float d = target.distance;
        if (d > 40.0f && base > 0.0f) {
            float excess = d - 40.0f;
            float factor = 1.0f - std::min(excess / 100.0f, 1.0f) * 0.40f;
            base *= factor;
            if (base < 5.0f) base = 5.0f;
        }

        return base;
    }

    // Backward-compatible boolean form: true when the effective rate is high enough
    // that the caller may prefer head as the default target before roll.
    // IMPORTANT: this is NO LONGER a hard 100% force — silent.hpp uses the float
    // rate above for the actual probabilistic decision.
    inline bool evaluate_headshot(const PlayerData& target, bool isScoping) {
        return evaluate_headshot_rate(target, isScoping) >= 100.0f;
    }

} // namespace trajectory
