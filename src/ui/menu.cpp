#include "menu.hpp"
#include "imgui.h"
#include "Android_draw/draw.h"
#include "Android_touch/TouchHelperA.h"
#include "driver.h"
#include "cfg.hpp"
#include "../other/memory.hpp"
#include "../func/esp.hpp"
#include "../func/aim/aim.hpp"
#include "../func/speed.hpp"
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <unistd.h>
#include <sys/stat.h>

// Forward-declare Dear ImGui's ClearActiveID to cancel active widgets during scroll
namespace ImGui {
    IMGUI_API void ClearActiveID();
}

extern bool main_thread_flag;
extern int native_window_screen_x;
extern int native_window_screen_y;
extern int 选择值;

// RULE: Starts with floating logo closed by default. Menu opens ONLY when user taps the floating logo.
bool g_ShowMenu = false;

static int activeTab = 0;
static float ui_opacity = 1.0f;
static std::atomic<bool> g_should_exit{false};

float g_ThemeColorArr[4] = { 0.882f, 0.000f, 0.596f, 1.0f };

// DESIGN: ELECTRIC ORCHID GLASS — UNIFIED MODERN PALETTE
namespace theme_orchid {
    constexpr ImU32 ColPrimary       = IM_COL32(225, 0, 152, 255);       // Electric Orchid #E10098
    constexpr ImU32 ColPrimaryLight  = IM_COL32(255, 90, 205, 255);      // Soft Orchid Glow #FF5ACD
    constexpr ImU32 ColHeaderBg      = IM_COL32(21, 16, 31, 255);        // Header #15101F
    constexpr ImU32 ColTextMuted     = IM_COL32(155, 140, 180, 255);     // Muted Text #9B8CB4
    constexpr ImU32 ColWhite         = IM_COL32(252, 248, 255, 255);     // Clean White
    constexpr ImU32 ColSwitchOnTrack   = IM_COL32(38, 204, 116, 255);      // Emerald Green #26CC74
    constexpr ImU32 ColSwitchOffTrack  = IM_COL32(28, 24, 38, 240);        // Dark Slate #1C1826
    constexpr ImU32 ColSwitchThumb     = IM_COL32(252, 252, 252, 255);     // Crisp White #FCFCFC
    constexpr ImU32 ColTextWhite       = IM_COL32(245, 248, 252, 255);
}

enum EIconType {
    ICON_EYE = 0,
    ICON_CROSSHAIR,
    ICON_SETTINGS,
    ICON_POWER
};

static const ImVec4 g_ThemeSwatches[6] = {
    ImVec4(0.882f, 0.000f, 0.596f, 1.00f), // Orchid
    ImVec4(0.00f, 0.85f, 0.45f, 1.00f),    // Emerald
    ImVec4(0.00f, 0.80f, 1.00f, 1.00f),    // Cyan
    ImVec4(0.95f, 0.30f, 0.30f, 1.00f),    // Crimson
    ImVec4(1.00f, 0.75f, 0.00f, 1.00f),    // Amber
    ImVec4(0.60f, 0.40f, 1.00f, 1.00f),    // Violet
};

// CIRCULAR FLOATING LOGO ('N')
static void DrawCircularLogo(ImDrawList* dl, ImVec2 center, float radius, bool spinning = false) {
    (void)spinning;
    ImU32 colPrimary = theme_orchid::ColPrimary;
    ImU32 colDarkBg  = IM_COL32(18, 14, 26, 245);
    ImU32 colWhite   = IM_COL32(255, 255, 255, 255);

    // 1. Base Dark Disc
    dl->AddCircleFilled(center, radius, colDarkBg, 64);

    // 2. Clean Solid Outline Border (Simple, No 4-sided animations)
    dl->AddCircle(center, radius, colPrimary, 64, 2.0f);

    // 3. Center "N" Letter with drop shadow
    ImFont* font = ImGui::GetFont();
    const char* txt = "N";
    float fontSize = radius * 1.15f;
    ImVec2 tsz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, txt);
    ImVec2 txtPos = ImVec2(center.x - tsz.x * 0.5f, center.y - tsz.y * 0.5f);

    dl->AddText(font, fontSize, ImVec2(txtPos.x + 1.2f, txtPos.y + 1.2f), IM_COL32(0, 0, 0, 180), txt);
    dl->AddText(font, fontSize, txtPos, colWhite, txt);
}

