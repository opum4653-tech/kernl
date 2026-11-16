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

// Cached viewport, refreshed every render(). Used by save_pos() so the position
// can be validated even when called from the shutdown path.
static float s_last_view_w = 0.0f;
static float s_last_view_h = 0.0f;

// ----------------------------------------------------------------------------
// POSITION PERSISTENCE
// /dev is a tmpfs node, so this never touches flash and never leaves a trace
// after reboot. Writes happen on drag release only, not per frame.
// ----------------------------------------------------------------------------
static bool pos_is_sane(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    // (0,0) is a legal placement, so only reject genuinely out-of-range values.
    if (x < 0.0f || y < 0.0f) return false;
    if (s_last_view_w > 0.0f && x > s_last_view_w) return false;
    if (s_last_view_h > 0.0f && y > s_last_view_h) return false;
    return true;
}

void save_pos() {
    if (!pos_is_sane(s_logo_pos.x, s_logo_pos.y)) return;
    FILE* fp = fopen("/dev/.nin_logo_pos.txt", "w");
    if (!fp) return;
    fprintf(fp, "%.1f %.1f\n", s_logo_pos.x, s_logo_pos.y);
    fclose(fp);
}

void load_pos() {
    FILE* fp = fopen("/dev/.nin_logo_pos.txt", "r");
    if (fp) {
        float x = 0.0f, y = 0.0f;
        if (fscanf(fp, "%f %f", &x, &y) == 2) {
            if (std::isfinite(x) && std::isfinite(y) && x >= 0.0f && y >= 0.0f) {
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
static void DrawCircularLogo(ImDrawList* dl, ImVec2 center, float radius) {
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

    s_last_view_w = realW;
    s_last_view_h = realH;

    const float logoRadius = 38.0f * ui_scale;
    const float logoMargin = 16.0f * ui_scale;
    const float logoDiameter = logoRadius * 2.0f;

    // A repeated position is supplied on every launch, so no orientation
    // bookkeeping is needed here — the clamp below reels the logo back into
    // view on rotation instead of throwing the user's placement away.
    if (s_logo_pos.x < 0.0f || s_logo_pos.y < 0.0f) {
        s_logo_pos = ImVec2(logoMargin + 40.0f * ui_scale, realH * 0.45f);
    }

    const float maxX = std::max(logoMargin, realW - logoDiameter - logoMargin);
    const float maxY = std::max(logoMargin, realH - logoDiameter - logoMargin);
    s_logo_pos.x = std::clamp(s_logo_pos.x, logoMargin, maxX);
    s_logo_pos.y = std::clamp(s_logo_pos.y, logoMargin, maxY);

    // Register the logo's touch zone with the multi-rect API. Appending rather
    // than replacing means the menu rect (registered later in the same frame)
    // coexists with this one instead of overwriting it.
    Touch_AddUiRect(s_logo_pos.x - 8.0f, s_logo_pos.y - 8.0f,
                    logoDiameter + 16.0f, logoDiameter + 16.0f);

    ImGui::SetNextWindowPos(s_logo_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(logoDiameter, logoDiameter), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags logoFlags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("##NinmodsKernelPill", nullptr, logoFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winP = ImGui::GetWindowPos();
        ImVec2 center(winP.x + logoRadius, winP.y + logoRadius);

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##LogoTouchZone", ImVec2(logoDiameter, logoDiameter));

        static ImVec2 s_click_down_pos(0, 0);
        static bool s_moved_drag = false;
        static bool s_was_activated = false;
        static double s_last_toggle_time = -1.0;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;
        bool active = ImGui::IsItemActive();
        bool deactivated = ImGui::IsItemDeactivated();
        double cur_time = ImGui::GetTime();

        if (ImGui::IsItemActivated()) {
            s_was_activated = true;
            s_click_down_pos = mousePos;
            s_moved_drag = false;
        }

        if (active) {
            float dx = mousePos.x - s_click_down_pos.x;
            float dy = mousePos.y - s_click_down_pos.y;
            if (std::hypot(dx, dy) > 8.0f) {
                s_moved_drag = true;
                s_logo_pos = ImVec2(
                    std::clamp(mousePos.x - logoRadius, logoMargin, maxX),
                    std::clamp(mousePos.y - logoRadius, logoMargin, maxY)
                );
            }
        }

        // Tap opens the menu. The logo is not drawn while the menu is open, so
        // the toggle below only ever fires from the closed state — closing goes
        // through the X button or the EXIT control.
        if (s_was_activated && deactivated && !s_moved_drag) {
            if (s_last_toggle_time < 0.0 || (cur_time - s_last_toggle_time > 0.25)) {
                g_ShowMenu = true;
                s_last_toggle_time = cur_time;
            }
        }

        if (deactivated) {
            // Persist only on release — one write per drag, not one per frame.
            if (s_moved_drag) {
                save_pos();
            }
            s_moved_drag = false;
            s_was_activated = false;
        }

        DrawCircularLogo(dl, center, logoRadius);

        // Target / Game connection status dot
        bool connected = (proc::pid.load(std::memory_order_relaxed) > 0 && proc::lib.load(std::memory_order_relaxed) > 0);
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
