#pragma once
#include "common.hpp"
#include "box.hpp"
#include "line.hpp"
#include "health.hpp"
#include "info.hpp"

// ==============================================================================================
// FILE: src/func/esp/esp.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Master coordinator for the Visuals (ESP) rendering pipeline.
//
// CROSS-FILE CALL GRAPH & WORKFLOW:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/ui/menu.cpp -> ui::menu::render():
//      * Calls func::esp::draw() on every frame inside the main 60fps render loop.
//
// 2. INPUT DATA:
//    - Parsed enemy repository: data::players (from src/game/data.hpp).
//    - Visual configuration toggles: cfg::esp::box, cfg::esp::tracer, cfg::esp::armor,
//      cfg::esp::bot_tag (from src/ui/cfg.hpp).
//    - Range is FRUSTUM-ONLY: no distance slider. The camera view defines visibility.
//
// 3. EXECUTION DISPATCH (Per-Player Render Sequence):
//    Step 1: Check master active switch (is_active()).
//    Step 2: Obtain ImGui foreground draw list.
//    Step 3: Acquire data::players_mutex and copy snapshot of data::players.
//    Step 4: Filter off-screen entities via ok() (frustum screen-bounds guard).
//    Step 5: Compute 2D bounding box dimensions via computePlayerBox() (in box.hpp).
//    Step 6: Render active visual layers:
//      * Layer 1: Kernel Target Line (drawTracer() in line.hpp).
//      * Layer 2: Kernel Bounding Box (drawBox() in box.hpp).
//      * Layer 3: Kernel Entity Tag (drawBotTag() in info.hpp).
//      * Layer 4: Kernel Armor Indicator (drawSideArmorBar() in health.hpp).
// ==============================================================================================

namespace func::esp {

    // Main ESP rendering function executed every frame
    inline void draw() {
        if (!is_active()) return;

        // Foreground draw list guarantees visuals render on top of transparent surface
        ImDrawList* dl = ImGui::GetForegroundDrawList();

        // Thread-safe copy of active parsed players
        std::vector<PlayerData> snap;
        {
            std::lock_guard<std::mutex> lk(data::players_mutex);
            snap = data::players;
        }

        // Iterate all active enemies in snapshot
        for (const auto& p : snap) {
            // Frustum-only visibility: ESP shows exactly what the camera renders.
            // data.cpp already culls off-screen players; this is a defensive screen-bounds guard.
            if (!ok(p.Head) && !ok(p.Toe)) continue;
            if (p.curHP <= 0) continue;

            // No distance limit — the camera frustum is the only range.
            // Full-opacity color (alpha fade tied to frustum edge, not artificial distance).
            ImU32 colBase = toU32(cfg::esp::box_col);

            // 1. ESP Laser Line (Tracer line originating from top of screen to head - decoupled from box validity)
            if (cfg::esp::tracer) {
                drawTracer(dl, p, colBase);
            }

            // Calculate 2D bounding box rectangle
            PlayerBox pb = computePlayerBox(p);
            if (!pb.valid) continue;

            ImVec2 tl = pb.tl;
            ImVec2 br = pb.br;

            // Screen boundary frustum culling (reject elements off-screen)
            if (br.x < -30.f || tl.x > g_sw + 30.f) continue;
            if (br.y < -30.f || tl.y > g_sh + 30.f) continue;

            float cx = (tl.x + br.x) * 0.5f;

            // 2. Kernel Bounding Box (Full 2D Bounding Box)
            if (cfg::esp::box) {
                drawBox(dl, tl, br, colBase, 1); // Type 1 = Full 2D Box
            }

            // 3. ESP Bot Tag (AI server bot indicator tag above head)
            if (cfg::esp::bot_tag && p.isBot) {
                drawBotTag(dl, p, cx, tl.y);
            }

            // 4. ESP Armor Bar (Vertical armor point gauge on right side of 2D box)
            if (cfg::esp::armor && p.curArmor > 0) {
                drawSideArmorBar(dl, p, tl, br);
            }
        }
    }

} // namespace func::esp
