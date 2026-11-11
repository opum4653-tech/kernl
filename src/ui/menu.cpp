#include "menu.hpp"
#include "imgui.h"
#include "Android_draw/draw.h"
#include "Android_touch/Touch.hpp"
#include "other/logger.hpp"
#include "driver.h"
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
extern int abs_ScreenX;
extern int abs_ScreenY;
extern int native_window_screen_x;
extern int native_window_screen_y;
extern int 选择值;

extern int selectedBoneIndex;
extern bool Ignorknock;
extern bool IgnoreBot;
extern int SilentAimPercentage;
extern float SpeedMultiplier;

bool g_ShowMenu = false;
bool g_AimMagnetUi = false;

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
    ImU32 colPrimary      = theme_orchid::ColPrimary;
    ImU32 colPrimaryLight = theme_orchid::ColPrimaryLight;
    ImU32 colDarkBg       = IM_COL32(18, 14, 26, 245);
    ImU32 colWhite        = IM_COL32(255, 255, 255, 255);
    ImU32 colCyan         = IM_COL32(0, 240, 255, 200);

    float t = (float)ImGui::GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * 3.0f);

    dl->AddCircleFilled(center, radius + 8.0f * pulse, (colPrimary & 0x00FFFFFF) | ((int)(45 * pulse) << 24), 48);
    dl->AddCircleFilled(center, radius + 4.0f, (colPrimary & 0x00FFFFFF) | ((int)(85 * pulse) << 24), 48);
    dl->AddCircleFilled(center, radius, colDarkBg, 64);
    dl->AddCircle(center, radius, colPrimary, 64, 2.5f);
    dl->AddCircle(center, radius - 2.5f, (colPrimaryLight & 0x00FFFFFF) | (180 << 24), 64, 1.2f);
    dl->AddCircle(center, radius * 0.76f, (colPrimary & 0x00FFFFFF) | (140 << 24), 48, 1.2f);

    float spinSpeed = spinning ? 3.5f : 1.8f;
    float ang = t * spinSpeed;
    for (int i = 0; i < 4; i++) {
        float a1 = ang + i * (3.14159265f * 0.5f);
        float a2 = a1 + 0.45f;
        ImVec2 pa(center.x + cosf(a1) * (radius - 4.5f), center.y + sinf(a1) * (radius - 4.5f));
        ImVec2 pb(center.x + cosf(a2) * (radius - 4.5f), center.y + sinf(a2) * (radius - 4.5f));
        dl->AddLine(pa, pb, (i % 2 == 0) ? colPrimaryLight : colCyan, 2.2f);
    }

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

static const char* get_logo_pos_file() {
    static std::string path = "";
    if (!path.empty()) return path.c_str();
    const char* dirs[] = {
        "/data/data/com.andorid.ninmods/files/logs",
        "/data/user/0/com.andorid.ninmods/files/logs",
        "/sdcard/Android/data/com.andorid.ninmods/files/logs"
    };
    for (const char* dir : dirs) {
        mkdir(dir, 0777);
        if (access(dir, W_OK) == 0) {
            path = std::string(dir) + "/ninmods_logo_pos.txt";
            return path.c_str();
        }
    }
    mkdir("/data/data/com.andorid.ninmods/files", 0777);
    mkdir("/data/data/com.andorid.ninmods/files/logs", 0777);
    path = "/data/data/com.andorid.ninmods/files/logs/ninmods_logo_pos.txt";
    return path.c_str();
}

static ImVec2 s_logo_pos = ImVec2(-1.0f, -1.0f);

