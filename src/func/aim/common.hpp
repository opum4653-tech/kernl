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

namespace aimbot {

    struct TargetInfo {
        bool valid = false;
        char name[36] = {0};
        float distance = 0.0f;
        int hp = 0;
        Vector3 screenPos = Vector3::Zero();
        Vector3 headWorld = Vector3::Zero();
    };

    // Target telemetry is written by the aim worker thread and read by the
    // render thread (menu readout + FOV lock ring). Guard it or the render
    // side reads a half-written name[] / vector and prints garbage.
    inline std::mutex s_targetInfoMutex;
    inline TargetInfo g_currentTarget;

    inline TargetInfo snapshot_target() {
        std::lock_guard<std::mutex> lk(s_targetInfoMutex);
        return g_currentTarget;
    }

    inline void publish_target_invalid() {
        std::lock_guard<std::mutex> lk(s_targetInfoMutex);
        g_currentTarget.valid = false;
    }

    inline void publish_target(const TargetInfo& info) {
        std::lock_guard<std::mutex> lk(s_targetInfoMutex);
        g_currentTarget = info;
    }

    // Safe 3D vector normalization with division-by-zero & NaN protection
    inline Vector3 SafeNormalize(const Vector3& v) {
        float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq < 1e-6f || !std::isfinite(lenSq)) {
            return Vector3(0.0f, 0.0f, 1.0f); // Default forward vector
        }
        float invLen = 1.0f / sqrtf(lenSq);
        return Vector3(v.x * invLen, v.y * invLen, v.z * invLen);
    }

    // Target stickiness with hysteresis & grace period to completely eliminate screen shaking
    inline uint64_t s_lockedTargetAddr = 0;
    inline int      s_lockTimeoutTicks = 0;

    // Smooth direction tracking across frame updates. Used by traditional aim
    // assist; silent aim and auto headshot snap instead of blending.
    inline Vector3 s_smoothDir = Vector3::Zero();

    // Target the current smoothed direction was built against. A new target
    // resets the blend so the acquisition frame snaps instead of sweeping the
    // bullet across the screen from the previous target's vector.
    inline uint64_t s_smoothTargetAddr = 0;

} // namespace aimbot
