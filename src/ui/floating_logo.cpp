#include "floating_logo.hpp"
#include "theme.hpp"
#include "menu.hpp"
#include "imgui.h"
#include "Android_touch/TouchHelperA.h"
#include "../other/memory.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

namespace ui::floating_logo {

static ImVec2 s_logo_pos = ImVec2(-1.0f, -1.0f);
static bool s_has_loaded_pos = false;

// ----------------------------------------------------------------------------
// ZERO-TRACE POS PERSISTENCE (/dev/.nin_logo_pos.txt - Pure RAM node)
// ----------------------------------------------------------------------------
void save_pos() {
    if (s_logo_pos.x <= 0.0f || s_logo_pos.y <= 0.0f) return;
    FILE* fp = fopen("/dev/.nin_logo_pos.txt", "w");
    if (fp) {
        fprintf(fp, "%.1f %.1f\n", s_logo_pos.x, s_logo_pos.y);
        fclose(fp);
    }
}

void load_pos() {
    FILE* fp = fopen("/dev/.nin_logo_pos.txt", "r");
    if (fp) {
        float x = 0.0f, y = 0.0f;
        if (fscanf(fp, "%f %f", &x, &y) == 2) {
            if (x > 0.0f && y > 0.0f) {
                s_logo_pos = ImVec2(x, y);
            }
        }
        fclose(fp);
    }
    s_has_loaded_pos = true;
}

ImVec2 get_pos() {
    return s_logo_pos;
}

void set_pos(ImVec2 pos) {
    s_logo_pos = pos;
}

// ----------------------------------------------------------------------------
// CIRCULAR FLOATING LOGO DRAWING ('N')
// ----------------------------------------------------------------------------
static void DrawCircularLogo(ImDrawList* dl, ImVec2 center, float radius, bool dragging) {
    (void)dragging;
    ImU32 colPrimary = theme_orchid::ColPrimary;
    ImU32 colDarkBg  = IM_COL32(18, 14, 26, 245);
    ImU32 colWhite   = IM_COL32(255, 255, 255, 255);

    // 1. Base Dark Disc
    dl->AddCircleFilled(center, radius, colDarkBg, 64);

    // 2. Clean Solid Outline Border in Active Theme Accent
    dl->AddCircle(center, radius, colPrimary, 64, 2.0f);

    // 3. Center "N" Letter with subtle drop shadow
    ImFont* font = ImGui::GetFont();
    const char* txt = "N";
    float fontSize = radius * 1.15f;
    ImVec2 tsz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, txt);
    ImVec2 txtPos = ImVec2(center.x - tsz.x * 0.5f, center.y - tsz.y * 0.5f);

    dl->AddText(font, fontSize, ImVec2(txtPos.x + 1.2f, txtPos.y + 1.2f), IM_COL32(0, 0, 0, 180), txt);
    dl->AddText(font, fontSize, txtPos, colWhite, txt);
}

// ----------------------------------------------------------------------------
// FLOATING LOGO RENDER & INTERACTION LOGIC
// ----------------------------------------------------------------------------
void render(float realW, float realH, float ui_scale) {
    if (!s_has_loaded_pos) {
        load_pos();
    }

    const float logoRadius = 38.0f * ui_scale;
    const float logoMargin = 16.0f * ui_scale;

    static int last_orientation = -1;
    static float last_screen_w = 0.0f;
    static float last_screen_h = 0.0f;
    static bool pill_needs_reposition = false;

    int current_orientation = (realW > realH) ? 1 : 0;
    if (last_orientation != -1 && (current_orientation != last_orientation ||
        std::abs(realW - last_screen_w) > 50.0f || std::abs(realH - last_screen_h) > 50.0f)) {
        pill_needs_reposition = true;
    }
    last_orientation = current_orientation;
    last_screen_w = realW;
    last_screen_h = realH;

    if (s_logo_pos.x <= 0.0f || s_logo_pos.y <= 0.0f || pill_needs_reposition) {
        s_logo_pos = ImVec2(logoMargin + 40.0f * ui_scale, realH * 0.45f);
        pill_needs_reposition = false;
    }

    // Clamp inside viewport
    s_logo_pos.x = std::clamp(s_logo_pos.x, logoMargin, realW - logoRadius * 2.0f - logoMargin);
    s_logo_pos.y = std::clamp(s_logo_pos.y, logoMargin, realH - logoRadius * 2.0f - logoMargin);

    // Report touch zone to touch subsystem so touches do not pass through to game
    Touch_SetUiRect(s_logo_pos.x - 8.0f, s_logo_pos.y - 8.0f, logoRadius * 2.0f + 16.0f, logoRadius * 2.0f + 16.0f, true);

    ImGui::SetNextWindowPos(s_logo_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(logoRadius * 2.0f, logoRadius * 2.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags logoFlags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("##NinmodsKernelPill", nullptr, logoFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winP = ImGui::GetWindowPos();
        ImVec2 center(winP.x + logoRadius, winP.y + logoRadius);

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##LogoTouchZone", ImVec2(logoRadius * 2.0f, logoRadius * 2.0f));

        static ImVec2 s_click_down_pos(0, 0);
        static bool s_is_dragging = false;
        static bool s_moved_drag = false;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;
        bool clicked = ImGui::IsItemClicked();
        bool active = ImGui::IsItemActive();
        bool deactivated = ImGui::IsItemDeactivated();
        static bool s_was_activated = false;
        static double s_last_toggle_time = -1.0;
        double cur_time = ImGui::GetTime();

        if (clicked || ImGui::IsItemActivated()) {
            s_was_activated = true;
            s_click_down_pos = mousePos;
            s_moved_drag = false;
            s_is_dragging = false;
        }

        if (active) {
            float dist = std::hypot(mousePos.x - s_click_down_pos.x, mousePos.y - s_click_down_pos.y);
            if (dist > 8.0f) {
                s_is_dragging = true;
                s_moved_drag = true;
                s_logo_pos = ImVec2(
                    std::clamp(mousePos.x - logoRadius, logoMargin, realW - logoRadius * 2.0f - logoMargin),
                    std::clamp(mousePos.y - logoRadius, logoMargin, realH - logoRadius * 2.0f - logoMargin)
                );
                save_pos();
            }
        }

        // Smooth tap toggle only on finger release without drag
        if (s_was_activated && deactivated && !s_moved_drag) {
            if (s_last_toggle_time < 0.0 || (cur_time - s_last_toggle_time > 0.25)) {
                g_ShowMenu = true;
                s_last_toggle_time = cur_time;
            }
        }

        if (deactivated) {
            s_moved_drag = false;
            s_is_dragging = false;
            s_was_activated = false;
        }

        DrawCircularLogo(dl, center, logoRadius, s_is_dragging);

        // Target / Game connection status dot
        bool connected = (proc::pid > 0 && proc::lib > 0);
        ImU32 stCol = connected ? IM_COL32(0, 240, 140, 255) : IM_COL32(255, 90, 90, 255);
        ImVec2 stDot(center.x + logoRadius * 0.68f, center.y - logoRadius * 0.68f);
        dl->AddCircleFilled(stDot, 6.0f * ui_scale, (stCol & 0x00FFFFFF) | (90 << 24));
        dl->AddCircleFilled(stDot, 4.0f * ui_scale, (stCol & 0x00FFFFFF) | (180 << 24));
        dl->AddCircleFilled(stDot, 2.5f * ui_scale, stCol);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace ui::floating_logo
