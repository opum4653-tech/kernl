#pragma once
#include "common.hpp"

// ==============================================================================================
// FILE: src/func/esp/line.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: ESP Laser Line (Tracer) renderer connecting screen anchor points to enemy targets.
//
// CROSS-FILE CALL GRAPH & ANCHOR MODES:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/func/esp/esp.hpp -> func::esp::draw():
//      * Calls drawTracer(dl, p, pb, colBase) when cfg::esp::tracer is enabled.
//
// 2. LASER ANCHOR MODES (cfg::esp::tracer_pos):
//    - Mode 0 (Top Origin - Default): Originates from screen top center (sw/2, 0) -> snaps to Head.
//    - Mode 1 (Center Reticle Origin): Originates from screen center (sw/2, sh/2) -> snaps to Chest.
//    - Mode 2 (Bottom Origin): Originates from screen bottom (sw/2, sh) -> snaps to Feet.
//
// 3. VISUAL RENDERING:
//    - Multi-pass laser beam: draws an outer translucent bloom beam (tThick + 1.4f)
//      and an intense inner laser core beam (tThick) with distance-based alpha fading.
// ==============================================================================================

namespace func::esp {

    // Renders the laser tracer line on the ImGui draw list (matching crack_master pattern)
    inline void drawTracer(ImDrawList* dl, const PlayerData& p, ImU32 colBase) {
        if (!cfg::esp::tracer) return;

        // Strictly draw tracer ONLY when head point is in front of camera and inside screen bounds
        if (p.Head.z < 0.05f || p.Head.x < 0.f || p.Head.x > g_sw ||
            p.Head.y < 0.f || p.Head.y > g_sh) {
            return;
        }

        ImVec2 lineStart;
        if (cfg::esp::tracer_pos == 1) {
            // Mode 1: Screen Center Origin -> snaps to Center / Chest
            lineStart = ImVec2(g_sw * 0.5f, g_sh * 0.5f);
        } else if (cfg::esp::tracer_pos == 2) {
            // Mode 2: Screen Bottom Origin -> snaps to Feet / Ground
            lineStart = ImVec2(g_sw * 0.5f, g_sh);
        } else {
            // Mode 0: Screen Top Origin (Default) -> snaps directly to Head coordinates
            lineStart = ImVec2(g_sw * 0.5f, 0.f);
        }

        ImVec2 target = ImVec2(p.Head.x, p.Head.y);

        // Distance-based alpha fade (closer enemies have more intense laser lines)
        float maxR = (cfg::esp::max_range.load() > 0.0f) ? cfg::esp::max_range.load() : 500.0f;
        float aa = ImClamp(1.f - p.distance / maxR, 0.25f, 1.f);
        float tThick = cfg::esp::tracer_thickness;

        // Pass 1: Outer soft ambient glow
        dl->AddLine(lineStart, target, withAlpha(colBase, 0.25f * aa), tThick + 1.4f);

        // Pass 2: Sharp focused inner laser beam
        dl->AddLine(lineStart, target, withAlpha(colBase, 0.95f * aa), tThick);

        // Mode 1 aesthetic accent: draw central reticle emitter dot
        if (cfg::esp::tracer_pos == 1) {
            dl->AddCircleFilled(lineStart, 2.5f, withAlpha(colBase, 0.85f));
            dl->AddCircle(lineStart, 4.0f, withAlpha(colBase, 0.40f), 12, 1.0f);
        }
    }

} // namespace func::esp
