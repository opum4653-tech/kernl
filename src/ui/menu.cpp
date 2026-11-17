#include "menu.hpp"
#include "floating_logo.hpp"
#include "theme.hpp"
#include "imgui.h"
#include "Android_draw/draw.h"
#include "Android_touch/TouchHelperA.h"
#include "driver.h"
#include "cfg.hpp"
#include "../globals.hpp"
#include "../other/memory.hpp"
#include "../func/esp.hpp"
#include "../func/aim/aim.hpp"
#include "../func/speed.hpp"
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <unistd.h>

extern int native_window_screen_x;
extern int native_window_screen_y;
extern ImFont* g_BoldFont;

// Starts with floating logo closed by default. Menu opens ONLY when user taps the floating logo.
bool g_ShowMenu = false;

static int activeTab = 0;
static std::atomic<bool> g_should_exit{false};
static float ui_opacity = 0.98f;

// ----------------------------------------------------------------------------
// CUSTOM WIDGET: SIDEBAR ICON BUTTON
// ----------------------------------------------------------------------------
static bool DrawSidebarIconButton(const char* str_id, EIconType icon_type, bool is_active, ImVec2 size, float ui_scale) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(str_id, size);
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 bMin = p;
    ImVec2 bMax = ImVec2(p.x + size.x, p.y + size.y);
    ImVec2 center = ImVec2((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f);

    float iconSize = size.y * 0.54f;

    if (is_active) {
        ImU32 colActiveBg = (theme_orchid::ColPrimary & 0x00FFFFFF) | (48 << 24);
        ImU32 colActiveBorder = (theme_orchid::ColPrimary & 0x00FFFFFF) | (160 << 24);
        dl->AddRectFilled(bMin, bMax, colActiveBg, 12.0f * ui_scale);
        dl->AddRect(bMin, bMax, colActiveBorder, 12.0f * ui_scale, 0, 1.5f);
        dl->AddRectFilled(ImVec2(bMin.x + 3.0f * ui_scale, bMin.y + 8.0f * ui_scale),
                          ImVec2(bMin.x + 7.5f * ui_scale, bMax.y - 8.0f * ui_scale),
                          theme_orchid::ColPrimary, 3.0f * ui_scale);
        DrawVectorIcon(dl, icon_type, center, iconSize, theme_orchid::ColWhite);
    } else {
        if (hovered) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 18), 12.0f * ui_scale);
        }
        ImU32 ic_col = hovered ? theme_orchid::ColPrimaryLight : theme_orchid::ColTextMuted;
        DrawVectorIcon(dl, icon_type, center, iconSize * 0.90f, ic_col);
    }

    return pressed;
}

// ----------------------------------------------------------------------------
// TOUCH SCROLL ENGINE (SWIPE DETECTED EVEN OVER ACTIVE CONTROLS)
// ----------------------------------------------------------------------------
static void handleTouchScroll() {
    ImGuiIO& io = ImGui::GetIO();
    enum GestureState { GESTURE_IDLE, GESTURE_HORIZONTAL, GESTURE_VERTICAL };
    static GestureState s_gesture = GESTURE_IDLE;
    static ImVec2 s_touch_start = ImVec2(0, 0);
    static ImGuiID s_active_child = 0;

    ImGuiID child_id = ImGui::GetID("##TouchScrollToken");

    if (io.MouseDown[0]) {
        if (s_gesture == GESTURE_IDLE) {
            if (s_active_child == 0 && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
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
                float delta_y = io.MouseDelta.y;
                if (std::abs(delta_y) > 0.01f) {
                    float cur_scroll = ImGui::GetScrollY();
                    float new_scroll = cur_scroll - delta_y;
                    float max_scroll = ImGui::GetScrollMaxY();
                    if (new_scroll < 0.0f) new_scroll = 0.0f;
                    if (new_scroll > max_scroll) new_scroll = max_scroll;
                    ImGui::SetScrollY(new_scroll);
                }
            }
        }
    } else {
        s_gesture = GESTURE_IDLE;
        s_active_child = 0;
    }
}

