#pragma once
#include "common.hpp"

namespace func::esp {

    // BOT TAG (AI) RENDERING ABOVE 2D BOX
    // Strictly controlled by cfg::esp::bot_tag switch.
    // Displays [BOT] tag above enemy head to distinguish server bots.
    inline void drawBotTag(ImDrawList* dl, const PlayerData& p, float cx, float top_y) {
        if (!cfg::esp::bot_tag || !p.isBot) return;

        const char* tag = "[BOT]";
        const float fs = ESP_FONT_SIZE + 1.f;
        ImFont* font = ImGui::GetFont();
        ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, tag);
        float tx = cx - ts.x * 0.5f;
        float ty = top_y - ts.y - 6.f;

        // Shadow outline for readability on any background
        dl->AddText(font, fs, {tx + 1.f, ty + 1.f}, IM_COL32(0, 0, 0, 240), tag);
        dl->AddText(font, fs, {tx - 1.f, ty}, IM_COL32(0, 0, 0, 180), tag);
        dl->AddText(font, fs, {tx + 1.f, ty}, IM_COL32(0, 0, 0, 180), tag);
        dl->AddText(font, fs, {tx, ty - 1.f}, IM_COL32(0, 0, 0, 180), tag);
        dl->AddText(font, fs, {tx, ty + 1.f}, IM_COL32(0, 0, 0, 180), tag);

        // Distinct red/amber bot tag color
        dl->AddText(font, fs, {tx, ty}, IM_COL32(255, 80, 80, 255), tag);
    }

} // namespace func::esp
