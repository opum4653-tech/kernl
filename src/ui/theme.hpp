#pragma once
#include "imgui.h"
#include <algorithm>

extern float g_ThemeColorArr[4];
#define g_ThemeColor ImVec4(g_ThemeColorArr[0], g_ThemeColorArr[1], g_ThemeColorArr[2], g_ThemeColorArr[3])

namespace theme_orchid {
    inline ImU32 ColPrimary       = IM_COL32(225, 0, 152, 255);       // Electric Orchid #E10098
    inline ImU32 ColPrimaryLight  = IM_COL32(255, 90, 205, 255);      // Soft Orchid Glow #FF5ACD
    constexpr ImU32 ColHeaderBg      = IM_COL32(21, 16, 31, 255);        // Header #15101F
    constexpr ImU32 ColTextMuted     = IM_COL32(155, 140, 180, 255);     // Muted Text #9B8CB4
    constexpr ImU32 ColWhite         = IM_COL32(252, 248, 255, 255);     // Clean White
    constexpr ImU32 ColSwitchOnTrack   = IM_COL32(0, 230, 118, 255);       // Bright Neon Green #00E676
    constexpr ImU32 ColSwitchOffTrack  = IM_COL32(38, 30, 48, 255);        // Sleek Dark Track #261E30
    constexpr ImU32 ColSwitchThumb     = IM_COL32(252, 252, 252, 255);     // Crisp White #FCFCFC
    constexpr ImU32 ColTextWhite       = IM_COL32(245, 248, 252, 255);

    inline void sync_from_arr() {
        ColPrimary = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_ThemeColorArr[0], g_ThemeColorArr[1], g_ThemeColorArr[2], g_ThemeColorArr[3]
        ));
        ColPrimaryLight = ImGui::ColorConvertFloat4ToU32(ImVec4(
            std::min(1.0f, g_ThemeColorArr[0] * 1.15f + 0.1f),
            std::min(1.0f, g_ThemeColorArr[1] * 1.15f + 0.1f),
            std::min(1.0f, g_ThemeColorArr[2] * 1.15f + 0.1f),
            1.0f
        ));
    }
}

enum EIconType {
    ICON_EYE = 0,
    ICON_CROSSHAIR,
    ICON_SETTINGS,
    ICON_POWER
};

void DrawVectorIcon(ImDrawList* dl, EIconType type, ImVec2 center, float size, ImU32 col);
