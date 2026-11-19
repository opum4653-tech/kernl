#pragma once
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "../../game/data.hpp"
#include "../../offsets/offsets.hpp"
#include "../../ui/cfg.hpp"
#include "../../globals.hpp"
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cmath>

// ==============================================================================================
// FILE: src/func/esp/common.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Common mathematical geometry, color conversions, and 2D bounding box derivations for ESP.
//
// CROSS-FILE CALL GRAPH & USAGE:
// ----------------------------------------------------------------------------------------------
// 1. CALLERS:
//    - src/func/esp/esp.hpp -> func::esp::draw():
//      * Calls is_active() to verify if any visual switch is enabled.
//      * Calls computePlayerBox() to calculate 2D screen bounding rectangles.
//      * Calls fadeDist() to apply soft distance-based alpha attenuation.
//    - src/func/esp/box.hpp, line.hpp, health.hpp, info.hpp:
//      * Uses color manipulation helpers (toU32, withAlpha, armorColor).
//
// 2. BOUNDING BOX DERIVATION ALGORITHM (computePlayerBox):
//    - Derives vertical height from projected Head and Toe screen coordinates.
//    - Enforces humanoid aspect ratio: width = height * Offsets::ESP::BoxWidthRatio (0.48f).
//    - Adds anatomical helmet padding above head (12%) and boots padding below feet (5%).
//    - Horizontally centers bounding box along natural spinal posture axis: (Head.x + Toe.x) * 0.5f.
// ==============================================================================================

namespace func::esp {

    // Returns true if master switch and at least one ESP feature is active
    inline bool is_active() {
        if (!cfg::master_enabled) return false;
        return cfg::esp::box || cfg::esp::armor || cfg::esp::tracer || cfg::esp::bot_tag;
    }

    // Converts an ImVec4 color struct to packed 32-bit integer color (ARGB/ABGR)
    inline ImU32 toU32(const ImVec4& c, float alphaMul = 1.f) {
        return IM_COL32(
            (int)(c.x * 255.f),
            (int)(c.y * 255.f),
            (int)(c.z * 255.f),
            (int)(c.w * 255.f * alphaMul));
    }

    // Modifies the alpha opacity of a 32-bit packed color while preserving RGB channels
    inline ImU32 withAlpha(ImU32 c, float mul) {
        float aF = ((float)((c >> 24) & 0xFF)) * mul;
        int a = (int)ImClamp(aF, 0.0f, 255.0f);
        return (c & 0x00FFFFFF) | ((ImU32)(uint32_t)a << 24);
    }

    // Applies linear distance attenuation to fade visuals softly as enemies approach max range
    inline ImU32 fadeDist(ImU32 base, float dist, float maxRange) {
        if (maxRange <= 0.0f) return base;
        float t = ImClamp(1.f - dist / maxRange, 0.35f, 1.f);
        return withAlpha(base, t);
    }

    // Computes dynamic health gradient: Green (100%) -> Yellow (50%) -> Red (0%)
    inline ImU32 hpColor(float t) {
        t = ImClamp(t, 0.f, 1.f);
        float r, g, b;
        if (t < 0.5f) { float k = t * 2.f; r = 1.f; g = 0.35f + 0.55f * k; b = 0.15f; }
        else          { float k = (t - 0.5f) * 2.f; r = 1.f - 0.9f * k; g = 0.9f; b = 0.15f + 0.30f * k; }
        return IM_COL32((int)(r*255), (int)(g*255), (int)(b*255), 255);
    }

    // Computes dynamic armor gradient in high-visibility sky-blue / cyan palette
    inline ImU32 armorColor(float t) {
        t = ImClamp(t, 0.f, 1.f);
        return IM_COL32((int)(80 + 100 * t), (int)(180 + 60 * t), 255, 255);
    }

    // Validates whether a projected screen coordinate is inside visible display bounds
    inline bool ok(const Vector3& v) {
        return v.z > 0.5f && v.x > -150.0f && v.x < (g_sw + 150.0f) && v.y > -150.0f && v.y < (g_sh + 150.0f);
    }

    // Formats distance in meters or kilometers
    inline void fmtDist(float d, char* out, size_t n) {
        if (d >= 1000.f) snprintf(out, n, "%.1fkm", d / 1000.f);
        else             snprintf(out, n, "%.0fm", d);
    }

    // Truncates strings with trailing ellipsis if exceeding target character width
    inline void truncName(const char* src, char* out, size_t n) {
        if (!src || !src[0]) { out[0] = 0; return; }
        size_t len = strlen(src);
        if (len <= 12) { snprintf(out, n, "%s", src); return; }
        snprintf(out, n, "%.11s.", src);
    }

    // Visual theme palette constants
    static constexpr ImU32 C_SHADOW = IM_COL32(0, 0, 0, 180);
    static constexpr ImU32 C_BORDER = IM_COL32(255, 255, 255, 40);
    static constexpr float ESP_FONT_SIZE = 12.f;

    // Geometric bounding rectangle descriptor
    struct PlayerBox {
        ImVec2 tl, br;  // Top-left and bottom-right 2D screen coordinates
        bool   valid;   // True if projection successfully resolved on screen
    };

    // Derives an accurate 2D humanoid bounding box from 3D projected Head and Toe positions
    inline PlayerBox computePlayerBox(const PlayerData& p) {
        PlayerBox pb{ {0,0}, {0,0}, false };
        if (!ok(p.Head)) return pb;

        float topY = p.Head.y;
        float botY = ok(p.Toe) ? p.Toe.y : (p.Head.y + 50.0f);
        if (topY > botY) std::swap(topY, botY);

        float height = botY - topY;
        if (height < 6.f) {
            // Distance-based height fallback if toe node is occluded or compressed
            float curSh = g_sh.load(std::memory_order_relaxed);
            if (curSh < 200.0f) curSh = 1080.0f;
            float maxH = std::max(15.0f, curSh * 0.9f);
            float calcH = (curSh * 1.6f) / std::max(p.distance, 1.0f);
            height = std::clamp(calcH, 15.0f, maxH);
            botY = topY + height;
        }

        // Align horizontally centered between head and toe (natural spine alignment)
        float midX = ok(p.Toe) ? ((p.Head.x + p.Toe.x) * 0.5f) : p.Head.x;
        float width = height * Offsets::ESP::BoxWidthRatio;
        float halfW = width * 0.5f;

        // Head bone is at eye level, add helmet padding above head (Offsets::ESP::BoxPadTopPercent = 12%)
        // Toe bone is at sole level, add boots padding below feet (Offsets::ESP::BoxPadBotPercent = 5%)
        const float padTop = height * Offsets::ESP::BoxPadTopPercent;
        const float padBot = height * Offsets::ESP::BoxPadBotPercent;

        pb.tl = ImVec2(midX - halfW, topY - padTop);
        pb.br = ImVec2(midX + halfW, botY + padBot);
        pb.valid = true;
        return pb;
    }

} // namespace func::esp