// ----------------------------------------------------------------------------
// CUSTOM WIDGET: PILL TOGGLE SWITCH
// Storage is atomic because the worker threads read the same flag this widget
// writes. The widget edits a local copy and stores it back once per press.
// ----------------------------------------------------------------------------
static bool DrawToggleSwitch(const char* label, std::atomic<bool>* v, float row_height = 46.0f, float ui_scale = 1.0f) {
    ImVec2 curPos = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;

    bool on = v->load(std::memory_order_relaxed);

    char btn_id[128];
    snprintf(btn_id, sizeof(btn_id), "##SwitchBtn_%s", label);

    bool pressed = ImGui::InvisibleButton(btn_id, ImVec2(availW, row_height));
    if (pressed) {
        on = !on;
        v->store(on, std::memory_order_relaxed);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. Label on the left, vertically centered
    ImFont* font = g_UiFont ? g_UiFont : ImGui::GetFont();
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
    ImU32 trackCol = on ? theme_orchid::ColSwitchOnTrack : theme_orchid::ColSwitchOffTrack;
    dl->AddRectFilled(sMin, sMax, trackCol, switchRounding);

    // Subtle dark track border when OFF
    if (!on) {
        dl->AddRect(sMin, sMax, IM_COL32(60, 50, 75, 220), switchRounding, 0, 1.2f);
    }

    // Circular white knob / thumb
    float knobRadius = switchRounding - 2.8f * ui_scale;
    float knobX = on ? (sMax.x - switchRounding) : (sMin.x + switchRounding);
    float knobY = sMin.y + switchRounding;

    dl->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, theme_orchid::ColSwitchThumb, 32);

    return pressed;
}

// ----------------------------------------------------------------------------
// CUSTOM WIDGET: SEGMENTED BUTTON ROW (DYNAMICALLY SCALED & RESPONSIVE)
// ----------------------------------------------------------------------------
static bool DrawSegmentedButtons(const char* str_id, std::atomic<int>* selected, const char* const labels[], int count, float height = 30.0f, float ui_scale = 1.0f) {
    float availW = ImGui::GetContentRegionAvail().x;
    float gap = 3.0f * ui_scale;
    float btnW = (availW - gap * (count - 1)) / (float)count;
    if (btnW < 40.0f * ui_scale) btnW = 40.0f * ui_scale;
    bool changed = false;

    int cur = selected->load(std::memory_order_relaxed);

    // Responsive padding based on available button width
    float padX = (btnW < 65.0f * ui_scale) ? 2.0f * ui_scale : 4.0f * ui_scale;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, 2.0f * ui_scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * ui_scale);

    for (int i = 0; i < count; i++) {
        if (i > 0) ImGui::SameLine(0, gap);

        char btnLabel[64];
        snprintf(btnLabel, sizeof(btnLabel), "%s##%s_%d", labels[i], str_id, i);

        bool is_active = (cur == i);
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

        if (ImGui::Button(btnLabel, ImVec2(btnW, height)) && cur != i) {
            cur = i;
            selected->store(cur, std::memory_order_relaxed);
            changed = true;
        }

        ImGui::PopStyleColor(4);
    }
    ImGui::PopStyleVar(2);
    return changed;
}

