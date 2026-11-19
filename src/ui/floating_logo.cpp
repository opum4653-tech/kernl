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

// Cached viewport, refreshed every render(). Used by save_pos() so the position
// can be normalized even when called from the shutdown path.
static float s_last_view_w = 0.0f;
static float s_last_view_h = 0.0f;
// Geometry of the last rendered frame, cached so save_pos() can clamp against
// the same bounds render() uses instead of against the raw viewport.
static float s_last_radius = 0.0f;
static float s_last_margin = 0.0f;

// ----------------------------------------------------------------------------
// POSITION PERSISTENCE
// /dev is a tmpfs node, so this never touches flash and never leaves a trace
// after reboot. Writes happen on drag release only, not per frame.
// ----------------------------------------------------------------------------
static bool pos_is_sane(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    // (0,0) is a legal placement, so only reject genuinely out-of-range values.
    if (x < 0.0f || y < 0.0f) return false;
    return true;
}

void save_pos() {
    if (!pos_is_sane(s_logo_pos.x, s_logo_pos.y)) return;

    // Clamp against the same bounds render() enforces.
    // 100% Volatile RAM-only persistence: zero disk I/O, zero file footprint.
    float x = s_logo_pos.x;
    float y = s_logo_pos.y;
    if (s_last_view_w > 0.0f && s_last_radius > 0.0f) {
        const float diameter = s_last_radius * 2.0f;
        const float maxX = std::max(s_last_margin, s_last_view_w - diameter - s_last_margin);
        const float maxY = std::max(s_last_margin, s_last_view_h - diameter - s_last_margin);
        x = std::clamp(x, s_last_margin, maxX);
        y = std::clamp(y, s_last_margin, maxY);
    } else {
        if (s_last_view_w > 0.0f) x = std::min(x, s_last_view_w);
        if (s_last_view_h > 0.0f) y = std::min(y, s_last_view_h);
    }

    s_logo_pos = ImVec2(x, y);
}

void load_pos() {
    // Volatile RAM initialization: strictly zero disk I/O.
    // Logo position is maintained in memory during live process lifecycle.
}

// ----------------------------------------------------------------------------
// CIRCULAR FLOATING LOGO DRAWING ('K' for Kernel Service)
// ----------------------------------------------------------------------------
static void DrawCircularLogo(ImDrawList* dl, ImVec2 center, float radius) {
    ImU32 colPrimary = theme_orchid::ColPrimary;
    ImU32 colDarkBg  = IM_COL32(18, 14, 26, 245);
    ImU32 colWhite   = IM_COL32(255, 255, 255, 255);

    // 0. Outer Halo Glow
    dl->AddCircle(center, radius + 2.0f, IM_COL32(230, 80, 180, 70), 64, 2.5f);

    // 1. Base Dark Disc
    dl->AddCircleFilled(center, radius, colDarkBg, 64);

    // 2. Clean Solid Outline Border in Active Theme Accent
    dl->AddCircle(center, radius, colPrimary, 64, 2.5f);

    // 3. Center "K" Letter with subtle drop shadow
    ImFont* font = g_LogoFont ? g_LogoFont : ImGui::GetFont();
    const char* txt = "K";
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
    s_last_view_w = realW;
    s_last_view_h = realH;

    const float logoRadius = 64.0f * ui_scale;
    const float logoMargin = 16.0f * ui_scale;
    const float logoDiameter = logoRadius * 2.0f;

    s_last_radius = logoRadius;
    s_last_margin = logoMargin;

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

    // Register the logo's touch zone as a circle matching the enlarged disc
    Touch_AddUiCircle(s_logo_pos.x + logoRadius, s_logo_pos.y + logoRadius, logoRadius + 10.0f * ui_scale);

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

    if (ImGui::Begin("##KernelServiceIndicator", nullptr, logoFlags)) {
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
            const float hitR = logoRadius + 10.0f * ui_scale;
            float ddx = mousePos.x - center.x;
            float ddy = mousePos.y - center.y;
            s_was_activated = (ddx * ddx + ddy * ddy) <= (hitR * hitR);
            s_click_down_pos = mousePos;
            s_moved_drag = false;
        }

        if (active && s_was_activated) {
            float dx = mousePos.x - s_click_down_pos.x;
            float dy = mousePos.y - s_click_down_pos.y;
            // High-tolerance drag slop: prevents finger micro-jitter from swallowing single taps
            const float dragSlop = 22.0f * ui_scale;
            if (s_moved_drag || std::hypot(dx, dy) > dragSlop) {
                s_moved_drag = true;
                s_logo_pos = ImVec2(
                    std::clamp(mousePos.x - logoRadius, logoMargin, maxX),
                    std::clamp(mousePos.y - logoRadius, logoMargin, maxY)
                );
            }
        }

        // Tap reliably opens the menu (immediate responsive trigger)
        if (s_was_activated && deactivated && !s_moved_drag) {
            if (s_last_toggle_time < 0.0 || (cur_time - s_last_toggle_time > 0.15)) {
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
        // 0.68 on both axes is a 45-degree offset, so the true distance from
        // the logo centre is 0.68 * radius * sqrt(2) = 0.96 * radius. The 6px
        // halo then pushed the dot past the disc edge. 0.56 lands it at ~0.79r
        // with room for the halo inside the border.
        ImVec2 stDot(center.x + logoRadius * 0.56f, center.y - logoRadius * 0.56f);
        dl->AddCircleFilled(stDot, 6.0f * ui_scale, (stCol & 0x00FFFFFF) | (90u << 24));
        dl->AddCircleFilled(stDot, 4.0f * ui_scale, (stCol & 0x00FFFFFF) | (180u << 24));
        dl->AddCircleFilled(stDot, 2.5f * ui_scale, stCol);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace ui::floating_logo