static void DrawVectorIcon(ImDrawList* dl, EIconType type, ImVec2 center, float size, ImU32 col) {
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
            dl->AddCircle(center, size * 0.30f, col, 16, 1.8f);
            dl->AddCircleFilled(center, size * 0.12f, col);
            for (int i = 0; i < 8; ++i) {
                float angle = i * (3.14159265f / 4.0f);
                float x1 = cx + cosf(angle) * (hs * 0.65f);
                float y1 = cy + sinf(angle) * (hs * 0.65f);
                float x2 = cx + cosf(angle) * hs;
                float y2 = cy + sinf(angle) * hs;
                dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, 2.0f);
            }
            break;
        }
        case ICON_POWER: {
            float r = size * 0.38f;
            dl->PathClear();
            dl->PathArcTo(center, r, -2.10f, 5.24f, 20);
            dl->PathStroke(col, 0, 2.0f);
            dl->AddLine(ImVec2(cx, cy - hs), ImVec2(cx, cy), col, 2.0f);
            break;
        }
    }
}

static bool DrawSidebarIconButton(const char* str_id, EIconType icon_type, bool is_active, ImVec2 size) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(str_id, size);
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 bMin = p;
    ImVec2 bMax = ImVec2(p.x + size.x, p.y + size.y);
    ImVec2 center = ImVec2((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f);

    if (is_active) {
        dl->AddRectFilled(bMin, bMax, IM_COL32(225, 0, 152, 48), 10.0f);
        dl->AddRect(bMin, bMax, IM_COL32(225, 0, 152, 140), 10.0f, 0, 1.2f);
        dl->AddRectFilled(ImVec2(bMin.x + 3.0f, bMin.y + 8.0f), ImVec2(bMin.x + 6.5f, bMax.y - 8.0f), theme_orchid::ColPrimary, 2.0f);
        DrawVectorIcon(dl, icon_type, center, 25.0f, theme_orchid::ColWhite);
    } else {
        if (hovered) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 18), 10.0f);
        }
        ImU32 ic_col = hovered ? theme_orchid::ColPrimaryLight : theme_orchid::ColTextMuted;
        DrawVectorIcon(dl, icon_type, center, 23.0f, ic_col);
    }

    return pressed;
}

static void handleTouchScroll() {
    ImGuiIO& io = ImGui::GetIO();
    enum GestureState { GESTURE_IDLE, GESTURE_HORIZONTAL, GESTURE_VERTICAL };
    static GestureState s_gesture = GESTURE_IDLE;
    static ImVec2 s_touch_start = ImVec2(0, 0);
    static ImGuiID s_active_child = 0;

    ImGuiID child_id = ImGui::GetID("##TouchScrollToken");

    if (io.MouseDown[0]) {
        if (s_gesture == GESTURE_IDLE) {
            if (s_active_child == 0 && ImGui::IsWindowHovered()) {
                s_touch_start = io.MousePos;
                s_active_child = child_id;
            }
        }

        if (s_active_child == child_id) {
            float dx = io.MousePos.x - s_touch_start.x;
            float dy = io.MousePos.y - s_touch_start.y;
            float abs_dx = std::abs(dx);
            float abs_dy = std::abs(dy);

            if (s_gesture == GESTURE_IDLE) {
                if (abs_dx >= 5.0f && abs_dx > abs_dy * 1.1f) {
                    s_gesture = GESTURE_HORIZONTAL;
                } else if (abs_dy >= 5.0f && abs_dy > abs_dx * 1.1f) {
                    s_gesture = GESTURE_VERTICAL;
                }
            }

            if (s_gesture == GESTURE_VERTICAL) {
                ImGui::ClearActiveID();
                float delta_y = io.MouseDelta.y;
                if (std::abs(delta_y) > 0.01f) {
                    ImGui::SetScrollY(ImGui::GetScrollY() - delta_y);
                }
            }
        }
    } else {
        s_gesture = GESTURE_IDLE;
        s_active_child = 0;
    }
}

static ImVec2 s_logo_pos = ImVec2(-1.0f, -1.0f);

