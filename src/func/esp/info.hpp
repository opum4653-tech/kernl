#pragma once
#include "common.hpp"

// ==============================================================================================
// FILE: src/func/esp/info.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Server Bot AI Tag ([BOT]) indicator rendered above enemy head boxes.
//
// CROSS-FILE CALL GRAPH & CONTRACTS:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/func/esp/esp.hpp -> func::esp::draw():
//      * Calls drawBotTag(dl, p, cx, tl.y) when cfg::esp::bot_tag is true and p.isBot is true.
//
// 2. DATA SOURCE:
//    - p.isBot: Live boolean flag extracted in data.cpp from Il2Cpp Player::IsBot (0x4A8).
//      Distinguishes artificial server-spawned bot entities from real human opponents.
//
// 3. VISUAL RENDERING:
//    - Text: "[BOT]" badge in bright warning red/coral (IM_COL32(255, 80, 80, 255)).
//    - 4-way cross shadow outline (top, bottom, left, right) ensuring 100% contrast readability
//      over bright skyboxes, desert terrain, or dark foliage.
// ==============================================================================================

namespace func::esp {

    // Renders the [BOT] AI badge horizontally centered above the enemy head box
    inline void drawBotTag(ImDrawList* dl, const PlayerData& p, float cx, float top_y) {
        if (!cfg::esp::bot_tag || !p.isBot) return;

        const char* tag = "[BOT]";
        const float fs = ESP_FONT_SIZE + 1.f;
        ImFont* font = ImGui::GetFont();

        // Calculate text bounds for perfect center alignment
        ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, tag);
        float tx = cx - ts.x * 0.5f;
        float ty = top_y - ts.y - 6.f; // 6 pixels above bounding box top edge

        // Multi-pass drop shadow outline for high contrast readability
        dl->AddText(font, fs, {tx + 1.f, ty + 1.f}, IM_COL32(0, 0, 0, 240), tag);
        dl->AddText(font, fs, {tx - 1.f, ty}, IM_COL32(0, 0, 0, 180), tag);
        dl->AddText(font, fs, {tx + 1.f, ty}, IM_COL32(0, 0, 0, 180), tag);
        dl->AddText(font, fs, {tx, ty - 1.f}, IM_COL32(0, 0, 0, 180), tag);
        dl->AddText(font, fs, {tx, ty + 1.f}, IM_COL32(0, 0, 0, 180), tag);

        // Primary bright coral-red text foreground
        dl->AddText(font, fs, {tx, ty}, IM_COL32(255, 80, 80, 255), tag);
    }

} // namespace func::esp
