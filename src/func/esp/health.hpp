#pragma once
#include "common.hpp"

namespace func::esp {

    // VERTICAL ARMOR BAR (RIGHT SIDE OF 2D BOX)
    // Strictly controlled by cfg::esp::armor switch.
    // If disabled from the menu or player has no armor, zero bar drawing occurs.
    inline void drawSideArmorBar(ImDrawList* dl, const PlayerData& p, ImVec2 tl, ImVec2 br) {
        if (!cfg::esp::armor || p.curArmor <= 0) return;

        float boxH = br.y - tl.y;
        if (boxH <= 4.f) return;

        // Positioned on the RIGHT side of the bounding box
        float barX = br.x + 5.f;
        float barW = 3.5f;

        float arm01 = ImClamp((float)p.curArmor / 100.f, 0.f, 1.f);
        float fillArm = boxH * arm01;

        // Dark background track
        dl->AddRectFilled({barX - 1.f, tl.y - 1.f}, {barX + barW + 1.f, br.y + 1.f},
                          IM_COL32(10, 14, 20, 220), 1.f);
        dl->AddRect({barX - 1.f, tl.y - 1.f}, {barX + barW + 1.f, br.y + 1.f},
                    IM_COL32(0, 0, 0, 180), 1.f, 0, 1.f);

        // Armor fill (bottom up with bright cyan/blue theme)
        dl->AddRectFilled({barX, br.y - fillArm}, {barX + barW, br.y},
                          armorColor(arm01), 1.0f);
    }

} // namespace func::esp