namespace ui::menu {

void save_logo_pos() {
    std::ofstream f(get_logo_pos_file());
    if (f.is_open()) {
        f << s_logo_pos.x << " " << s_logo_pos.y;
        f.close();
    }
}

void load_logo_pos() {
    std::ifstream f(get_logo_pos_file());
    if (f.is_open()) {
        float x, y;
        if (f >> x >> y) {
            s_logo_pos = ImVec2(x, y);
        }
        f.close();
    }
}

void render() {
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

    // 1. FLOATING CIRCULAR 'N' LOGO
    const float logoRadius = 24.0f * ui_scale;
    const float logoMargin = 12.0f;

    if (s_logo_pos.x < 0.0f || pill_needs_reposition) {
        s_logo_pos = ImVec2(logoMargin + 20.0f, realH * 0.45f);
        pill_needs_reposition = false;
    }

    s_logo_pos.x = std::max(logoMargin, std::min(s_logo_pos.x, realW - logoRadius * 2.0f - logoMargin));
    s_logo_pos.y = std::max(logoMargin, std::min(s_logo_pos.y, realH - logoRadius * 2.0f - logoMargin));

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

        if (clicked) {
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

        if (deactivated && !s_moved_drag) {
            g_ShowMenu = !g_ShowMenu;
            s_moved_drag = false;
            s_is_dragging = false;
        } else if (deactivated) {
            s_moved_drag = false;
            s_is_dragging = false;
        }

        DrawCircularLogo(dl, center, logoRadius, s_is_dragging);

        bool connected = (game_pid > 0 && lib_base > 0);
        ImU32 stCol = connected ? IM_COL32(0, 240, 140, 255) : IM_COL32(255, 90, 90, 255);
        ImVec2 stDot(center.x + logoRadius * 0.68f, center.y - logoRadius * 0.68f);
        dl->AddCircleFilled(stDot, 6.0f, (stCol & 0x00FFFFFF) | (90 << 24));
        dl->AddCircleFilled(stDot, 4.0f, (stCol & 0x00FFFFFF) | (180 << 24));
        dl->AddCircleFilled(stDot, 2.5f, stCol);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    if (!g_ShowMenu) {
        return;
    }

    // 2. MAIN UNIFIED ELECTRIC ORCHID GLASS MENU
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
        bool connected = (game_pid > 0 && lib_base > 0);
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

                ImGui::Checkbox("Box ESP", &showBox);
                ImGui::Checkbox("Health Bar", &showHealth);
                ImGui::Checkbox("Laser Line", &showLine);
                ImGui::Checkbox("Skeleton ESP", &showSkeleton);
                ImGui::Checkbox("Player Name", &showName);
                ImGui::Checkbox("Distance", &showDistance);
                ImGui::Checkbox("Player Count", &showCount);
                ImGui::Checkbox("360 Alert", &is3600);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Colors & Customization
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightESP", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "COLORS & STYLES");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 120, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                const char* colorModes[] = { "Custom", "Health", "Distance", "Rainbow", "Cyan",
                                             "Green", "Purple", "Orange", "Red", "White" };

                ImGui::Text("Box Color Mode");
                ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##BoxColorMode", &boxColorMode, colorModes, IM_ARRAYSIZE(colorModes));

                if (boxColorMode == 0) {
                    ImGui::Text("Custom Box Color");
                    ImGui::ColorEdit4("##BoxCustomCol", &boxCustomColor.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
                }

                ImGui::Dummy(ImVec2(0, 6));
                ImGui::Text("Line Color Mode");
                ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##LineColorMode", &lineColorMode, colorModes, IM_ARRAYSIZE(colorModes));

                if (lineColorMode == 0) {
                    ImGui::Text("Custom Line Color");
                    ImGui::ColorEdit4("##LineCustomCol", &lineCustomColor.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
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
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "AIMBOT CONTROLS");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 lpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(lpos.x, lpos.y - 4), ImVec2(lpos.x + 130, lpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                ImGui::Checkbox("Enable Aimbot", &Aimbot);
                ImGui::Checkbox("Aim Lock (Instant Snap)", &AimLock);
                ImGui::Checkbox("Silent Aim", &AimSilent);
                ImGui::Checkbox("Visible Check", &AimVisible);
                ImGui::Checkbox("Aim Magnet", &g_AimMagnetUi);

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::Checkbox("Ignore Knocked", &Ignorknock);
                ImGui::Checkbox("Ignore AI Bots", &IgnoreBot);

                handleTouchScroll();
            }
            ImGui::EndChild();

            // Right Column: Sliders & Modes
            ImGui::SetCursorPos(ImVec2(contentX + colW + colGap, contentY));
            if (ImGui::BeginChild("##ColRightAim", ImVec2(colW, contentH), true)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.80f, 1.0f), "AIM CONFIGURATION");
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 rpos = ImGui::GetCursorScreenPos();
                cdl->AddLine(ImVec2(rpos.x, rpos.y - 4), ImVec2(rpos.x + 140, rpos.y - 4), theme_orchid::ColPrimary, 2.0f);
                ImGui::Dummy(ImVec2(0, 4));

                const char* bones[] = { "Head", "Chest", "Hip" };
                ImGui::Text("Target Aim Bone");
                ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##TargetBone", &selectedBoneIndex, bones, IM_ARRAYSIZE(bones));

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::Text("Aim FOV Range (%.0f deg)", AimFov);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##AimFovSlider", &AimFov, 0.0f, 360.0f, "%.0f deg");

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::Text("Silent Aim Hit Rate (%d%%)", SilentAimPercentage);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderInt("##SilentRateSlider", &SilentAimPercentage, 0, 100, "%d%%");

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

                ImGui::Checkbox("Speed Hack", &SpeedHackV2);
                if (SpeedHackV2) {
                    ImGui::SetNextItemWidth(-1);
                    ImGui::SliderFloat("##SpeedMult", &SpeedMultiplier, 1.0f, 5.0f, "%.1fx");
                }

                ImGui::Checkbox("No Recoil", &NoRecoil);
                ImGui::Checkbox("No Reload", &NoReload);
                ImGui::Checkbox("Ghost Hack", &GhostHack);

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
                snprintf(pidStr, sizeof(pidStr), "%d", game_pid);

                char libStr[32];
                snprintf(libStr, sizeof(libStr), "0x%lx", (unsigned long)lib_base);

                ImGui::TextDisabled("STATUS:");
                ImGui::SameLine(100);
                ImGui::TextColored(connected ? ImVec4(0, 1, 0.5f, 1) : ImVec4(1, 0.3f, 0.3f, 1), connected ? "READY" : "OFFLINE");

                ImGui::TextDisabled("PID:");
                ImGui::SameLine(100);
                ImGui::Text(game_pid > 0 ? pidStr : "NONE");

                ImGui::TextDisabled("LIB BASE:");
                ImGui::SameLine(100);
                ImGui::Text(lib_base > 0 ? libStr : "NONE");

                ImGui::TextDisabled("DRIVER:");
                ImGui::SameLine(100);
                ImGui::TextColored(ImVec4(0, 0.85f, 0.9f, 1), "%s", dName);

                ImGui::Dummy(ImVec2(0, 8));
                ImGui::Text("Theme Accent");
                for (int i = 0; i < 6; i++) {
                    ImGui::PushID(800 + i);
                    if (ImGui::ColorButton("##th", g_ThemeSwatches[i], 0, ImVec2(28, 28))) {
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
                if (ImGui::Button("EXIT OVERLAY", ImVec2(-1, 36.0f))) {
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
