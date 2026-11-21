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
#include "../game/data.hpp"
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <unistd.h>

// ==============================================================================================
// FILE: src/ui/menu.cpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Interactive ImGui Graphical User Interface, Floating Logo, and Touch Scroll Controller.
//
// SYSTEM ARCHITECTURE & TAB STRUCTURE:
// ----------------------------------------------------------------------------------------------
// 1. FLOATING LOGO (src/ui/floating_logo.cpp):
//    - Renders as a draggable circular floating logo on screen.
//    - Tapping toggles g_ShowMenu (true = menu open, false = menu minimized to logo).
//
// 2. TOUCH SCROLL ENGINE (handleTouchScroll()):
//    - Custom swipe gesture detector handling vertical flick & drag scrolling on touch screens.
//    - Works even when finger touches directly over sliders, buttons, or checkboxes.
//
// 3. THREE-TAB CONFIGURATION INTERFACE:
//    - Tab 0 (Kernel Visuals):
//      * Master Switch, Kernel Bounding Box, Kernel Armor Indicator, Kernel Target Line, Kernel Entity Tag,
//        Max Render Distance Slider (0m - 500m), Fixed Purple Accent (#E10098).
//    - Tab 1 (Kernel Trajectory):
//      * Master Switch, Kernel Silent Trajectory (Unnormalized Vector), Kernel Scope Trigger (Instant Head),
//        Kernel Precision Vector (100% Red), Ignore Downed Entities, Kernel FOV Boundary, Elevation Bias.
//    - Tab 2 (Kernel System & Diagnostics):
//      * Master Switch, 100% Official Stock Movement Model,
//        Driver Telemetry (PID, LibBase, Driver Mode), and EXIT OVERLAY Button.
//
// 4. CROSS-FILE RENDER PIPELINE:
//    - Calls func::esp::draw() (in src/func/esp/esp.hpp) to draw world ESP on background draw list.
//    - Updates touch regions via Touch_ClearUiRects() and Touch_CommitUiRects() (in TouchHelperA.h).
// ==============================================================================================

extern int native_window_screen_x;
extern int native_window_screen_y;

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

