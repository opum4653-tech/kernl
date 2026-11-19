#pragma once
#include "common.hpp"

// ==============================================================================================
// FILE: src/func/esp/health.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Vertical Armor Point (Vest/Helmet) gauge rendered on the right side of the 2D box.
//
// CROSS-FILE CALL GRAPH & CONTRACTS:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/func/esp/esp.hpp -> func::esp::draw():
//      * Calls drawSideArmorBar(dl, p, tl, br) when cfg::esp::armor is true and curArmor > 0.
//
// 2. DATA SOURCE:
//    - curArmor: Live armor protection points extracted in data.cpp (readHealthAndArmor)
//      from Il2Cpp m_PRIDataPool (0x70) Index 4 (HP_ArmorEntry = 0x40).
//
// 3. GEOMETRIC ALIGNMENT:
//    - Position: Exactly 5 pixels to the right of bounding box border (br.x + 5.f).
//    - Height: Matches 100% of bounding box vertical height (br.y - tl.y).
//    - Fill Direction: Vertical bottom-up fill (br.y to br.y - fillArm).
//    - Palette: High-visibility cyan/sky-blue armor theme via armorColor().
// ==============================================================================================

namespace func::esp {

    // Renders the vertical armor gauge adjacent to the enemy bounding box
    inline void drawSideArmorBar(ImDrawList* dl, const PlayerData& p, ImVec2 tl, ImVec2 br) {
        if (!cfg::esp::armor || p.curArmor <= 0) return;

        float boxH = br.y - tl.y;
        if (boxH <= 4.f) return;

        // Positioned on the RIGHT side of the bounding box
        float barX = br.x + 5.f;
        float barW = 3.5f;

        // Normalize armor fraction [0.0 .. 1.0] (capped at 100 baseline)
        float arm01 = ImClamp((float)p.curArmor / 100.f, 0.f, 1.f);
        float fillArm = boxH * arm01;

        // 1. Dark semi-transparent background track
        dl->AddRectFilled({barX - 1.f, tl.y - 1.f}, {barX + barW + 1.f, br.y + 1.f},
                          IM_COL32(10, 14, 20, 220), 1.f);
        // Track border outline
        dl->AddRect({barX - 1.f, tl.y - 1.f}, {barX + barW + 1.f, br.y + 1.f},
                    IM_COL32(0, 0, 0, 180), 1.f, 0, 1.f);

        // 2. Dynamic armor level fill (bottom-up fill with cyan/blue gradient)
        dl->AddRectFilled({barX, br.y - fillArm}, {barX + barW, br.y},
                          armorColor(arm01), 1.0f);
    }

} // namespace func::esp