namespace ui::menu {

void save_logo_pos() {
    ui::floating_logo::save_pos();
}

void load_logo_pos() {
    ui::floating_logo::load_pos();
}

// ----------------------------------------------------------------------------
// MAIN UNIFIED RENDER ENTRY POINT
// ----------------------------------------------------------------------------
void render() {
    // 1. Render pure kernel driver overlays (ESP & Aim assist FOV)
    // Note: func::aim::update() runs dedicatedly in background aim thread (aim.hpp)
    func::esp::draw();
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

    g_sw = realW;
    g_sh = realH;

    // Dynamically synchronize theme accent colors across all components
    theme_orchid::sync_from_arr();

    float ui_scale = realH / 1080.0f;
    if (ui_scale < 0.65f) ui_scale = 0.65f;
    if (ui_scale > 1.45f) ui_scale = 1.45f;

    // ------------------------------------------------------------------------
    // STATE 1: FLOATING LOGO (Modularized into floating_logo.cpp)
    // ------------------------------------------------------------------------
    if (!g_ShowMenu) {
        ui::floating_logo::render(realW, realH, ui_scale);
        return;
    }

    // ------------------------------------------------------------------------
    // STATE 2: MAIN UNIFIED ELECTRIC ORCHID GLASS MENU
    // ------------------------------------------------------------------------
    const float winW = std::max(560.0f * ui_scale, std::min(920.0f * ui_scale, realW * 0.90f));
    const float winH = std::max(420.0f * ui_scale, std::min(580.0f * ui_scale, realH * 0.88f));

    static ImVec2 s_menu_pos = ImVec2(-1.0f, -1.0f);
    static int last_orientation = -1;
    static float last_screen_w = 0.0f;
    static float last_screen_h = 0.0f;
    static bool menu_needs_reposition = false;

    int current_orientation = (realW > realH) ? 1 : 0;
    if (last_orientation != -1 && (current_orientation != last_orientation ||
        std::abs(realW - last_screen_w) > 50.0f || std::abs(realH - last_screen_h) > 50.0f)) {
        menu_needs_reposition = true;
    }
    last_orientation = current_orientation;
    last_screen_w = realW;
    last_screen_h = realH;

    if (s_menu_pos.x < 0.0f || menu_needs_reposition) {
        s_menu_pos = ImVec2((realW - winW) * 0.5f, (realH - winH) * 0.5f);
        menu_needs_reposition = false;
    }

    // Clamp into the viewport. Upper bounds are floored at 0 so a window larger
    // than the screen pins to the top-left corner instead of going negative.
    s_menu_pos.x = std::clamp(s_menu_pos.x, 0.0f, std::max(0.0f, realW - winW));
    s_menu_pos.y = std::clamp(s_menu_pos.y, 0.0f, std::max(0.0f, realH - winH));

    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGui::SetNextWindowPos(s_menu_pos, ImGuiCond_Always);

    ImVec4 colPrim(g_ThemeColorArr[0], g_ThemeColorArr[1], g_ThemeColorArr[2], 1.0f);
    ImVec4 colPrimLight(
        std::min(1.0f, g_ThemeColorArr[0] * 1.15f + 0.1f),
        std::min(1.0f, g_ThemeColorArr[1] * 1.15f + 0.1f),
        std::min(1.0f, g_ThemeColorArr[2] * 1.15f + 0.1f),
        1.0f
    );

    // Exact 20 PushStyleColor calls
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.047f, 0.078f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(colPrim.x, colPrim.y, colPrim.z, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.110f, 0.086f, 0.157f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.160f, 0.120f, 0.220f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.220f, 0.160f, 0.300f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, colPrim);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, colPrim);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, colPrimLight);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.110f, 0.086f, 0.157f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.200f, 0.140f, 0.280f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(colPrim.x, colPrim.y, colPrim.z, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(colPrim.x, colPrim.y, colPrim.z, 0.40f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(colPrim.x, colPrim.y, colPrim.z, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(colPrim.x, colPrim.y, colPrim.z, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.988f, 0.973f, 1.000f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.608f, 0.549f, 0.706f, 1.0f));

    // Invisible scrollbars across window
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0, 0, 0, 0));

    // Exact 4 PushStyleVar calls
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ui_opacity);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * ui_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoCollapse;

    // Register the menu barrier from the rect we are about to force with
    // SetNextWindowPos/Size. Doing it here rather than inside the Begin() body
    // means the barrier still exists on a frame where Begin() reports the
    // window as clipped — otherwise taps in that region bled into the game.
    Touch_AddUiRect(s_menu_pos.x, s_menu_pos.y, winW, winH);

    if (ImGui::Begin("##NinmodsKernelMainWindow", nullptr, winFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // --------------------------------------------------------------------
        // 1. TOP HEADER (SCALED BY ui_scale)
        // --------------------------------------------------------------------
        const float headerH = 50.0f * ui_scale;
        const float closeBtnSize = 36.0f * ui_scale;
        const float xRadius = 6.0f * ui_scale;
        const float badgeH = 24.0f * ui_scale;
        const float badgeW = 76.0f * ui_scale;

        ImVec2 hMin = winPos;
        ImVec2 hMax(winPos.x + winSize.x, winPos.y + headerH);

        // Header Background
        dl->AddRectFilled(hMin, hMax, theme_orchid::ColHeaderBg, 12.0f * ui_scale, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(hMin.x, hMax.y), ImVec2(hMax.x, hMax.y), IM_COL32(40, 30, 56, 255), 1.0f);

        // Header Drag Zone (Excludes close button & status badge)
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##HeaderDragZone", ImVec2(winSize.x - closeBtnSize - badgeW - 30.0f * ui_scale, headerH));
        if (ImGui::IsItemActive()) {
            s_menu_pos.x += io.MouseDelta.x;
            s_menu_pos.y += io.MouseDelta.y;
            // Same floored upper bound as the placement clamp: a window wider
            // than the screen pins at the top-left instead of going negative.
            s_menu_pos.x = std::clamp(s_menu_pos.x, 0.0f, std::max(0.0f, realW - winW));
            s_menu_pos.y = std::clamp(s_menu_pos.y, 0.0f, std::max(0.0f, realH - winH));
        }

        // Header Branding ("NINMODS" + "KERNEL CORE")
        ImFont* brandFont = g_BoldFont ? g_BoldFont : ImGui::GetFont();
        float brandFontSize = 20.0f * ui_scale;
        ImVec2 brandPos(winPos.x + 18.0f * ui_scale, winPos.y + (headerH - brandFontSize) * 0.5f);
        dl->AddText(brandFont, brandFontSize, brandPos, theme_orchid::ColWhite, "NINMODS");

        float brandTextW = brandFont->CalcTextSizeA(brandFontSize, FLT_MAX, 0.0f, "NINMODS").x;
        // Sub-label and badge draw at 12-14px; the 14px ESP face suits them.
        // Knock the 32px menu face down to 12 and the stems go muddy.
        ImFont* smallFont = g_EspFont ? g_EspFont : ImGui::GetFont();
        dl->AddText(smallFont, 14.0f * ui_scale,
                    ImVec2(brandPos.x + brandTextW + 8.0f * ui_scale, brandPos.y + 3.0f * ui_scale),
                    theme_orchid::ColPrimaryLight, "KERNEL CORE");

        // Live Connection Status Badge
        bool connected = (proc::pid.load(std::memory_order_relaxed) > 0 && proc::lib.load(std::memory_order_relaxed) > 0);
        ImVec2 bMin(winPos.x + winSize.x - closeBtnSize - badgeW - 14.0f * ui_scale, winPos.y + (headerH - badgeH) * 0.5f);
        ImVec2 bMax(bMin.x + badgeW, bMin.y + badgeH);
        ImU32 badgeBg = connected ? IM_COL32(16, 48, 32, 220) : IM_COL32(48, 20, 24, 220);
        ImU32 badgeBorder = connected ? IM_COL32(0, 230, 118, 200) : IM_COL32(255, 75, 75, 200);
        dl->AddRectFilled(bMin, bMax, badgeBg, 6.0f * ui_scale);
        dl->AddRect(bMin, bMax, badgeBorder, 6.0f * ui_scale, 0, 1.0f);

        const char* stTxt = connected ? "ARMED" : "STANDBY";
        ImVec2 ssz = smallFont->CalcTextSizeA(12.0f * ui_scale, FLT_MAX, 0.0f, stTxt);
        ImVec2 stPos(bMin.x + (badgeW - ssz.x) * 0.5f, bMin.y + (badgeH - ssz.y) * 0.5f);
        dl->AddText(smallFont, 12.0f * ui_scale, stPos, theme_orchid::ColWhite, stTxt);

        // Close Button (Smooth 'X')
        ImVec2 cMin(winPos.x + winSize.x - closeBtnSize - 8.0f * ui_scale, winPos.y + (headerH - closeBtnSize) * 0.5f);
        ImGui::SetCursorScreenPos(cMin);
        if (ImGui::InvisibleButton("##CloseMenuBtn", ImVec2(closeBtnSize, closeBtnSize))) {
            g_ShowMenu = false;
        }
        bool closeHovered = ImGui::IsItemHovered();
        ImVec2 cCenter(cMin.x + closeBtnSize * 0.5f, cMin.y + closeBtnSize * 0.5f);
        ImU32 closeCol = closeHovered ? theme_orchid::ColPrimaryLight : IM_COL32(200, 195, 215, 230);
        dl->AddLine(ImVec2(cCenter.x - xRadius, cCenter.y - xRadius), ImVec2(cCenter.x + xRadius, cCenter.y + xRadius), closeCol, 2.0f);
        dl->AddLine(ImVec2(cCenter.x + xRadius, cCenter.y - xRadius), ImVec2(cCenter.x - xRadius, cCenter.y + xRadius), closeCol, 2.0f);

        // --------------------------------------------------------------------
        // 2. LEFT SIDEBAR NAVIGATION
        // --------------------------------------------------------------------
        const float bodyH = winSize.y - headerH;
        const float sidebarW = std::max(88.0f, 96.0f * ui_scale);

        ImGui::SetCursorPos(ImVec2(0, headerH));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.067f, 0.055f, 0.094f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::BeginChild("##SidebarKernel", ImVec2(sidebarW, bodyH), false, ImGuiWindowFlags_NoScrollbar)) {
            dl->AddLine(ImVec2(winPos.x + sidebarW - 0.5f, winPos.y + headerH),
                        ImVec2(winPos.x + sidebarW - 0.5f, winPos.y + winSize.y),
                        IM_COL32(40, 30, 56, 255), 1.0f);

            float availSidebarH = bodyH - 24.0f * ui_scale;
            float btnH = std::clamp(availSidebarH / 4.6f, 36.0f, 60.0f);
            float btnSpacing = std::clamp((availSidebarH - btnH * 4.0f) / 3.0f, 4.0f * ui_scale, 12.0f * ui_scale);
            const ImVec2 btnSize = ImVec2(sidebarW - 14.0f * ui_scale, btnH);
            const float btnX = (sidebarW - btnSize.x) * 0.5f;

            // Tab 0: Visuals (ICON_EYE)
            ImGui::SetCursorPos(ImVec2(btnX, 12.0f * ui_scale));
            if (DrawSidebarIconButton("##TabESP", ICON_EYE, (activeTab == 0), btnSize, ui_scale)) {
                activeTab = 0;
            }

            // Tab 1: Combat / Aim (ICON_CROSSHAIR)
            ImGui::SetCursorPos(ImVec2(btnX, 12.0f * ui_scale + btnH + btnSpacing));
            if (DrawSidebarIconButton("##TabCombat", ICON_CROSSHAIR, (activeTab == 1), btnSize, ui_scale)) {
                activeTab = 1;
            }

            // Tab 2: Settings / Misc (ICON_SETTINGS)
            ImGui::SetCursorPos(ImVec2(btnX, 12.0f * ui_scale + (btnH + btnSpacing) * 2.0f));
            if (DrawSidebarIconButton("##TabSettings", ICON_SETTINGS, (activeTab == 2), btnSize, ui_scale)) {
                activeTab = 2;
            }

            // Tab 3: Power / Exit. Bottom of the sidebar, clamped between the
            // Tab 2 clearance and the child's lower edge — the old std::max had
            // no upper bound, so a short window pushed the button out of clip.
            float powerMinY = 12.0f * ui_scale + (btnH + btnSpacing) * 3.0f;
            float powerMaxY = std::max(powerMinY, bodyH - btnH);
            float powerY = std::clamp(bodyH - btnH - 12.0f * ui_scale, powerMinY, powerMaxY);
            ImGui::SetCursorPos(ImVec2(btnX, powerY));
            if (DrawSidebarIconButton("##TabPower", ICON_POWER, false, btnSize, ui_scale)) {
                g_should_exit = true;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // --------------------------------------------------------------------
        // 3. MAIN CONTENT AREA (FULLY SCALED DUAL-COLUMN GLASS CARDS)
        // --------------------------------------------------------------------
        const float contentGapX = 14.0f * ui_scale;
        const float contentGapY = 10.0f * ui_scale;
        const float contentX = sidebarW + contentGapX;
        const float contentY = headerH + contentGapY;
        const float contentW = winSize.x - contentX - contentGapX;
        const float contentH = bodyH - (contentGapY * 2.0f);
        const float colGap   = 14.0f * ui_scale;
        const float colW     = (contentW - colGap) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.086f, 0.067f, 0.130f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.196f, 0.141f, 0.282f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * ui_scale, 10.0f * ui_scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * ui_scale, 8.0f * ui_scale));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * ui_scale, 4.0f * ui_scale));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 14.0f * ui_scale);

        // TAB 0: ESP VISUALS
        if (activeTab == 0) {
            // Left Column: Features
            ImGui::SetCursorPos(ImVec2(contentX, contentY));
            if (ImGui::BeginChild("##ColLeftESP", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "ESP VISUALS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 95 * ui_scale, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                if (DrawToggleSwitch("Master Switch", &cfg::master_enabled, 44.0f * ui_scale, ui_scale)) {
                    if (!cfg::master_enabled) {
                        func::aim::motion::reset();
                    }
                }
                ImGui::Dummy(ImVec2(0, 6 * ui_scale));

                cfg::Checkbox("ESP Box (Chaturbhuj)", cfg::esp::box);
                cfg::Checkbox("ESP Health Bar (Right)", cfg::esp::health);
                cfg::Checkbox("ESP Armor Bar", cfg::esp::armor);
                cfg::Checkbox("ESP Laser Line", cfg::esp::tracer);
                cfg::Checkbox("ESP Player Name", cfg::esp::name);
                cfg::Checkbox("ESP Distance", cfg::esp::distance);
                cfg::Checkbox("ESP Bot Tag (AI)", cfg::esp::bot_tag);
                cfg::Checkbox("ESP Weapon Name", cfg::esp::weapon);
                cfg::Checkbox("Firing Indicator", cfg::esp::firing_ind);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Settings & Filters
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightESP", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "SETTINGS & FILTERS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 120 * ui_scale, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                cfg::Checkbox("Ignore Bot Players", cfg::esp::ignore_bots);
                cfg::Checkbox("Crosshair Center", cfg::esp::crosshair);
                cfg::Checkbox("Loot ESP", cfg::esp::loot);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                static const char* const boxStyles[] = { "Corner", "Full", "Glow" };
                ImGui::TextDisabled("Box Style Mode");
                DrawSegmentedButtons("BoxStyle", &cfg::esp::box_type, boxStyles, 3, 30.0f * ui_scale, ui_scale);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##MaxRange", cfg::esp::max_range, 50.0f, 500.0f, "%.0f m");
                ImGui::TextDisabled("Max Render Distance");

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##LaserThickness", cfg::esp::tracer_thickness, 0.5f, 4.0f, "%.1f px");
                ImGui::TextDisabled("Laser Line Width");

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##BoxThickness", cfg::esp::box_thickness, 1.0f, 5.0f, "%.1f px");
                ImGui::TextDisabled("Box Border Width");

                ImGui::Dummy(ImVec2(0, 6 * ui_scale));
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
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 130 * ui_scale, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                if (DrawToggleSwitch("Master Switch", &cfg::master_enabled, 44.0f * ui_scale, ui_scale)) {
                    if (!cfg::master_enabled) {
                        func::aim::motion::reset();
                    }
                }
                ImGui::Dummy(ImVec2(0, 6 * ui_scale));

                cfg::Checkbox("Auto Headshot (100% Red)", cfg::aim::auto_headshot);
                cfg::Checkbox("Aim Silent", cfg::aim::silent);
                cfg::Checkbox("Enable Aim Assist", cfg::aim::enable);
                cfg::Checkbox("Scope Aim (Instant Head)", cfg::aim::scope_aim);
                cfg::Checkbox("Sniper Headshot Lock", cfg::aim::sniper_aim);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                static const char* const boneLabels[] = { "Head", "Chest", "Hip" };
                ImGui::TextDisabled("Target Aim Bone");
                DrawSegmentedButtons("AimBone", &cfg::aim::target_bone, boneLabels, 3, 30.0f * ui_scale, ui_scale);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                static const char* const triggerLabels[] = { "On Firing", "In FOV" };
                ImGui::TextDisabled("Aim Trigger Condition");
                DrawSegmentedButtons("AimTrigger", &cfg::aim::trigger_mode, triggerLabels, 2, 30.0f * ui_scale, ui_scale);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                static const char* const priorityLabels[] = { "Crosshair", "Low HP", "Distance" };
                ImGui::TextDisabled("Target Priority");
                DrawSegmentedButtons("AimPriority", &cfg::aim::priority_mode, priorityLabels, 3, 30.0f * ui_scale, ui_scale);

                ImGui::Dummy(ImVec2(0, 6 * ui_scale));
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "SAFETY FILTERS");
                lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 100 * ui_scale, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                cfg::Checkbox("Ignore Knocked", cfg::aim::ignore_knocked);
                cfg::Checkbox("Ignore AI Bots", cfg::aim::ignore_bots);
                cfg::Checkbox("Visible Check", cfg::aim::visible_check);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Sliders & Modes
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightAim", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "FOV & SMOOTHNESS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 125 * ui_scale, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                cfg::Checkbox("Draw FOV Circle", cfg::aim::show_fov);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##AimFovSlider", cfg::aim::aim_fov, 10.0f, 360.0f, "%.0f deg");
                ImGui::TextDisabled("Aim FOV Range");

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##SilentRateSlider", cfg::aim::headshot_rate, 0.0f, 100.0f, "%.0f%%");
                ImGui::TextDisabled("Silent Aim Hit Rate");

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##AimSmooth", cfg::aim::smooth, 1.0f, 25.0f, "%.1f");
                ImGui::TextDisabled("Smooth Factor (Higher = Humanized)");

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##AimMaxDist", cfg::aim::max_distance, 20.0f, 300.0f, "%.0f m");
                ImGui::TextDisabled("Max Aiming Range");

                ImGui::Dummy(ImVec2(0, 8 * ui_scale));
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "LIVE TARGET HUD");
                rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 105 * ui_scale, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                func::aim::TargetInfo ti = func::aim::snapshot_target();
                if (ti.valid) {
                    const char* targetName = ti.name[0] ? ti.name : "Enemy";
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Target: %s", targetName);
                    ImGui::Text("Distance: %.1f m", ti.distance);
                    ImGui::Text("Health: %d HP", ti.hp);
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
        // TAB 2: MISC & SYSTEM SETTINGS (BALANCED DUAL COLUMNS)
        else if (activeTab == 2) {
            // Left Column: Movement & Special Capabilities
            ImGui::SetCursorPos(ImVec2(contentX, contentY));
            if (ImGui::BeginChild("##ColLeftMisc", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "MOVEMENT & VISION");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 130 * ui_scale, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                if (DrawToggleSwitch("Master Switch", &cfg::master_enabled, 44.0f * ui_scale, ui_scale)) {
                    if (!cfg::master_enabled) {
                        func::aim::motion::reset();
                    }
                }
                ImGui::Dummy(ImVec2(0, 6 * ui_scale));

                cfg::Checkbox("Wide FOV (iPad View)", cfg::motion::ipad_view);
                if (cfg::motion::ipad_view) {
                    ImGui::SetNextItemWidth(-1);
                    cfg::SliderFloat("##IpadScale", cfg::motion::ipad_val, 1.0f, 2.5f, "%.1fx");
                }

                cfg::Checkbox("Speed Hack", cfg::motion::speed_hack);
                if (cfg::motion::speed_hack) {
                    ImGui::SetNextItemWidth(-1);
                    cfg::SliderFloat("##SpeedMult", cfg::motion::speed_mult, 1.0f, 5.0f, "%.1fx");
                }

                cfg::Checkbox("High Jump", cfg::motion::high_jump);
                if (cfg::motion::high_jump) {
                    ImGui::SetNextItemWidth(-1);
                    cfg::SliderFloat("##HighJump", cfg::motion::high_jump_val, 1.0f, 5.0f, "%.1fx");
                }

                cfg::Checkbox("Auto-Pickup Range", cfg::motion::pickup_range);
                if (cfg::motion::pickup_range) {
                    ImGui::SetNextItemWidth(-1);
                    cfg::SliderFloat("##PickupRange", cfg::motion::pickup_val, 5.0f, 35.0f, "%.0f m");
                }

                cfg::Checkbox("Air Flight (Elevation)", cfg::motion::air_flight);
                if (cfg::motion::air_flight) {
                    ImGui::SetNextItemWidth(-1);
                    cfg::SliderFloat("##FlightHeight", cfg::motion::flight_height, 2.0f, 15.0f, "%.1f m");
                }

                cfg::Checkbox("Ghost Hack", cfg::motion::ghost_hack);
                cfg::Checkbox("Phantom Mode", cfg::motion::phantom_mode);

                ImGui::Dummy(ImVec2(0, 6 * ui_scale));
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "MOVEMENT DIAGNOSTICS");
                lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 130 * ui_scale, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                float statColW = std::clamp(80.0f * ui_scale, 75.0f, 96.0f);
                ImGui::TextDisabled("POSTURE:");
                ImGui::SameLine(statColW);
                ImGui::Text("%s", cfg::motion::air_flight ? "AIRBORNE" : "GROUND");

                ImGui::TextDisabled("SPEED:");
                ImGui::SameLine(statColW);
                ImGui::Text("%.1fx", cfg::motion::speed_hack ? cfg::motion::speed_mult.load(std::memory_order_relaxed) : 1.0f);

                ImGui::TextDisabled("ENTITY:");
                ImGui::SameLine(statColW);
                ImGui::Text("%s", cfg::motion::phantom_mode ? "PHANTOM" : "NORMAL");

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Combat Mods & System Status
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightMisc", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "COMBAT & SYSTEM");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 130 * ui_scale, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4 * ui_scale));

                cfg::Checkbox("Hitbox Scaler", cfg::motion::hitbox_scale);
                if (cfg::motion::hitbox_scale) {
                    ImGui::SetNextItemWidth(-1);
                    cfg::SliderFloat("##HitboxScale", cfg::motion::hitbox_size, 1.0f, 3.0f, "%.1fx");
                }

                cfg::Checkbox("No Recoil", cfg::motion::no_recoil);
                cfg::Checkbox("No Reload", cfg::motion::no_reload);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::TextDisabled("DRIVER & SYSTEM INFO");

                const char* dName = driver ? driver->get_driver_name() : "ARMED";

                char pidStr[32];
                snprintf(pidStr, sizeof(pidStr), "%d", proc::pid.load());

                char libStr[32];
                snprintf(libStr, sizeof(libStr), "0x%lX", (unsigned long)proc::lib.load());

                float labelColW = std::clamp(80.0f * ui_scale, 75.0f, 96.0f);

                ImGui::TextDisabled("PID:");
                ImGui::SameLine(labelColW);
                ImGui::Text("%s", proc::pid.load(std::memory_order_relaxed) > 0 ? pidStr : "NONE");

                ImGui::TextDisabled("LIB BASE:");
                ImGui::SameLine(labelColW);
                ImGui::Text("%s", proc::lib.load(std::memory_order_relaxed) > 0 ? libStr : "NONE");

                ImGui::TextDisabled("DRIVER:");
                ImGui::SameLine(labelColW);
                ImGui::TextColored(ImVec4(0, 0.85f, 0.9f, 1), "%s", dName);

                ImGui::TextDisabled("FPS:");
                ImGui::SameLine(labelColW);
                ImGui::Text("%.1f", io.Framerate);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                // Floor is 0.40, not 0.20. The slider lives inside the window
                // it dims via ImGuiStyleVar_Alpha, so at 0.20 the control that
                // would bring the opacity back was itself at 20% - nearly
                // invisible and very hard to grab.
                ImGui::SliderFloat("##Opacity", &ui_opacity, 0.40f, 1.0f, "Opacity: %.2f");

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
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

                ImGui::Dummy(ImVec2(0, 6 * ui_scale));
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

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(20);
}

} // namespace ui::menu