// ----------------------------------------------------------------------------
// CUSTOM WIDGET: SECTION HEADER
// Title in the live theme accent plus a rule sized to the text that was just
// drawn. The inline blocks this replaces hardcoded orchid and hand-picked rule
// widths (95 / 105 / 120 / 125 / 130 px), so switching the accent swatch left
// every section title pink and the underline wider than the words above it.
// ----------------------------------------------------------------------------
static void SectionHeader(const char* title, float ui_scale) {
    ImFont* font = g_BoldFont ? g_BoldFont : (g_UiFont ? g_UiFont : ImGui::GetFont());
    const float fs = 15.0f * ui_scale;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, title);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(font, fs, pos, theme_orchid::ColPrimaryLight, title);
    float ruleY = pos.y + ts.y + 3.0f * ui_scale;
    dl->AddLine(ImVec2(pos.x, ruleY), ImVec2(pos.x + ts.x, ruleY), theme_orchid::ColPrimary, 2.0f);

    ImGui::Dummy(ImVec2(0.0f, ts.y + 6.0f * ui_scale));
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
    ImGuiIO& io = ImGui::GetIO();
    float realW = (io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : (float)native_window_screen_x;
    float realH = (io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : (float)native_window_screen_y;
    if (realW <= 0) realW = 2400.0f;
    if (realH <= 0) realH = 1080.0f;

    // Publish the viewport BEFORE any overlay draws. The ESP and FOV paths read
    // g_sw / g_sh for their projection, and the old order ran them first, so
    // every overlay was one frame behind after a rotation - and on the very
    // first frame it used globals.cpp's 1920x1080 placeholder.
    g_sw = realW;
    g_sh = realH;

    // Dynamically synchronize theme accent colors across all components
    theme_orchid::sync_from_arr();

    float ui_scale = realH / 1080.0f;
    if (ui_scale < 0.65f) ui_scale = 0.65f;
    if (ui_scale > 1.45f) ui_scale = 1.45f;

    // 1. Render pure kernel driver overlays (ESP & Aim assist FOV)
    // Note: func::aim::update() runs dedicatedly in background aim thread (aim.hpp)
    // Both overlays draw on the background list. ImGui::Render() emits
    // background -> all windows -> foreground, so this is what keeps the menu
    // window and the floating logo above ESP and the FOV circle.
    func::esp::draw();
    trajectory::render_fov(ImGui::GetBackgroundDrawList());

    if (g_should_exit.load()) {
        main_thread_flag = false;
        g_ShowMenu = false;
        return;
    }

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
    const float winH = std::max(460.0f * ui_scale, std::min(620.0f * ui_scale, realH * 0.90f));

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
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
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
        bool fontPushed = false;
        if (g_UiFont) {
            ImGui::PushFont(g_UiFont);
            fontPushed = true;
        }
        ImGui::SetWindowFontScale(std::clamp(ui_scale, 0.90f, 1.10f));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // --------------------------------------------------------------------
        // 1. TOP HEADER (COMPACT HEIGHT & BALANCED GEOMETRY)
        // --------------------------------------------------------------------
        const float sidebarW = std::max(76.0f, 84.0f * ui_scale);
        const float headerH = 48.0f * ui_scale; // Compact header height (reduced from ~96px)
        const float bodyH = winSize.y - headerH;
        const float closeBtnSize = 28.0f * ui_scale;
        const float xHalf = closeBtnSize * 0.28f;
        const float badgeH = 22.0f * ui_scale;
        const float badgeW = 70.0f * ui_scale;

        ImVec2 hMin = winPos;
        ImVec2 hMax(winPos.x + winSize.x, winPos.y + headerH);

        // Header Background
        dl->AddRectFilled(hMin, hMax, theme_orchid::ColHeaderBg, 12.0f * ui_scale, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(hMin.x, hMax.y), ImVec2(hMax.x, hMax.y), IM_COL32(40, 30, 56, 255), 1.0f);

        // Right-hand cluster: close button padded comfortably from right window border (18px)
        const float closeRightPad = 18.0f * ui_scale;
        const ImVec2 cMin(winPos.x + winSize.x - closeBtnSize - closeRightPad,
                          winPos.y + (headerH - closeBtnSize) * 0.5f);
        const ImVec2 bMin(cMin.x - badgeW - 12.0f * ui_scale,
                          winPos.y + (headerH - badgeH) * 0.5f);
        const ImVec2 bMax(bMin.x + badgeW, bMin.y + badgeH);

        // Header Drag Zone (Excludes close button & status badge)
        const float dragW = std::max(32.0f * ui_scale, bMin.x - winPos.x - 10.0f * ui_scale);
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##HeaderDragZone", ImVec2(dragW, headerH));
        if (ImGui::IsItemActive()) {
            s_menu_pos.x += io.MouseDelta.x;
            s_menu_pos.y += io.MouseDelta.y;
            s_menu_pos.x = std::clamp(s_menu_pos.x, 0.0f, std::max(0.0f, realW - winW));
            s_menu_pos.y = std::clamp(s_menu_pos.y, 0.0f, std::max(0.0f, realH - winH));
        }

        // Header Branding ("NINMODS" + "KERNEL CORE")
        ImFont* brandFont = g_BoldFont ? g_BoldFont : ImGui::GetFont();
        float brandFontSize = 18.0f * ui_scale;
        ImVec2 brandPos(winPos.x + 16.0f * ui_scale, winPos.y + (headerH - brandFontSize) * 0.5f);
        dl->AddText(brandFont, brandFontSize, brandPos, theme_orchid::ColWhite, "NINMODS");

        float brandTextW = brandFont->CalcTextSizeA(brandFontSize, FLT_MAX, 0.0f, "NINMODS").x;
        ImFont* smallFont = g_EspFont ? g_EspFont : ImGui::GetFont();
        const float subFontSize = 12.0f * ui_scale;
        const float subX = brandPos.x + brandTextW + 8.0f * ui_scale;
        const float subW = smallFont->CalcTextSizeA(subFontSize, FLT_MAX, 0.0f, "KERNEL CORE").x;
        if (subX + subW <= bMin.x - 8.0f * ui_scale) {
            dl->AddText(smallFont, subFontSize,
                        ImVec2(subX, winPos.y + (headerH - subFontSize) * 0.5f),
                        theme_orchid::ColPrimaryLight, "KERNEL CORE");
        }

        // Live Connection Status Badge
        bool connected = (proc::pid.load(std::memory_order_relaxed) > 0 && proc::lib.load(std::memory_order_relaxed) > 0);
        ImU32 badgeBg = connected ? IM_COL32(16, 48, 32, 220) : IM_COL32(48, 20, 24, 220);
        ImU32 badgeBorder = connected ? IM_COL32(0, 230, 118, 200) : IM_COL32(255, 75, 75, 200);
        dl->AddRectFilled(bMin, bMax, badgeBg, 6.0f * ui_scale);
        dl->AddRect(bMin, bMax, badgeBorder, 6.0f * ui_scale, 0, 1.0f);

        const char* stTxt = connected ? "ARMED" : "STANDBY";
        ImVec2 ssz = smallFont->CalcTextSizeA(11.0f * ui_scale, FLT_MAX, 0.0f, stTxt);
        ImVec2 stPos(bMin.x + (badgeW - ssz.x) * 0.5f, bMin.y + (badgeH - ssz.y) * 0.5f);
        dl->AddText(smallFont, 11.0f * ui_scale, stPos, theme_orchid::ColWhite, stTxt);

        // Close Button (Smooth 'X' with instant-touch response)
        const float hitPad = 6.0f * ui_scale;
        const ImVec2 hitMin(cMin.x - hitPad, cMin.y - hitPad);
        const ImVec2 hitSize(closeBtnSize + hitPad * 2.0f, closeBtnSize + hitPad * 2.0f);

        ImGui::SetCursorScreenPos(hitMin);
        bool closePressed = ImGui::InvisibleButton("##CloseMenuBtn", hitSize);
        bool closeHovered = ImGui::IsItemHovered();

        // Instant close on release OR on touch-down (MouseClicked[0])
        if (closePressed || (closeHovered && io.MouseClicked[0])) {
            g_ShowMenu = false;
        }

        ImVec2 cCenter(cMin.x + closeBtnSize * 0.5f, cMin.y + closeBtnSize * 0.5f);
        if (closeHovered) {
            dl->AddCircleFilled(cCenter, (closeBtnSize * 0.5f) + 2.0f,
                                (theme_orchid::ColPrimary & 0x00FFFFFF) | (60u << 24), 32);
            dl->AddCircle(cCenter, (closeBtnSize * 0.5f) + 2.0f,
                          theme_orchid::ColPrimary, 32, 1.5f);
        }
        ImU32 closeCol = closeHovered ? theme_orchid::ColPrimaryLight : IM_COL32(200, 195, 215, 230);
        dl->AddLine(ImVec2(cCenter.x - xHalf, cCenter.y - xHalf),
                    ImVec2(cCenter.x + xHalf, cCenter.y + xHalf), closeCol, 2.4f);
        dl->AddLine(ImVec2(cCenter.x + xHalf, cCenter.y - xHalf),
                    ImVec2(cCenter.x - xHalf, cCenter.y + xHalf), closeCol, 2.4f);

        // --------------------------------------------------------------------
        // 2. LEFT SIDEBAR NAVIGATION
        // --------------------------------------------------------------------
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
                SectionHeader("ESP VISUALS", ui_scale);

                cfg::Checkbox("ESP Box", cfg::esp::box);
                cfg::Checkbox("ESP Armor Bar", cfg::esp::armor);
                cfg::Checkbox("ESP Laser Line", cfg::esp::tracer);
                cfg::Checkbox("ESP Bot Tag", cfg::esp::bot_tag);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Settings & Styling
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightESP", ImVec2(colW, contentH), true)) {
                SectionHeader("SETTINGS & STYLING", ui_scale);

                ImGui::TextDisabled("Range: camera view (frustum-only)");
                ImGui::TextWrapped("ESP shows only players visible on screen. "
                                   "No distance cap — if you can see it, ESP marks it.");

                handleTouchScroll();
            }
            ImGui::EndChild();
        }
        // TAB 1: AIMBOT ENGINE
        else if (activeTab == 1) {
            // Left Column: Trajectory Engine
            ImGui::SetCursorPos(ImVec2(contentX, contentY));
            if (ImGui::BeginChild("##ColLeftAim", ImVec2(colW, contentH), true)) {
                SectionHeader("AIMBOT ENGINE", ui_scale);

                cfg::Checkbox("Aim Silent", cfg::aim::silent);
                cfg::Checkbox("Scope Aim (Instant Head)", cfg::aim::scope_aim);
                cfg::Checkbox("Auto Headshot", cfg::aim::auto_headshot);
                cfg::Checkbox("Ignore Knocked", cfg::aim::ignore_knocked);

                ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                ImGui::TextDisabled("HEADSHOT RATE:");
                ImGui::SetNextItemWidth(-1);
                cfg::SliderInt("##HeadshotRate", cfg::aim::headshot_rate, 0, 100, "%d%% Headshot Rate");

                int hsRate = cfg::aim::headshot_rate.load(std::memory_order_relaxed);
                bool hsEnabled = cfg::aim::auto_headshot.load(std::memory_order_relaxed);
                if (!hsEnabled) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "OFF - enable Auto Headshot to use rate");
                } else if (hsRate > 70) {
                    ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "[RISK] >70%% Ban Risk (ML Outlier)");
                    ImGui::TextDisabled("Effective rate drops with distance");
                } else {
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "[SAFE] %d%% Natural Player Rate", hsRate);
                    ImGui::TextDisabled("Effective rate drops with distance");
                }

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Trajectory & FOV Boundary
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightAim", ImVec2(colW, contentH), true)) {
                SectionHeader("TRAJECTORY & FOV", ui_scale);

                cfg::Checkbox("Draw FOV Circle", cfg::aim::show_fov);
                if (cfg::aim::show_fov.load(std::memory_order_relaxed)) {
                    ImGui::Dummy(ImVec2(0, 4 * ui_scale));
                    ImGui::SetNextItemWidth(-1);
                    cfg::SliderFloat("##FovRangeSlider", cfg::aim::aim_fov, 0.0f, 180.0f, "FOV Range: %.0f deg");
                    ImGui::TextDisabled("%s", "Adjust kill zone circle radius");
                }

                ImGui::Dummy(ImVec2(0, 6 * ui_scale));
                ImGui::SetNextItemWidth(-1);
                cfg::SliderFloat("##HeadBiasSlider", cfg::aim::head_bias, -0.15f, 0.15f, "%.2f m");
                ImGui::TextDisabled("%s", "Headshot Vertical Trim (0.00m = Exact Head Center)");

                handleTouchScroll();
            }
            ImGui::EndChild();
        }
        // TAB 2: SYSTEM & SETTINGS
        else if (activeTab == 2) {
            // Left Column: Master Settings & Safe Operation
            ImGui::SetCursorPos(ImVec2(contentX, contentY));
            if (ImGui::BeginChild("##ColLeftMisc", ImVec2(colW, contentH), true)) {
                SectionHeader("SYSTEM CONTROLS", ui_scale);

                cfg::Checkbox("Master Switch", cfg::master_enabled);
                ImGui::Dummy(ImVec2(0, 8 * ui_scale));

                ImGui::TextDisabled("Kernel Mod Engine");
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "Zero-Footprint Anti-Ban Safe");
                ImGui::Dummy(ImVec2(0, 6 * ui_scale));

                ImGui::TextDisabled("Movement Model");
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "100%% Official Stock");

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: System Diagnostics & Exit
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightMisc", ImVec2(colW, contentH), true)) {
                SectionHeader("SYSTEM & EXIT", ui_scale);

                ImDrawList* cdl = ImGui::GetWindowDrawList();
                auto DrawDiagTile = [&](const char* label, const char* val, ImU32 valCol) {
                    float availW = ImGui::GetContentRegionAvail().x;
                    ImFont* f = g_UiFont ? g_UiFont : ImGui::GetFont();
                    float fSize = 13.0f * ui_scale;

                    ImVec2 lSz = f->CalcTextSizeA(fSize, FLT_MAX, 0.0f, label);
                    ImVec2 vSz = f->CalcTextSizeA(fSize, FLT_MAX, 0.0f, val);

                    if (availW >= (lSz.x + vSz.x + 28.0f * ui_scale)) {
                        float tileH = 34.0f * ui_scale;
                        ImVec2 p0 = ImGui::GetCursorScreenPos();
                        ImVec2 p1(p0.x + availW, p0.y + tileH);

                        cdl->AddRectFilled(p0, p1, IM_COL32(20, 16, 28, 220), 6.0f * ui_scale);
                        cdl->AddRect(p0, p1, IM_COL32(48, 38, 66, 180), 6.0f * ui_scale, 0, 1.0f);

                        ImVec2 lPos(p0.x + 10.0f * ui_scale, p0.y + (tileH - fSize) * 0.5f);
                        cdl->AddText(f, fSize, lPos, theme_orchid::ColTextMuted, label);

                        ImVec2 vPos(p1.x - vSz.x - 10.0f * ui_scale, p0.y + (tileH - fSize) * 0.5f);
                        cdl->AddText(f, fSize, vPos, valCol, val);

                        ImGui::Dummy(ImVec2(availW, tileH + 4.0f * ui_scale));
                    } else {
                        float tileH = 46.0f * ui_scale;
                        ImVec2 p0 = ImGui::GetCursorScreenPos();
                        ImVec2 p1(p0.x + availW, p0.y + tileH);

                        cdl->AddRectFilled(p0, p1, IM_COL32(20, 16, 28, 220), 6.0f * ui_scale);
                        cdl->AddRect(p0, p1, IM_COL32(48, 38, 66, 180), 6.0f * ui_scale, 0, 1.0f);

                        ImVec2 lPos(p0.x + 10.0f * ui_scale, p0.y + 6.0f * ui_scale);
                        cdl->AddText(f, fSize * 0.9f, lPos, theme_orchid::ColTextMuted, label);

                        ImVec2 vPos(p0.x + 10.0f * ui_scale, p0.y + tileH - fSize - 6.0f * ui_scale);
                        cdl->AddText(f, fSize, vPos, valCol, val);

                        ImGui::Dummy(ImVec2(availW, tileH + 4.0f * ui_scale));
                    }
                };

                const char* dName = proc::driver_status().name;

                char pidStr[32];
                if (proc::pid.load(std::memory_order_relaxed) > 0) {
                    snprintf(pidStr, sizeof(pidStr), "%d", proc::pid.load(std::memory_order_relaxed));
                } else {
                    snprintf(pidStr, sizeof(pidStr), "NONE");
                }

                char libStr[32];
                if (proc::lib.load(std::memory_order_relaxed) > 0) {
                    snprintf(libStr, sizeof(libStr), "0x%lX", (unsigned long)proc::lib.load(std::memory_order_relaxed));
                } else {
                    snprintf(libStr, sizeof(libStr), "NONE");
                }

                DrawDiagTile("PID", pidStr, IM_COL32(240, 235, 255, 255));
                DrawDiagTile("LIB BASE", libStr, IM_COL32(240, 235, 255, 255));
                DrawDiagTile("DRIVER", dName, IM_COL32(0, 220, 235, 255));

                char playCountStr[32];
                {
                    std::lock_guard<std::mutex> lk(data::players_mutex);
                    snprintf(playCountStr, sizeof(playCountStr), "%zu ENEMY", data::players.size());
                }
                DrawDiagTile("PLAYERS", playCountStr, data::inMatch ? IM_COL32(50, 230, 100, 255) : theme_orchid::ColPrimaryLight);

                const char* matchStateStr = data::inMatch.load() ? "ACTIVE" : "STANDBY";
                DrawDiagTile("MATCH", matchStateStr, data::inMatch.load() ? IM_COL32(50, 230, 100, 255) : theme_orchid::ColTextMuted);

                ImGui::Dummy(ImVec2(0, 8 * ui_scale));
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 40, 60, 200));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(240, 60, 80, 240));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 20, 40, 255));
                if (ImGui::Button("EXIT OVERLAY", ImVec2(-1, 38.0f * ui_scale))) {
                    g_should_exit = true;
                }
                ImGui::PopStyleColor(3);

                handleTouchScroll();
            }
            ImGui::EndChild();
        }

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(2);

        if (fontPushed) {
            ImGui::PopFont();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(20);
}

} // namespace ui::menu
