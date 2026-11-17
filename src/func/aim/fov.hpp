#pragma once
#include "common.hpp"

namespace aimbot {

    // FOV CIRCLE & ACTIVE TARGET LOCK VISUALIZER (Matching pewpew draw_overlay architecture)
    inline void render_fov(ImDrawList* dl = nullptr) {
        if (!cfg::master_enabled) return;
        if (!cfg::aim::show_fov) return;

        if (!dl) {
            dl = ImGui::GetForegroundDrawList();
        }

        ImGuiIO& io = ImGui::GetIO();
        float currentSw = g_sw.load(std::memory_order_relaxed);
        float currentSh = g_sh.load(std::memory_order_relaxed);
        float sw = (currentSw > 0.0f) ? currentSw : ((io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : 2400.0f);
        float sh = (currentSh > 0.0f) ? currentSh : ((io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : 1080.0f);

        ImVec2 center(sw * 0.5f, sh * 0.5f);
        float r = cfg::aim::get_fov_radius();
        if (r < 30.0f) r = 180.0f;

        // Clean, visible FOV Circle at screen center (pewpew style)
        // Main visible circle (crisp white, 1.5px)
        dl->AddCircle(center, r, IM_COL32(255, 255, 255, 220), 64, 1.5f);
        // Outer aesthetic theme glow (electric orchid, 1.0px)
        dl->AddCircle(center, r + 1.0f, IM_COL32(225, 0, 153, 140), 64, 1.0f);

        // Active Target Lock Indicator (if in match and locked onto enemy)
        if (data::inMatch.load(std::memory_order_relaxed)) {
            TargetInfo t = snapshot_target();
            if (t.valid && t.screenPos.z >= 0.001f && t.screenPos.x > 1.0f && t.screenPos.y > 1.0f) {
                ImVec2 pos(t.screenPos.x, t.screenPos.y);
                dl->AddCircle(pos, 12.0f, IM_COL32(255, 40, 80, 230), 24, 2.0f);
                dl->AddCircleFilled(pos, 3.5f, IM_COL32(255, 255, 255, 255));
            }
        }
    }

} // namespace aimbot