// ----------------------------------------------------------------------------
// CUSTOM WIDGET: PILL TOGGLE SWITCH (EXACT REPLICA OF CRACK_MASTER)
// ----------------------------------------------------------------------------
static bool DrawToggleSwitch(const char* label, bool* v, float row_height = 46.0f, float ui_scale = 1.0f) {
    ImVec2 curPos = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;

    char btn_id[128];
    snprintf(btn_id, sizeof(btn_id), "##SwitchBtn_%s", label);

    bool pressed = ImGui::InvisibleButton(btn_id, ImVec2(availW, row_height));
    if (pressed) {
        *v = !(*v);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. Label on the left, vertically centered
    ImFont* font = ImGui::GetFont();
    float fontSize = 17.0f * ui_scale;
    ImVec2 tsz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    ImVec2 label_pos(curPos.x + 8.0f * ui_scale, curPos.y + (row_height - tsz.y) * 0.5f);
    dl->AddText(font, fontSize, label_pos, theme_orchid::ColTextWhite, label);

    // 2. Capsule / Pill switch on the right
    float switchW = 58.0f * ui_scale;
    float switchH = 30.0f * ui_scale;
    float switchRounding = switchH * 0.5f;
    float switchPadRight = 8.0f * ui_scale;

    ImVec2 sMin(curPos.x + availW - switchW - switchPadRight, curPos.y + (row_height - switchH) * 0.5f);
    ImVec2 sMax(sMin.x + switchW, sMin.y + switchH);

    // Track color (Solid Green if ON, Dark slate if OFF)
    ImU32 trackCol = *v ? theme_orchid::ColSwitchOnTrack : theme_orchid::ColSwitchOffTrack;
    dl->AddRectFilled(sMin, sMax, trackCol, switchRounding);

    // Subtle dark track border when OFF
    if (!(*v)) {
        dl->AddRect(sMin, sMax, IM_COL32(60, 50, 75, 220), switchRounding, 0, 1.2f);
    }

    // Circular white knob / thumb
    float knobRadius = switchRounding - 2.8f * ui_scale;
    float knobX = *v ? (sMax.x - switchRounding) : (sMin.x + switchRounding);
    float knobY = sMin.y + switchRounding;

    dl->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, theme_orchid::ColSwitchThumb, 32);

    return pressed;
}

// ----------------------------------------------------------------------------
// CUSTOM WIDGET: SEGMENTED BUTTON ROW (ZERO DROPDOWN / COMBO REPLACEMENT)
// ----------------------------------------------------------------------------
static bool DrawSegmentedButtons(const char* str_id, int* selected, const char* const labels[], int count, float height = 30.0f) {
    float availW = ImGui::GetContentRegionAvail().x;
    float gap = 4.0f;
    float btnW = (availW - gap * (count - 1)) / (float)count;
    bool changed = false;

    for (int i = 0; i < count; i++) {
        if (i > 0) ImGui::SameLine(0, gap);

        char btnLabel[64];
        snprintf(btnLabel, sizeof(btnLabel), "%s##%s_%d", labels[i], str_id, i);

        bool is_active = (*selected == i);
        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme_orchid::ColPrimary);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme_orchid::ColPrimaryLight);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme_orchid::ColPrimary);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.09f, 0.17f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.14f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.18f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, theme_orchid::ColTextMuted);
        }

        if (ImGui::Button(btnLabel, ImVec2(btnW, height))) {
            *selected = i;
            changed = true;
        }

        ImGui::PopStyleColor(4);
    }
    return changed;
}

