#include "theme.hpp"
#include <cmath>

float g_ThemeColorArr[4] = { 0.882f, 0.000f, 0.596f, 1.0f };

void DrawVectorIcon(ImDrawList* dl, EIconType type, ImVec2 center, float size, ImU32 col) {
    float hs = size * 0.5f;
    float cx = center.x;
    float cy = center.y;

    switch (type) {
        case ICON_EYE: {
            dl->AddBezierQuadratic(ImVec2(cx - hs, cy), ImVec2(cx, cy - hs * 0.85f), ImVec2(cx + hs, cy), col, 2.0f);
            dl->AddBezierQuadratic(ImVec2(cx - hs, cy), ImVec2(cx, cy + hs * 0.85f), ImVec2(cx + hs, cy), col, 2.0f);
            dl->AddCircleFilled(center, size * 0.22f, col);
            dl->AddCircleFilled(center, size * 0.08f, IM_COL32(14, 12, 20, 255));
            break;
        }
        case ICON_CROSSHAIR: {
            dl->AddCircle(center, size * 0.38f, col, 16, 1.8f);
            dl->AddCircleFilled(center, 1.8f, col);
            dl->AddLine(ImVec2(cx, cy - hs), ImVec2(cx, cy - size * 0.22f), col, 1.8f);
            dl->AddLine(ImVec2(cx, cy + size * 0.22f), ImVec2(cx, cy + hs), col, 1.8f);
            dl->AddLine(ImVec2(cx - hs, cy), ImVec2(cx - size * 0.22f, cy), col, 1.8f);
            dl->AddLine(ImVec2(cx + size * 0.22f, cy), ImVec2(cx + hs, cy), col, 1.8f);
            break;
        }
        case ICON_SETTINGS: {
            dl->AddCircle(center, size * 0.32f, col, 20, 2.2f);
            dl->AddCircleFilled(center, size * 0.12f, col);
            for (int i = 0; i < 6; i++) {
                float angle = i * (3.14159265f / 3.0f);
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);
                ImVec2 p1(cx + cosA * (size * 0.30f), cy + sinA * (size * 0.30f));
                ImVec2 p2(cx + cosA * (size * 0.48f), cy + sinA * (size * 0.48f));
                dl->AddLine(p1, p2, col, 2.5f);
            }
            break;
        }
        case ICON_POWER: {
            dl->AddCircle(center, size * 0.34f, col, 24, 2.0f);
            dl->AddLine(ImVec2(cx, cy - size * 0.40f), ImVec2(cx, cy - size * 0.05f), col, 2.2f);
            break;
        }
    }
}
