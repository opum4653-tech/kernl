#pragma once
#include "common.hpp"

// ==============================================================================================
// FILE: src/func/aim/fov.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: FOV kill-zone circle renderer and active locked-target indicator marker.
//
// CROSS-FILE CALL GRAPH & CONTRACTS:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/func/esp/common.hpp -> draw_overlay():
//      * Renders every frame before the main ImGui menu widgets.
//
// 2. DATA SOURCES:
//    - User settings: cfg::aim::show_fov (toggle), cfg::aim::get_fov_radius() (radius).
//    - Target lock telemetry: snapshot_target() (from src/func/aim/common.hpp).
//    - Screen dimensions: g_sw, g_sh (atomic screen resolution snapshots).
//
// 3. VISUAL ARCHITECTURE:
//    - Canvas: ImGui Background Draw List (dl = ImGui::GetBackgroundDrawList()).
//      Renders beneath floating logos and menu panels, but over game frames.
//    - Theme: Unified signature purple (#E10098) with dual-ring anti-aliased orchid glow.
//    - Lock Marker: Solid center dot + pulsing outer ring centered on target screenPos.
// ==============================================================================================

namespace trajectory {

    // Renders the FOV boundary circle and the active locked-target indicator marker
    inline void render_fov(ImDrawList* dl = nullptr) {
        if (!cfg::master_enabled) return;
        if (!cfg::aim::show_fov) return;

        if (!dl) {
            // Background draw list ensures the FOV ring sits under interactive menu controls
            dl = ImGui::GetBackgroundDrawList();
        }

        ImGuiIO& io = ImGui::GetIO();
        float currentSw = g_sw.load(std::memory_order_relaxed);
        float currentSh = g_sh.load(std::memory_order_relaxed);
        float sw = (currentSw > 0.0f) ? currentSw : ((io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : 2400.0f);
        float sh = (currentSh > 0.0f) ? currentSh : ((io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : 1080.0f);

        // Calculate screen center point
        ImVec2 center(sw * 0.5f, sh * 0.5f);
        float r = cfg::aim::get_fov_radius();
        if (r <= 0.0f) return;

        // 1. Primary FOV circle in unified signature purple (#E10098)
        dl->AddCircle(center, r, IM_COL32(225, 0, 152, 235), 64, 1.8f);

        // 2. Outer soft orchid glow ring for visual depth and anti-aliased appearance
        dl->AddCircle(center, r + 1.2f, IM_COL32(255, 90, 205, 120), 64, 1.0f);

        // 3. Active Target Lock Indicator: displayed only during active match when target is locked
        if (data::inMatch.load(std::memory_order_relaxed)) {
            TargetInfo t = snapshot_target();
            if (t.valid && t.screenPos.z >= 0.001f && t.screenPos.x > 1.0f && t.screenPos.y > 1.0f) {
                ImVec2 pos(t.screenPos.x, t.screenPos.y);
                // Outer tracking ring
                dl->AddCircle(pos, 12.0f, IM_COL32(225, 0, 152, 240), 24, 2.0f);
                // Inner solid tracking dot
                dl->AddCircleFilled(pos, 3.5f, IM_COL32(255, 90, 205, 255));
            }
        }
    }

} // namespace trajectory