namespace ui::menu {

void save_logo_pos() {
    // 0% storage footprint - keep position in RAM only
}

void load_logo_pos() {
    // 0% storage footprint - keep position in RAM only
}

void render() {
    // Render pure kernel driver overlays (ESP & Aim assist FOV)
    func::esp::draw();
    func::aim::update();
    func::aim::render_fov(ImGui::GetBackgroundDrawList());

    if (g_should_exit.load()) {
        main_thread_flag = false;
        g_ShowMenu = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    float realW = (io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : (float)native_window_screen_x;
    float realH = (io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : (float)native_window_screen_y;
    if (realW <= 0) realW = 2400.0f;
    if (realH <= 0) realH = 1080.0f;

    static int last_orientation = -1;
    static float last_screen_w = 0.0f;
    static float last_screen_h = 0.0f;
    static bool menu_needs_reposition = false;
    static bool pill_needs_reposition = false;

    int current_orientation = (realW > realH) ? 1 : 0;
    if (last_orientation != -1 && (current_orientation != last_orientation || std::abs(realW - last_screen_w) > 50.0f || std::abs(realH - last_screen_h) > 50.0f)) {
        menu_needs_reposition = true;
        pill_needs_reposition = true;
    }
    last_orientation = current_orientation;
    last_screen_w = realW;
    last_screen_h = realH;

    float ui_scale = realH / 1080.0f;
    if (ui_scale < 0.65f) ui_scale = 0.65f;
    if (ui_scale > 1.45f) ui_scale = 1.45f;

    // --------------------------------------------------------------------
    // STATE 1: FLOATING CIRCULAR 'N' LOGO (Rendered ONLY when menu is closed)
    // --------------------------------------------------------------------
    const float logoRadius = 28.0f * ui_scale;
    const float logoMargin = 16.0f;

    if (s_logo_pos.x <= 0.0f || s_logo_pos.y <= 0.0f || pill_needs_reposition) {
        s_logo_pos = ImVec2(logoMargin + 40.0f, realH * 0.45f);
        pill_needs_reposition = false;
    }

    s_logo_pos.x = std::max(logoMargin, std::min(s_logo_pos.x, realW - logoRadius * 2.0f - logoMargin));
    s_logo_pos.y = std::max(logoMargin, std::min(s_logo_pos.y, realH - logoRadius * 2.0f - logoMargin));

    if (!g_ShowMenu) {
        // Report UI rect with padding to Touch subsystem so touches on ball do not penetrate to game
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

            ImVec2 mousePos = io.MousePos;
            bool clicked = ImGui::IsItemClicked();
            bool active = ImGui::IsItemActive();
            bool deactivated = ImGui::IsItemDeactivated();
            static bool s_was_activated = false;
            static double s_last_toggle_time = 0.0;
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
                    s_logo_pos = ImVec2(mousePos.x - logoRadius, mousePos.y - logoRadius);
                    save_logo_pos();
                }
            }

            // Smooth tap toggle without artificial startup delay
            if (s_was_activated && (clicked || (deactivated && !s_moved_drag))) {
                if (cur_time - s_last_toggle_time > 0.25) {
                    g_ShowMenu = true;
                    s_last_toggle_time = cur_time;
                }
                s_moved_drag = false;
                s_is_dragging = false;
                s_was_activated = false;
            } else if (deactivated) {
                s_moved_drag = false;
                s_is_dragging = false;
                s_was_activated = false;
            }

            DrawCircularLogo(dl, center, logoRadius, s_is_dragging);

            bool connected = (proc::pid > 0 && proc::lib > 0);
            ImU32 stCol = connected ? IM_COL32(0, 240, 140, 255) : IM_COL32(255, 90, 90, 255);
            ImVec2 stDot(center.x + logoRadius * 0.68f, center.y - logoRadius * 0.68f);
            dl->AddCircleFilled(stDot, 6.0f, (stCol & 0x00FFFFFF) | (90 << 24));
            dl->AddCircleFilled(stDot, 4.0f, (stCol & 0x00FFFFFF) | (180 << 24));
            dl->AddCircleFilled(stDot, 2.5f, stCol);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        // Menu is closed, done for this frame
        return;
    }

    // --------------------------------------------------------------------
    // STATE 2: MAIN UNIFIED ELECTRIC ORCHID GLASS MENU (When menu is open)
    // --------------------------------------------------------------------
    const float winW = std::max(500.0f * ui_scale, std::min(860.0f * ui_scale, realW * 0.92f));
    const float winH = std::max(360.0f * ui_scale, std::min(500.0f * ui_scale, realH * 0.92f));

    static ImVec2 s_menu_pos = ImVec2(-1.0f, -1.0f);
    if (s_menu_pos.x < 0.0f || menu_needs_reposition) {
        s_menu_pos = ImVec2((realW - winW) * 0.5f, (realH - winH) * 0.5f);
        menu_needs_reposition = false;
    }

    s_menu_pos.x = std::max(0.0f, std::min(s_menu_pos.x, realW - winW));
    s_menu_pos.y = std::max(0.0f, std::min(s_menu_pos.y, realH - winH));

    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGui::SetNextWindowPos(s_menu_pos, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.047f, 0.078f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.882f, 0.000f, 0.596f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.110f, 0.086f, 0.157f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.160f, 0.120f, 0.220f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.220f, 0.160f, 0.300f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.882f, 0.000f, 0.596f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.882f, 0.000f, 0.596f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.000f, 0.353f, 0.804f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.110f, 0.086f, 0.157f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.200f, 0.140f, 0.280f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.882f, 0.000f, 0.596f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.882f, 0.000f, 0.596f, 0.40f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.882f, 0.000f, 0.596f, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.882f, 0.000f, 0.596f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.988f, 0.973f, 1.000f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.608f, 0.549f, 0.706f, 1.0f));

    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0, 0, 0, 0));

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ui_opacity);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##NinmodsKernelMainWindow", nullptr, winFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // Report full menu window rect to Touch subsystem so touches on menu do not penetrate to game
        Touch_SetUiRect(winPos.x, winPos.y, winSize.x, winSize.y, true);

        // 1. TOP HEADER (BRANDING + STATUS + CLOSE)
        const float headerH = 50.0f;
        ImVec2 hMin = winPos;
        ImVec2 hMax = ImVec2(winPos.x + winSize.x, winPos.y + headerH);

        dl->AddRectFilled(hMin, hMax, theme_orchid::ColHeaderBg, 12.0f, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(winPos.x, winPos.y + headerH), ImVec2(winPos.x + winSize.x, winPos.y + headerH), IM_COL32(40, 30, 56, 255), 1.0f);

        // Header Draggable Bar
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##KernelHeaderDrag", ImVec2(winSize.x - 70.0f, headerH));
        if (ImGui::IsItemActive()) {
            s_menu_pos.x += io.MouseDelta.x;
            s_menu_pos.y += io.MouseDelta.y;
            s_menu_pos.x = std::max(0.0f, std::min(s_menu_pos.x, realW - winW));
            s_menu_pos.y = std::max(0.0f, std::min(s_menu_pos.y, realH - winH));
        }

        // Title: NINMODS KERNEL
        ImVec2 brandPos = ImVec2(winPos.x + 18.0f, winPos.y + 14.0f);
        dl->AddText(ImGui::GetFont(), 19.0f, brandPos, theme_orchid::ColWhite, "NINMODS");
        dl->AddText(ImGui::GetFont(), 15.0f, ImVec2(brandPos.x + 92.0f, brandPos.y + 3.0f), theme_orchid::ColPrimaryLight, "KERNEL CORE");

        // Status pill
        bool connected = (proc::pid > 0 && proc::lib > 0);
        const char* badgeText = connected ? "ACTIVE" : "OFFLINE";
        ImVec2 badgeSz = ImGui::CalcTextSize(badgeText);
        float badgeW = badgeSz.x + 24.0f;
        float badgeH = 24.0f;
        ImVec2 bMin(winPos.x + winSize.x - 60.0f - badgeW - 10.0f, winPos.y + (headerH - badgeH) * 0.5f);
        ImVec2 bMax(bMin.x + badgeW, bMin.y + badgeH);

        ImU32 badgeCol = connected ? IM_COL32(0, 240, 140, 255) : IM_COL32(255, 90, 90, 255);
        dl->AddRectFilled(bMin, bMax, (badgeCol & 0x00FFFFFF) | (35 << 24), 12.0f);
        dl->AddRect(bMin, bMax, (badgeCol & 0x00FFFFFF) | (150 << 24), 12.0f, 0, 1.0f);
        dl->AddCircleFilled(ImVec2(bMin.x + 10.0f, bMin.y + badgeH * 0.5f), 3.5f, badgeCol);
        dl->AddText(ImVec2(bMin.x + 18.0f, bMin.y + (badgeH - badgeSz.y) * 0.5f), badgeCol, badgeText);

        // Close Button (X)
        const float closeBtnSize = 36.0f;
        ImVec2 closePos = ImVec2(winPos.x + winSize.x - closeBtnSize - 10.0f, winPos.y + (headerH - closeBtnSize) * 0.5f);
        ImGui::SetCursorPos(ImVec2(winSize.x - closeBtnSize - 10.0f, (headerH - closeBtnSize) * 0.5f));

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.882f, 0.000f, 0.596f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.882f, 0.000f, 0.596f, 0.65f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button("##CloseKernelBtn", ImVec2(closeBtnSize, closeBtnSize))) {
            g_ShowMenu = false;
            s_logo_pos = ImVec2(std::clamp(winPos.x, logoMargin, realW - logoRadius * 2.0f - logoMargin),
                                std::clamp(winPos.y, logoMargin, realH - logoRadius * 2.0f - logoMargin));
            save_logo_pos();
            Touch_SetUiRect(s_logo_pos.x, s_logo_pos.y, logoRadius * 2.0f, logoRadius * 2.0f, true);
        }
        bool closeHovered = ImGui::IsItemHovered();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImVec2 closeCenter = ImVec2(closePos.x + closeBtnSize * 0.5f, closePos.y + closeBtnSize * 0.5f);
        ImU32 closeCol = closeHovered ? theme_orchid::ColPrimaryLight : IM_COL32(200, 195, 215, 230);
        const float xRadius = 6.0f;
        dl->AddLine(ImVec2(closeCenter.x - xRadius, closeCenter.y - xRadius), ImVec2(closeCenter.x + xRadius, closeCenter.y + xRadius), closeCol, 2.2f);
        dl->AddLine(ImVec2(closeCenter.x - xRadius, closeCenter.y + xRadius), ImVec2(closeCenter.x + xRadius, closeCenter.y - xRadius), closeCol, 2.2f);

        // 2. LEFT SIDEBAR (ICON-ONLY)
        const float bodyY = headerH;
        const float bodyH = winSize.y - headerH;
        const float sidebarW = 76.0f;

        ImGui::SetCursorPos(ImVec2(0, bodyY));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.067f, 0.055f, 0.094f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::BeginChild("##SidebarKernel", ImVec2(sidebarW, bodyH), false, ImGuiWindowFlags_NoScrollbar)) {
            dl->AddLine(ImVec2(winPos.x + sidebarW, winPos.y + headerH), ImVec2(winPos.x + sidebarW, winPos.y + winSize.y), IM_COL32(40, 30, 56, 255), 1.0f);

            const ImVec2 btnSize = ImVec2(56.0f, 52.0f);
            const float btnX = (sidebarW - btnSize.x) * 0.5f;

            // Tab 0: Visuals (ICON_EYE)
            ImGui::SetCursorPos(ImVec2(btnX, 12.0f));
            if (DrawSidebarIconButton("##TabESP", ICON_EYE, (activeTab == 0), btnSize)) {
                activeTab = 0;
            }

            // Tab 1: Combat / Aim (ICON_CROSSHAIR)
            ImGui::SetCursorPos(ImVec2(btnX, 12.0f + btnSize.y + 8.0f));
            if (DrawSidebarIconButton("##TabCombat", ICON_CROSSHAIR, (activeTab == 1), btnSize)) {
                activeTab = 1;
            }

            // Tab 2: Settings / Misc (ICON_SETTINGS)
            ImGui::SetCursorPos(ImVec2(btnX, 12.0f + (btnSize.y + 8.0f) * 2.0f));
            if (DrawSidebarIconButton("##TabSettings", ICON_SETTINGS, (activeTab == 2), btnSize)) {
                activeTab = 2;
            }

            // Tab 3: Power / Exit
            ImGui::SetCursorPos(ImVec2(btnX, bodyH - btnSize.y - 12.0f));
            if (DrawSidebarIconButton("##TabPower", ICON_POWER, false, btnSize)) {
                g_should_exit = true;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // 3. MAIN CONTENT AREA (DUAL-COLUMN UNIFIED GLASS CARDS)
        const float contentX = sidebarW + 14.0f;
        const float contentY = headerH + 10.0f;
        const float contentW = winSize.x - contentX - 14.0f;
        const float contentH = bodyH - 20.0f;
        const float colGap   = 14.0f;
        const float colW     = (contentW - colGap) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.086f, 0.067f, 0.130f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.196f, 0.141f, 0.282f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));

        // TAB 0: ESP VISUALS
        if (activeTab == 0) {
            // Left Column: Features
            ImGui::SetCursorPos(ImVec2(contentX, contentY));
            if (ImGui::BeginChild("##ColLeftESP", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "ESP VISUALS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 95, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                if (DrawToggleSwitch("Master Switch", &cfg::master_enabled, 44.0f * ui_scale, ui_scale)) {
                    if (!cfg::master_enabled) {
                        func::aim::motion::reset();
                    }
                }
                ImGui::Dummy(ImVec2(0, 6));

                ImGui::Checkbox("ESP Box (Chaturbhuj)", &cfg::esp::box);
                ImGui::Checkbox("ESP Health Bar (Right)", &cfg::esp::health);
                ImGui::Checkbox("ESP Armor Bar", &cfg::esp::armor);
                ImGui::Checkbox("ESP Laser Line", &cfg::esp::tracer);
                ImGui::Checkbox("ESP Player Name", &cfg::esp::name);
                ImGui::Checkbox("ESP Distance", &cfg::esp::distance);
                ImGui::Checkbox("ESP Bot Tag (AI)", &cfg::esp::bot_tag);
                ImGui::Checkbox("ESP Weapon Name", &cfg::esp::weapon);
                ImGui::Checkbox("Firing Indicator", &cfg::esp::firing_ind);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Settings & Filters (Zero Dropdowns)
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightESP", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "SETTINGS & FILTERS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 120, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                ImGui::Checkbox("Ignore Bot Players", &cfg::esp::ignore_bots);
                ImGui::Checkbox("Crosshair Center", &cfg::esp::crosshair);
                ImGui::Checkbox("Loot ESP", &cfg::esp::loot);

                ImGui::Dummy(ImVec2(0, 4));
                static const char* const boxStyles[] = { "Corner", "Full", "Glow" };
                ImGui::TextDisabled("Box Style Mode");
                DrawSegmentedButtons("BoxStyle", &cfg::esp::box_type, boxStyles, 3, 30.0f * ui_scale);

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##MaxRange", &cfg::esp::max_range, 50.0f, 500.0f, "%.0f m");
                ImGui::TextDisabled("Max Render Distance");

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##LaserThickness", &cfg::esp::tracer_thickness, 0.5f, 4.0f, "%.1f px");
                ImGui::TextDisabled("Laser Line Width");

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##BoxThickness", &cfg::esp::box_thickness, 1.0f, 5.0f, "%.1f px");
                ImGui::TextDisabled("Box Border Width");

                ImGui::Dummy(ImVec2(0, 6));
                ImGui::TextDisabled("ESP Accent Color");
                for (int i = 0; i < 6; i++) {
                    ImGui::PushID(900 + i);
                    if (ImGui::ColorButton("##espcol", g_ThemeSwatches[i], 0, ImVec2(28 * ui_scale, 28 * ui_scale))) {
                        cfg::esp::box_col = g_ThemeSwatches[i];
                    }
                    if (i < 5) ImGui::SameLine();
                    ImGui::PopID();
                }

                handleTouchScroll();
            }
            ImGui::EndChild();
        }
        // TAB 1: AIMBOT & COMBAT
        else if (activeTab == 1) {
            // Left Column: Toggles
            ImGui::SetCursorPos(ImVec2(contentX, contentY));
            if (ImGui::BeginChild("##ColLeftAim", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "AIMBOT SETTINGS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 130, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                if (DrawToggleSwitch("Master Switch", &cfg::master_enabled, 44.0f * ui_scale, ui_scale)) {
                    if (!cfg::master_enabled) {
                        func::aim::motion::reset();
                    }
                }
                ImGui::Dummy(ImVec2(0, 6));

                ImGui::Checkbox("Enable Aim Assist", &cfg::aim::enable);
                ImGui::Checkbox("Aim Silent", &cfg::aim::silent);
                ImGui::Checkbox("Scope Aim (Instant Head)", &cfg::aim::scope_aim);
                ImGui::Checkbox("Sniper Headshot Lock", &cfg::aim::sniper_aim);

                ImGui::Dummy(ImVec2(0, 4));
                static const char* const boneLabels[] = { "Head", "Chest", "Hip" };
                ImGui::TextDisabled("Target Aim Bone");
                DrawSegmentedButtons("AimBone", &cfg::aim::target_bone, boneLabels, 3, 30.0f * ui_scale);

                ImGui::Dummy(ImVec2(0, 4));
                static const char* const triggerLabels[] = { "When Firing", "Always In FOV" };
                ImGui::TextDisabled("Aim Trigger Condition");
                DrawSegmentedButtons("AimTrigger", &cfg::aim::trigger_mode, triggerLabels, 2, 30.0f * ui_scale);

                ImGui::Dummy(ImVec2(0, 4));
                static const char* const priorityLabels[] = { "Crosshair", "Lowest HP", "Distance" };
                ImGui::TextDisabled("Target Priority");
                DrawSegmentedButtons("AimPriority", &cfg::aim::priority_mode, priorityLabels, 3, 30.0f * ui_scale);

                ImGui::Dummy(ImVec2(0, 6));
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "SAFETY FILTERS");
                lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 100, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                ImGui::Checkbox("Ignore Knocked", &cfg::aim::ignore_knocked);
                ImGui::Checkbox("Ignore AI Bots", &cfg::aim::ignore_bots);
                ImGui::Checkbox("Visible Check", &cfg::aim::visible_check);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Sliders & Modes
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightAim", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "FOV & SMOOTHNESS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 125, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                ImGui::Checkbox("Draw FOV Circle", &cfg::aim::show_fov);

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##AimFovSlider", &cfg::aim::aim_fov, 10.0f, 360.0f, "%.0f deg");
                ImGui::TextDisabled("Aim FOV Range (%.0f deg)", cfg::aim::aim_fov);

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##SilentRateSlider", &cfg::aim::headshot_rate, 0.0f, 100.0f, "%.0f%%");
                ImGui::TextDisabled("Silent Aim Hit Rate (%.0f%%)", cfg::aim::headshot_rate);

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##AimSmooth", &cfg::aim::smooth, 1.0f, 25.0f, "%.1f (Smooth)");
                ImGui::TextDisabled("Smooth Factor (Higher = Safe & Human)");

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##AimMaxDist", &cfg::aim::max_distance, 20.0f, 300.0f, "%.0f m");
                ImGui::TextDisabled("Max Aiming Range");

                ImGui::Dummy(ImVec2(0, 8));
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "LIVE TARGET HUD");
                rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 105, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                if (func::aim::g_currentTarget.valid) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Target: %s", func::aim::g_currentTarget.name);
                    ImGui::Text("Distance: %.1f m", func::aim::g_currentTarget.distance);
                    ImGui::Text("Health: %d HP", func::aim::g_currentTarget.hp);
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.6f, 1.0f), "Status: LOCKED [IN FOV]");
                } else {
                    ImGui::TextDisabled("Target: None (Scanning)");
                    ImGui::TextDisabled("Distance: -- m");
                    ImGui::TextDisabled("Health: --");
                    ImGui::TextDisabled("Status: STANDBY");
                }

                handleTouchScroll();
            }
            ImGui::EndChild();
        }
        // TAB 2: MISC & SYSTEM SETTINGS
        else if (activeTab == 2) {
            // Left Column: Game Mods
            ImGui::SetCursorPos(ImVec2(contentX, contentY));
            if (ImGui::BeginChild("##ColLeftMisc", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "GAMEPLAY MODS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 115, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                if (DrawToggleSwitch("Master Switch", &cfg::master_enabled, 44.0f * ui_scale, ui_scale)) {
                    if (!cfg::master_enabled) {
                        func::aim::motion::reset();
                    }
                }
                ImGui::Dummy(ImVec2(0, 6));

                ImGui::Checkbox("Speed Hack", &cfg::motion::speed_hack);
                if (cfg::motion::speed_hack) {
                    ImGui::SetNextItemWidth(-1);
                    ImGui::SliderFloat("##SpeedMult", &cfg::motion::speed_mult, 1.0f, 5.0f, "%.1fx");
                }

                ImGui::Checkbox("No Recoil", &cfg::motion::no_recoil);
                ImGui::Checkbox("No Reload", &cfg::motion::no_reload);
                ImGui::Checkbox("Ghost Hack", &cfg::motion::ghost_hack);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: System & Driver Status
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightMisc", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "SYSTEM & DRIVER");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 120, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                const char* dNames[] = { "NONE", "RT HOOK", "RT DEV", "DEV COMPAT", "QX 10-11", "KPM NEO" };
                const char* dName = (选择值 >= 1 && 选择值 <= 5) ? dNames[选择值] : "ARMED";

                char pidStr[32];
                snprintf(pidStr, sizeof(pidStr), "%d", proc::pid);

                char libStr[32];
                snprintf(libStr, sizeof(libStr), "0x%lx", (unsigned long)proc::lib);

                ImGui::TextDisabled("STATUS:");
                ImGui::SameLine(100);
                ImGui::TextColored(connected ? ImVec4(0, 1, 0.5f, 1) : ImVec4(1, 0.3f, 0.3f, 1), connected ? "READY" : "OFFLINE");

                ImGui::TextDisabled("PID:");
                ImGui::SameLine(100);
                ImGui::Text("%s", proc::pid > 0 ? pidStr : "NONE");

                ImGui::TextDisabled("LIB BASE:");
                ImGui::SameLine(100);
                ImGui::Text("%s", proc::lib > 0 ? libStr : "NONE");

                ImGui::TextDisabled("DRIVER:");
                ImGui::SameLine(100);
                ImGui::TextColored(ImVec4(0, 0.85f, 0.9f, 1), "%s", dName);

                ImGui::TextDisabled("FPS:");
                ImGui::SameLine(100);
                ImGui::Text("%.1f", io.Framerate);

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::SliderFloat("##Opacity", &ui_opacity, 0.2f, 1.0f, "Opacity: %.2f");

                ImGui::Dummy(ImVec2(0, 6));
                ImGui::TextDisabled("Theme Accent");
                for (int i = 0; i < 6; i++) {
                    ImGui::PushID(800 + i);
                    if (ImGui::ColorButton("##th", g_ThemeSwatches[i], 0, ImVec2(28 * ui_scale, 28 * ui_scale))) {
                        g_ThemeColorArr[0] = g_ThemeSwatches[i].x;
                        g_ThemeColorArr[1] = g_ThemeSwatches[i].y;
                        g_ThemeColorArr[2] = g_ThemeSwatches[i].z;
                        g_ThemeColorArr[3] = g_ThemeSwatches[i].w;
                    }
                    if (i < 5) ImGui::SameLine();
                    ImGui::PopID();
                }

                ImGui::Dummy(ImVec2(0, 8));
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 40, 60, 200));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(240, 60, 80, 240));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 20, 40, 255));
                if (ImGui::Button("EXIT OVERLAY", ImVec2(-1, 36.0f * ui_scale))) {
                    g_should_exit = true;
                }
                ImGui::PopStyleColor(3);

                handleTouchScroll();
            }
            ImGui::EndChild();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(19);
}

} // namespace ui::menu
