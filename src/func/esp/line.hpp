#pragma once
#include "common.hpp"

namespace func::esp {

    // TRACER LINE (ORIGINS: 0 = Top, 1 = Center / Reticle Dot, 2 = Bottom)
    // Snaps cleanly to the enemy body geometry:
    // - Top Origin (0): Snaps to the head / top of bounding box
    // - Center Origin (1): Snaps to the chest / center of bounding box
    // - Bottom Origin (2): Snaps to the feet / bottom of bounding box
    inline void drawTracer(ImDrawList* dl, const PlayerData& p, const PlayerBox& pb, ImU32 colBase) {
        if (!cfg::esp::tracer || !pb.valid) return;

        // Verify player is on screen
        if (pb.br.x < -30.f || pb.tl.x > g_sw + 30.f ||
            pb.br.y < -30.f || pb.tl.y > g_sh + 30.f) {
            return;
        }

        float midX = (pb.tl.x + pb.br.x) * 0.5f;
        float topY = pb.tl.y;
        float botY = pb.br.y;

        ImVec2 lineStart;
        ImVec2 target;

        if (cfg::esp::tracer_pos == 1) {
            // Screen Center / Official Reticle Dot Origin -> Target Chest / Center of Box
            lineStart = ImVec2(g_sw * 0.5f, g_sh * 0.5f);
            target = ImVec2(midX, (topY + botY) * 0.5f);
        } else if (cfg::esp::tracer_pos == 2) {
            // Screen Bottom Origin -> Target Feet / Bottom of Box
            lineStart = ImVec2(g_sw * 0.5f, g_sh);
            target = ImVec2(midX, botY);
        } else {
            // Screen Top Origin (Default) -> Target Head / Top of Box
            lineStart = ImVec2(g_sw * 0.5f, 0.f);
            target = ImVec2(midX, topY);
        }

        float aa = ImClamp(1.f - p.distance / cfg::esp::max_range, 0.25f, 1.f);
        float tThick = cfg::esp::tracer_thickness;

        // Dynamic laser line with soft ambient glow
        dl->AddLine(lineStart, target, withAlpha(colBase, 0.25f * aa), tThick + 1.4f);
        dl->AddLine(lineStart, target, withAlpha(colBase, 0.95f * aa), tThick);

        // If originating from center reticle dot, draw a small central glow dot
        if (cfg::esp::tracer_pos == 1) {
            dl->AddCircleFilled(lineStart, 2.5f, withAlpha(colBase, 0.85f));
            dl->AddCircle(lineStart, 4.0f, withAlpha(colBase, 0.40f), 12, 1.0f);
        }
    }

} // namespace func::esp
