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
//    - Multi-pass laser beam: outer translucent bloom (tThick + 1.4f) plus an
//      intense inner core (tThick). Uniform opacity — the camera frustum is
//      the only range; no distance fade or artificial cutoff.
// ==============================================================================================

namespace func::esp {

    // Renders the laser tracer line on the ImGui draw list (matching crack_master pattern)
    inline void drawTracer(ImDrawList* dl, const PlayerData& p, ImU32 colBase) {
        if (!cfg::esp::tracer) return;

        // ── FRUSTUM GATE (far-enemy fix) ────────────────────────────────────────
        // The old check demanded the HEAD point be strictly inside [0..g_sw]
        // × [0..g_sh]. Distant enemies are small and often sit with their head
        // just past the viewport edge while their feet are still visible —
        // those tracers were silently dropped. Use the same tolerant frustum
        // test as data.cpp / esp.hpp and accept either head or toe.
        const bool headOK = ok(p.Head);
        const bool toeOK  = ok(p.Toe);
        if (!headOK && !toeOK) return;

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

        // Snap to whichever bone is actually visible. For a far enemy whose
        // head sits just outside the viewport, the feet still give a valid
        // terminal point instead of losing the tracer entirely.
        ImVec2 target = headOK ? ImVec2(p.Head.x, p.Head.y)
                               : ImVec2(p.Toe.x, p.Toe.y);
        // Clamp into the physical viewport so the beam is always drawn to a
        // visible endpoint (ok() allows a 150px off-screen tolerance).
        target.x = ImClamp(target.x, 0.0f, (float)g_sw);
        target.y = ImClamp(target.y, 0.0f, (float)g_sh);

        // Uniform alpha — no distance limit, camera frustum is the only range.
        float aa = 1.0f;
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
