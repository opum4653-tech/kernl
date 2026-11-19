#pragma once
#include "common.hpp"
#include "box.hpp"
#include "line.hpp"
#include "health.hpp"
#include "info.hpp"

// ==============================================================================================
// FILE: src/func/esp/esp.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
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
//      cfg::esp::bot_tag, cfg::esp::max_range (from src/ui/cfg.hpp).
//
// 3. EXECUTION DISPATCH (Per-Player Render Sequence):
//    Step 1: Check master active switch (is_active()).
//    Step 2: Obtain ImGui background draw list (dl = ImGui::GetBackgroundDrawList()).
//    Step 3: Acquire data::players_mutex and copy snapshot of data::players.
//    Step 4: Filter out-of-range (> max_range) or behind-camera (z < 0.5f) entities.
//    Step 5: Compute 2D bounding box dimensions via computePlayerBox() (in box.hpp).
//    Step 6: Render active visual layers:
//      * Layer 1: ESP Laser Line (drawTracer() in line.hpp).
//      * Layer 2: ESP 2D Box / Chaturbhuj (drawBox() in box.hpp).
//      * Layer 3: ESP Bot Tag AI badge (drawBotTag() in info.hpp).
//      * Layer 4: ESP Armor Bar (drawSideArmorBar() in health.hpp).
// ==============================================================================================

namespace func::esp {

    // Main ESP rendering function executed every frame
    inline void draw() {
        if (!is_active()) return;

        // Background draw list ensures visuals render beneath interactive ImGui menus & floating logo
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // Thread-safe copy of active parsed players
        std::vector<PlayerData> snap;
        {
            std::lock_guard<std::mutex> lk(data::players_mutex);
            snap = data::players;
        }

        // Iterate all active enemies in snapshot
        for (const auto& p : snap) {
            // Reject entities behind near clipping plane or dead entities
            if (p.Head.z < 0.5f || p.Toe.z < 0.5f) continue;
            if (p.curHP <= 0) continue;

            // Reject entities beyond the configured max render distance slider
            if (p.distance > cfg::esp::max_range) continue;

            // Calculate 2D bounding box rectangle
            PlayerBox pb = computePlayerBox(p);
            if (!pb.valid) continue;

            ImVec2 tl = pb.tl;
            ImVec2 br = pb.br;

            // Screen boundary frustum culling (reject elements off-screen)
            if (br.x < -30.f || tl.x > g_sw + 30.f) continue;
            if (br.y < -30.f || tl.y > g_sh + 30.f) continue;

            float cx = (tl.x + br.x) * 0.5f;

            // Distance-based alpha fade for soft aesthetic blending at range boundaries
            ImU32 colBase = toU32(cfg::esp::box_col);
            colBase = fadeDist(colBase, p.distance, cfg::esp::max_range);

            // 1. ESP Laser Line (Tracer line originating from top of screen to head)
            if (cfg::esp::tracer) {
                drawTracer(dl, p, pb, colBase);
            }

            // 2. ESP Box (Full 2D Bounding Box — Chaturbhuj)
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
