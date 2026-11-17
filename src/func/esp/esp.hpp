#pragma once
#include "common.hpp"
#include "box.hpp"
#include "line.hpp"
#include "health.hpp"
#include "info.hpp"

namespace func::esp {

    // MAIN ESP DRAW PIPELINE — STRICT 16 CORE ARCHITECTURE
    inline void draw() {
        if (!is_active()) return;

        // Background draw list: rendered before every ImGui window, so the menu
        // and the floating logo always sit above ESP.
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        std::vector<PlayerData> snap;
        {
            std::lock_guard<std::mutex> lk(data::players_mutex);
            snap = data::players;
        }

        for (const auto& p : snap) {
            if (p.Head.z < 0.5f || p.Toe.z < 0.5f) continue;
            if (p.curHP <= 0) continue;
            if (p.distance > cfg::esp::max_range) continue;

            PlayerBox pb = computePlayerBox(p);
            if (!pb.valid) continue;

            ImVec2 tl = pb.tl;
            ImVec2 br = pb.br;

            if (br.x < -30.f || tl.x > g_sw + 30.f) continue;
            if (br.y < -30.f || tl.y > g_sh + 30.f) continue;

            float cx = (tl.x + br.x) * 0.5f;

            ImU32 colBase = toU32(cfg::esp::box_col);
            colBase = fadeDist(colBase, p.distance, cfg::esp::max_range);

            // 1. ESP Laser Line (Tracer)
            if (cfg::esp::tracer) {
                drawTracer(dl, p, pb, colBase);
            }

            // 2. ESP Box (Chaturbhuj)
            if (cfg::esp::box) {
                drawBox(dl, tl, br, colBase, 1); // Type 1 = Full 2D Box (Chaturbhuj)
            }

            // 3. ESP Bot Tag (AI)
            if (cfg::esp::bot_tag && p.isBot) {
                drawBotTag(dl, p, cx, tl.y);
            }

            // 4. ESP Armor Bar
            if (cfg::esp::armor && p.curArmor > 0) {
                drawSideArmorBar(dl, p, tl, br);
            }
        }
    }

} // namespace func::esp
