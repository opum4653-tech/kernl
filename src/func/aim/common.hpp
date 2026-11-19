#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "../../game/player.hpp"
#include "../../game/data.hpp"
#include "../../game/math.hpp"
#include "../../offsets/offsets.hpp"
#include "../../ui/cfg.hpp"
#include "../../other/memory.hpp"
#include "../../other/vector3.h"
#include "../../globals.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <unistd.h>

// ==============================================================================================
// FILE: src/func/aim/common.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Common synchronization primitives, telemetry structures, and math utilities for aimbot.
//
// CROSS-FILE CALL GRAPH & USAGE MAPPINGS:
// ----------------------------------------------------------------------------------------------
// 1. TELEMETRY SYNCHRONIZATION:
//    - Writer: src/func/aim/aim.hpp -> aimbot::update():
//      * Calls publish_target(info) with locked target screen position.
//    - Reader: src/func/aim/fov.hpp -> aimbot::render_fov():
//      * Calls snapshot_target() to draw purple target lock indicator ring.
//
// 2. HYSTERESIS & TARGET TRACKING:
//    - Managed by: src/func/aim/target.hpp -> acquire_target():
//      * s_lockedTargetAddr holds the active Il2Cpp entity pointer.
//      * s_lockTimeoutTicks grants a grace-period lease across rapid burst-firing intervals.
// ==============================================================================================

namespace aimbot {

    // Clean, cache-aligned telemetry structure for the FOV lock indicator
    struct TargetInfo {
        bool valid = false;                 // True when target lock is currently held
        Vector3 screenPos = Vector3::Zero();// 2D screen coordinate {x, y, z=depth}
    };

    // Mutex guarding cross-thread telemetry writes from aim worker to render loop
    inline std::mutex s_targetInfoMutex;
    inline TargetInfo g_currentTarget;

    // Thread-safe copy of the active target telemetry
    inline TargetInfo snapshot_target() {
        std::lock_guard<std::mutex> lk(s_targetInfoMutex);
        return g_currentTarget;
    }

    // Invalidates target telemetry when out of combat or target dies
    inline void publish_target_invalid() {
        std::lock_guard<std::mutex> lk(s_targetInfoMutex);
        g_currentTarget.valid = false;
    }

    // Publishes fresh target telemetry from aimbot evaluation cycle
    inline void publish_target(const TargetInfo& info) {
        std::lock_guard<std::mutex> lk(s_targetInfoMutex);
        g_currentTarget = info;
    }

    // Safe 3D vector normalization with division-by-zero & NaN protection
    inline Vector3 SafeNormalize(const Vector3& v) {
        float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq < 1e-6f || !std::isfinite(lenSq)) {
            return Vector3(0.0f, 0.0f, 1.0f); // Default forward vector fallback
        }
        float invLen = 1.0f / sqrtf(lenSq);
        return Vector3(v.x * invLen, v.y * invLen, v.z * invLen);
    }

    // Target stickiness with hysteresis & grace period to completely eliminate screen shaking
    inline uint64_t s_lockedTargetAddr = 0;
    inline int      s_lockTimeoutTicks = 0;

    // Direction blend vector across frame updates
    inline Vector3 s_smoothDir = Vector3::Zero();

    // Target entity pointer for which smoothed direction was calculated
    inline uint64_t s_smoothTargetAddr = 0;

} // namespace aimbot
