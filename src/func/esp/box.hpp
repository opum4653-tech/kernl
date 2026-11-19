#pragma once
#include "common.hpp"

// ==============================================================================================
// FILE: src/func/esp/box.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: 2D Bounding Box (Chaturbhuj) renderer with multi-pass glow borders and semi-transparent fills.
//
// CROSS-FILE CALL GRAPH & USAGE:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/func/esp/esp.hpp -> func::esp::draw():
//      * Calls drawBox(dl, tl, br, colBase, 1) when cfg::esp::box is enabled.
//
// 2. COORDINATES & GEOMETRY:
//    - Inputs tl (top-left) and br (bottom-right) are derived in computePlayerBox() in common.hpp.
//    - Bounding dimensions automatically scale based on enemy distance and humanoid aspect ratios.
//
// 3. RENDER STYLES:
//    - Type 1 (Full 2D Box / Chaturbhuj): Multi-pass soft glow border (3 gradient passes)
//      plus a 5% alpha interior fill for clear entity silhouette highlighting.
// ==============================================================================================

namespace func::esp {

    // Renders the 2D bounding box on the specified ImGui draw list
    inline void drawBox(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 col, int type) {
        float thick = cfg::esp::box_thickness;
        float round = cfg::esp::box_rounding;

        if (type == 0) {
            // Corner Bracket Box style
            float len = (br.x - tl.x) * 0.28f;
            ImVec2 pts[8][2] = {
                {tl,                 {tl.x + len, tl.y}},
                {tl,                 {tl.x, tl.y + len}},
                {{br.x - len, tl.y}, {br.x, tl.y}},
                {{br.x, tl.y},       {br.x, tl.y + len}},
                {{tl.x, br.y - len}, {tl.x, br.y}},
                {{tl.x, br.y},       {tl.x + len, br.y}},
                {{br.x - len, br.y}, {br.x, br.y}},
                {{br.x, br.y - len}, {br.x, br.y}},
            };
            for (auto& s : pts) {
                dl->AddLine(s[0], s[1], withAlpha(col, 0.25f), thick + 3.f);
                dl->AddLine({s[0].x + 1, s[0].y + 1},
                            {s[1].x + 1, s[1].y + 1}, C_SHADOW, thick + 1.f);
                dl->AddLine(s[0], s[1], col, thick);
            }
        } else if (type == 1) {
            // Full 2D Box (Chaturbhuj) with internal multi-pass orchid glow border
            for (int g = 1; g <= 3; g++) {
                dl->AddRect(
                    ImVec2(tl.x - g, tl.y - g),
                    ImVec2(br.x + g, br.y + g),
                    withAlpha(col, 0.35f - g * 0.08f),
                    round + g, 0, thick + g * 0.8f);
            }
            // Primary crisp inner border
            dl->AddRect(tl, br, col, round, 0, thick);
            // Subtle 5% transparent interior fill for depth
            dl->AddRectFilled(tl, br, withAlpha(col, 0.05f), round);
        } else {
            // Outline Box with Dual Alpha Borders
            dl->AddRect(tl, br, withAlpha(col, 0.18f), round, 0, thick + 5.f);
            dl->AddRect(tl, br, withAlpha(col, 0.45f), round, 0, thick + 2.f);
            dl->AddRect(tl, br, col, round, 0, thick);
            dl->AddRectFilled(tl, br, withAlpha(col, 0.06f), round);
        }
    }

} // namespace func::esp
