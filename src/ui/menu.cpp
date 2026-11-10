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

float g_ThemeColorArr[4] = { 1.00f, 0.20f, 0.20f, 1.00f };

float g_GlowIntensity = 1.0f;
float g_MenuWidth     = 1250.0f;
float g_MenuHeight    = 850.0f;
bool  g_darkMode      = true;
int   g_CurrentTab    = 0;

bool  g_ShowMenu      = false;
bool  g_AimMagnetUi   = false;

float  g_LogoX = 30.0f;
float  g_LogoY = 0.38f;
bool   g_LogoDragging = false;
ImVec2 g_LogoDragOffset(0, 0);
ImVec2 g_LogoDragStart(0, 0);
float  g_LogoSize = 130.0f;
float  g_LogoDragThreshold = 8.0f;

static bool s_logoJustMoved = false;

extern ImFont* g_IconFont;
extern ImFont* g_BoldFont;

static const ImVec4 g_ThemeSwatches[6] = {
    ImVec4(1.00f, 0.20f, 0.20f, 1.00f),
    ImVec4(0.00f, 0.85f, 0.45f, 1.00f),
    ImVec4(0.30f, 0.55f, 1.00f, 1.00f),
    ImVec4(1.00f, 0.30f, 0.85f, 1.00f),
    ImVec4(1.00f, 0.75f, 0.00f, 1.00f),
    ImVec4(0.00f, 0.85f, 0.90f, 1.00f),
};

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

namespace ui::menu {

void save_logo_pos() {
    std::ofstream f(get_logo_pos_file());
    if (f.is_open()) {
        f << g_LogoX << " " << g_LogoY << " " << g_LogoSize;
        f.close();
    }
}

void load_logo_pos() {
    std::ifstream f(get_logo_pos_file());
    if (f.is_open()) {
        float x, y, s;
        if (f >> x >> y) {
            g_LogoX = x;
            g_LogoY = y;
            if (f >> s) g_LogoSize = s;
        }
        f.close();
    }
}

}
namespace ui::menu {

static void ApplyCustomTheme(float scale_y = 1.0f) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 0.0f;
    style.FrameBorderSize  = 0.0f;
    style.PopupBorderSize  = 0.0f;
    style.TabBorderSize    = 0.0f;
    style.WindowRounding   = 14.0f;
    style.ChildRounding    = 10.0f;
    style.FrameRounding    = 6.0f;

    float g2 = 20.0f * scale_y;
    style.ItemSpacing = ImVec2(12.0f, std::max(1.0f, std::min(g2, 26.0f)));
    style.FramePadding = ImVec2(10.0f, std::max(4.0f, 8.0f * scale_y));

    if (!g_darkMode) {
        colors[ImGuiCol_WindowBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ChildBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]       = ImVec4(0.98f, 0.98f, 1.00f, 1.00f);
        colors[ImGuiCol_Border]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_Text]          = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
        colors[ImGuiCol_TextDisabled]  = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_Button]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
        colors[ImGuiCol_ButtonActive]  = ImVec4(0.87f, 0.87f, 0.90f, 1.00f);
        colors[ImGuiCol_FrameBg]       = ImVec4(0.93f, 0.93f, 0.95f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]= ImVec4(0.89f, 0.89f, 0.92f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.86f, 0.86f, 0.89f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ChildBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]       = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
        colors[ImGuiCol_Border]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_Text]          = ImVec4(0.95f, 0.95f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]  = ImVec4(0.45f, 0.45f, 0.48f, 1.00f);
        colors[ImGuiCol_Button]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonActive]  = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBg]       = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]= ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
    }

    colors[ImGuiCol_SliderGrab]       = g_ThemeColor;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(g_ThemeColorArr[0]*0.9f,
                                               g_ThemeColorArr[1]*0.9f,
                                               g_ThemeColorArr[2]*0.9f,
                                               g_ThemeColorArr[3]);
    colors[ImGuiCol_Header]        = g_ThemeColor;
    colors[ImGuiCol_HeaderHovered] = g_ThemeColor;
    colors[ImGuiCol_HeaderActive]  = g_ThemeColor;
}

static void RenderFloatingLogo() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen = io.DisplaySize;

    const float logoSize = g_LogoSize;
    const float margin   = 10.0f;

    ImVec2 logoPos(g_LogoX, screen.y * g_LogoY);
    logoPos.x = std::max(margin, std::min(logoPos.x, screen.x - logoSize - margin));
    logoPos.y = std::max(margin, std::min(logoPos.y, screen.y - logoSize - margin));

    g_LogoX = logoPos.x;
    g_LogoY = screen.y > 0.0f ? (logoPos.y / screen.y) : 0.38f;

    ImGui::SetNextWindowPos(logoPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(logoSize, logoSize), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("##NINMODS_LOGO_BTN", nullptr, flags)) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 wp  = ImGui::GetWindowPos();
        ImVec2 ctr = ImVec2(wp.x + logoSize * 0.5f, wp.y + logoSize * 0.5f);
        float  rad = logoSize * 0.44f;

        ImGui::SetCursorScreenPos(wp);
        bool clicked = ImGui::InvisibleButton("##logoClick", ImVec2(logoSize, logoSize));

        const bool hovered     = ImGui::IsItemHovered();
        const bool active      = ImGui::IsItemActive();
        const bool deactivated = ImGui::IsItemDeactivated();
        const ImVec2 mouse     = ImGui::GetMousePos();

        if (ImGui::IsItemActivated()) {
            g_LogoDragStart  = mouse;
            g_LogoDragOffset = ImVec2(mouse.x - logoPos.x, mouse.y - logoPos.y);
            g_LogoDragging   = false;
            s_logoJustMoved  = false;
        }

        if (active) {
            const float dx = mouse.x - g_LogoDragStart.x;
            const float dy = mouse.y - g_LogoDragStart.y;
            if (!g_LogoDragging && sqrtf(dx * dx + dy * dy) > g_LogoDragThreshold) {
                g_LogoDragging  = true;
                s_logoJustMoved = true;
            }

            if (g_LogoDragging) {
                float newX = mouse.x - g_LogoDragOffset.x;
                float newY = mouse.y - g_LogoDragOffset.y;

                newX = std::max(margin, std::min(newX, screen.x - logoSize - margin));
                newY = std::max(margin, std::min(newY, screen.y - logoSize - margin));

                g_LogoX = newX;
                g_LogoY = screen.y > 0.0f ? (newY / screen.y) : 0.38f;
            }
        }

        static double s_last_toggle_time = 0.0;
        double cur_time = ImGui::GetTime();

        if (clicked || (deactivated && !s_logoJustMoved)) {
            const float dx = mouse.x - g_LogoDragStart.x;
            const float dy = mouse.y - g_LogoDragStart.y;
            if (sqrtf(dx * dx + dy * dy) <= g_LogoDragThreshold) {
                if (cur_time - s_last_toggle_time > 0.20) {
                    g_ShowMenu = !g_ShowMenu;
                    s_last_toggle_time = cur_time;
                }
            }
            s_logoJustMoved = false;
            g_LogoDragging  = false;
        } else if (deactivated) {
            if (g_LogoDragging || s_logoJustMoved) {
                ui::menu::save_logo_pos();
            }
            s_logoJustMoved = false;
            g_LogoDragging  = false;
        }

    ImU32 accent = ImGui::ColorConvertFloat4ToU32(g_ThemeColor);
    ImU32 cyan   = IM_COL32(80, 220, 255, 255);
    ImU32 purple = IM_COL32(180, 120, 255, 255);
    ImU32 white  = IM_COL32(255, 255, 255, 255);

    float t     = (float)ImGui::GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * 2.5f);
    float scale = (hovered || g_LogoDragging) ? (1.0f + 0.08f * pulse) : 1.0f;
    float r     = rad * scale;

    draw->AddCircleFilled(ctr, r + 22.0f, (accent & 0x00FFFFFF) | ((int)(45 * pulse) << 24), 64);
    draw->AddCircleFilled(ctr, r + 12.0f, (accent & 0x00FFFFFF) | ((int)(75 * pulse) << 24), 56);
    draw->AddCircleFilled(ctr, r + 5.0f,  (accent & 0x00FFFFFF) | ((int)(90 * pulse) << 24), 48);

    draw->AddCircleFilled(ctr, r,        IM_COL32(15, 15, 24, 252), 64);
    draw->AddCircleFilled(ctr, r - 3.0f, IM_COL32(22, 22, 36, 250), 64);

    draw->AddCircle(ctr, r,        accent, 64, 4.5f);
    draw->AddCircle(ctr, r - 3.0f, (cyan   & 0x00FFFFFF) | (160 << 24), 64, 2.0f);
    draw->AddCircle(ctr, r - 7.0f, (purple & 0x00FFFFFF) | (100 << 24), 60, 1.5f);

    draw->AddCircle(ctr, r * 0.82f, (accent & 0x00FFFFFF) | (200 << 24), 48, 2.0f);
    draw->AddCircle(ctr, r * 0.66f, (cyan   & 0x00FFFFFF) | (150 << 24), 44, 1.5f);
    draw->AddCircle(ctr, r * 0.50f, (purple & 0x00FFFFFF) | (130 << 24), 40, 1.5f);

    float spin = g_LogoDragging ? 3.2f : 1.6f;
    float ang  = t * spin;
    for (int i = 0; i < 6; i++) {
        float a1 = ang + i * (3.14159265f * 2.0f / 6.0f);
        float a2 = a1 + 0.42f;
        ImVec2 pa(ctr.x + cosf(a1) * (r - 6.0f), ctr.y + sinf(a1) * (r - 6.0f));
        ImVec2 pb(ctr.x + cosf(a2) * (r - 6.0f), ctr.y + sinf(a2) * (r - 6.0f));
        ImU32 arcCol = (i % 2 == 0) ? cyan : accent;
        draw->AddLine(pa, pb, arcCol, 4.0f);
    }

    ImFont* bold = g_BoldFont ? g_BoldFont : ImGui::GetFont();
    const char* txt = "N";
    float txtSize = 54.0f;
    ImVec2 tsz = bold->CalcTextSizeA(txtSize, FLT_MAX, 0, txt);

    draw->AddText(bold, txtSize, ImVec2(ctr.x - tsz.x * 0.5f, ctr.y - tsz.y * 0.5f - 3.0f), (white & 0x00FFFFFF) | (80 << 24), txt);
    draw->AddText(bold, txtSize, ImVec2(ctr.x - tsz.x * 0.5f, ctr.y - tsz.y * 0.5f - 4.0f), (white & 0x00FFFFFF) | (140 << 24), txt);
    draw->AddText(bold, txtSize, ImVec2(ctr.x - tsz.x * 0.5f, ctr.y - tsz.y * 0.5f - 5.0f), white, txt);

    bool connected = (game_pid > 0 && lib_base > 0);
    ImU32 stCol = connected ? IM_COL32(0, 240, 140, 255) : IM_COL32(255, 90, 90, 255);
    ImVec2 stDot(ctr.x + r * 0.68f, ctr.y - r * 0.68f);

    draw->AddCircleFilled(stDot, 12.0f, (stCol & 0x00FFFFFF) | (100 << 24));
    draw->AddCircleFilled(stDot,  7.0f, (stCol & 0x00FFFFFF) | (150 << 24));
    draw->AddCircleFilled(stDot,  4.5f, stCol);

    if (g_LogoDragging) {
        const char* moveTxt = "MOVE";
        float mSize = 14.0f;
        ImVec2 mSz = bold->CalcTextSizeA(mSize, FLT_MAX, 0, moveTxt);
        float iy = ctr.y + r + 6.0f;
        draw->AddRectFilled(ImVec2(ctr.x - mSz.x * 0.5f - 12.0f, iy), ImVec2(ctr.x + mSz.x * 0.5f + 12.0f, iy + 24.0f), IM_COL32(0, 0, 0, 220), 6.0f);
        draw->AddRect(ImVec2(ctr.x - mSz.x * 0.5f - 12.0f, iy), ImVec2(ctr.x + mSz.x * 0.5f + 12.0f, iy + 24.0f), cyan, 6.0f, 0, 1.5f);
        draw->AddText(bold, mSize, ImVec2(ctr.x - mSz.x * 0.5f, iy + 4.0f), cyan, moveTxt);
    } else if (hovered) {
        const char* hintTxt = "TAP=OPEN  DRAG=MOVE";
        float hSize = 11.0f;
        ImVec2 hSz = bold->CalcTextSizeA(hSize, FLT_MAX, 0, hintTxt);
        float iy = ctr.y + r + 6.0f;
        draw->AddRectFilled(ImVec2(ctr.x - hSz.x * 0.5f - 10.0f, iy), ImVec2(ctr.x + hSz.x * 0.5f + 10.0f, iy + 20.0f), IM_COL32(0, 0, 0, 180), 5.0f);
        draw->AddText(bold, hSize, ImVec2(ctr.x - hSz.x * 0.5f, iy + 4.0f), (cyan & 0x00FFFFFF) | (200 << 24), hintTxt);
    }

    }
    ImGui::End();
    ImGui::PopStyleVar();
}

static void RenderMenu() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen = io.DisplaySize;

    g_MenuWidth  = std::max(480.0f, std::min(1180.0f, screen.x * 0.94f));
    g_MenuHeight = std::max(360.0f, std::min(820.0f, screen.y * 0.94f));
    if (g_MenuWidth > screen.x) g_MenuWidth = screen.x;
    if (g_MenuHeight > screen.y) g_MenuHeight = screen.y;

    float scale_y = g_MenuHeight / 900.0f;
    scale_y = std::max(0.65f, std::min(scale_y, 1.10f));

    static bool s_last_dark = !g_darkMode;
    static float s_last_scale = -1.0f;
    static ImVec4 s_last_accent = ImVec4(0, 0, 0, 0);

    bool need_update = (s_last_dark != g_darkMode) ||
                       (fabsf(s_last_scale - scale_y) > 0.01f) ||
                       (s_last_accent.x != g_ThemeColorArr[0] ||
                        s_last_accent.y != g_ThemeColorArr[1] ||
                        s_last_accent.z != g_ThemeColorArr[2] ||
                        s_last_accent.w != g_ThemeColorArr[3]);
    if (need_update) {
        ApplyCustomTheme(scale_y);
        s_last_dark = g_darkMode;
        s_last_scale = scale_y;
        s_last_accent = ImVec4(g_ThemeColorArr[0], g_ThemeColorArr[1], g_ThemeColorArr[2], g_ThemeColorArr[3]);
    }

    static ImVec2 s_menu_pos(-1.0f, -1.0f);
    if (s_menu_pos.x < 0.0f || s_menu_pos.x > screen.x || s_menu_pos.y > screen.y) {
        s_menu_pos = ImVec2((screen.x - g_MenuWidth) * 0.5f, (screen.y - g_MenuHeight) * 0.5f);
    }
    s_menu_pos.x = std::max(0.0f, std::min(s_menu_pos.x, screen.x - g_MenuWidth));
    s_menu_pos.y = std::max(0.0f, std::min(s_menu_pos.y, screen.y - g_MenuHeight));

    ImGui::SetNextWindowSize(ImVec2(g_MenuWidth, g_MenuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowPos(s_menu_pos, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("NINMODS_CORE", nullptr, flags)) {

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    float  W  = ws.x;
    float  H  = ws.y;

    ImU32 bgDeep     = g_darkMode ? IM_COL32(8, 8, 16, 252)    : IM_COL32(242, 244, 250, 252);
    ImU32 bgCard     = g_darkMode ? IM_COL32(20, 20, 32, 250)  : IM_COL32(255, 255, 255, 250);
    ImU32 bgInset    = g_darkMode ? IM_COL32(30, 30, 46, 255)  : IM_COL32(232, 236, 245, 255);
    ImU32 border     = g_darkMode ? IM_COL32(60, 60, 88, 255)  : IM_COL32(195, 200, 218, 255);
    ImU32 borderGlow = g_darkMode ? IM_COL32(90, 90, 130, 255) : IM_COL32(165, 175, 205, 255);
    ImU32 accent     = ImGui::ColorConvertFloat4ToU32(g_ThemeColor);
    ImU32 accent2    = g_darkMode ? IM_COL32(120, 200, 255, 255) : IM_COL32(30, 130, 230, 255);
    ImU32 accent3    = g_darkMode ? IM_COL32(255, 180, 80, 255)  : IM_COL32(210, 120, 20, 255);

    ImU32 textMain   = g_darkMode ? IM_COL32(245, 245, 255, 255) : IM_COL32(20, 24, 36, 255);
    ImU32 textDim    = g_darkMode ? IM_COL32(160, 160, 190, 255) : IM_COL32(95, 105, 125, 255);

    ImU32 green      = g_darkMode ? IM_COL32(0, 240, 140, 255)   : IM_COL32(0, 180, 100, 255);
    ImU32 redColor   = g_darkMode ? IM_COL32(255, 90, 90, 255)   : IM_COL32(220, 50, 50, 255);
    ImU32 purple     = g_darkMode ? IM_COL32(180, 120, 255, 255) : IM_COL32(130, 70, 220, 255);
    ImU32 cyan       = g_darkMode ? IM_COL32(80, 220, 255, 255)  : IM_COL32(10, 150, 210, 255);
    ImU32 yellow     = g_darkMode ? IM_COL32(255, 220, 100, 255) : IM_COL32(210, 160, 10, 255);
    ImU32 pink       = g_darkMode ? IM_COL32(255, 120, 200, 255) : IM_COL32(210, 60, 160, 255);

    ImVec2 p1 = wp;
    ImVec2 p2 = ImVec2(wp.x + W, wp.y + H);

    draw->AddRectFilled(ImVec2(p1.x - 5, p1.y - 5), ImVec2(p2.x + 5, p2.y + 5), (accent & 0x00FFFFFF) | (25 << 24), 22.0f);
    draw->AddRectFilled(ImVec2(p1.x + 5, p1.y + 8), ImVec2(p2.x + 5, p2.y + 8), IM_COL32(0, 0, 0, g_darkMode ? 140 : 45), 18.0f);
    draw->AddRectFilled(p1, p2, bgDeep, 18.0f);
    draw->AddRect(ImVec2(p1.x + 1, p1.y + 1), ImVec2(p2.x - 1, p2.y - 1), borderGlow, 17.0f, 0, 1.6f);
    draw->AddRectFilledMultiColor(ImVec2(p1.x + 20, p1.y), ImVec2(p2.x - 20, p1.y + 3), accent, accent2, accent2, accent);

    float pad   = std::max(16.0f, 34.0f * scale_y);
    float headY = wp.y + pad * 0.85f;
    ImFont* titleFont = g_BoldFont ? g_BoldFont : ImGui::GetFont();

    const char* title = "M O H A M M A D";
    float titleSize = 30.0f * scale_y;
    float chW = 20.0f * scale_y;
    float tx = wp.x + pad;
    ImU32 grad[4] = { accent, accent2, purple, pink };
    for (int i = 0; title[i]; i++) {
        if (title[i] == ' ') { tx += chW * 0.35f; continue; }
        char buf[2] = { title[i], 0 };
        ImU32 col = grad[i % 4];
        draw->AddText(titleFont, titleSize, ImVec2(tx, headY), col, buf);
        tx += chW;
    }

    float lineY = headY + 40.0f * scale_y;
    for (int i = 0; i < 12; i++) {
        float x = wp.x + pad + i * (17.0f * scale_y);
        ImU32 dc = (i % 2 == 0) ? accent : accent2;
        draw->AddLine(ImVec2(x, lineY), ImVec2(x + 9.0f * scale_y, lineY), dc, 2.0f);
    }

    float closeSize = std::max(30.0f, 42.0f * scale_y);
    float headerH   = std::max(45.0f, 65.0f * scale_y);
    float dragBarW  = std::max(100.0f, W - pad - closeSize - 20.0f * scale_y);
    ImGui::SetCursorScreenPos(wp);
    ImGui::InvisibleButton("##NinmodsHeaderDragBar", ImVec2(dragBarW, headerH));
    if (ImGui::IsItemActive()) {
        s_menu_pos.x += io.MouseDelta.x;
        s_menu_pos.y += io.MouseDelta.y;
        s_menu_pos.x = std::max(0.0f, std::min(s_menu_pos.x, screen.x - g_MenuWidth));
        s_menu_pos.y = std::max(0.0f, std::min(s_menu_pos.y, screen.y - g_MenuHeight));
    }

    ImVec2 closePos(wp.x + W - pad - closeSize, headY - 6.0f * scale_y);

    ImGui::SetCursorScreenPos(closePos);
    ImGui::PushID("ninmods_close");
    if (ImGui::InvisibleButton("##close", ImVec2(closeSize, closeSize))) {
        g_ShowMenu = false;
    }
    bool closeHov = ImGui::IsItemHovered();
    ImGui::PopID();

    ImVec2 closeCenter(closePos.x + closeSize * 0.5f, closePos.y + closeSize * 0.5f);
    float  closeRad = closeSize * 0.5f;
    ImU32 closeCol = closeHov ? IM_COL32(255, 90, 90, 255) : IM_COL32(200, 60, 60, 220);

    if (closeHov) {
        draw->AddCircleFilled(closeCenter, closeRad + 4.0f, (closeCol & 0x00FFFFFF) | (60 << 24), 32);
    }
    draw->AddCircleFilled(closeCenter, closeRad, closeCol, 32);
    draw->AddCircle(closeCenter, closeRad, IM_COL32(255, 255, 255, 100), 32, 1.4f);

    float xOff = closeRad * 0.42f;
    ImU32 xCol = IM_COL32(255, 255, 255, 255);
    draw->AddLine(ImVec2(closeCenter.x - xOff, closeCenter.y - xOff), ImVec2(closeCenter.x + xOff, closeCenter.y + xOff), xCol, 3.0f);
    draw->AddLine(ImVec2(closeCenter.x + xOff, closeCenter.y - xOff), ImVec2(closeCenter.x - xOff, closeCenter.y + xOff), xCol, 3.0f);

    float badgeW = 130.0f * scale_y;
    float badgeH = 36.0f * scale_y;
    ImVec2 bP1 = ImVec2(closePos.x - badgeW - 14.0f * scale_y, headY - 3.0f * scale_y);
    ImVec2 bP2 = ImVec2(bP1.x + badgeW, bP1.y + badgeH);

    bool connected = (game_pid > 0 && lib_base > 0);
    ImU32 liveCol = connected ? green : redColor;

    draw->AddRectFilled(bP1, bP2, (liveCol & 0x00FFFFFF) | (40 << 24), badgeH * 0.5f);
    draw->AddRect(bP1, bP2, (liveCol & 0x00FFFFFF) | (200 << 24), badgeH * 0.5f, 0, 1.6f);

    float pulse = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 3.0f);
    ImVec2 dotC = ImVec2(bP1.x + 22.0f * scale_y, bP1.y + badgeH * 0.5f);
    draw->AddCircleFilled(dotC, 10.0f * scale_y, (liveCol & 0x00FFFFFF) | ((int)(90 * pulse) << 24));
    draw->AddCircleFilled(dotC, 5.5f * scale_y, liveCol);

    const char* badgeTxt = connected ? "ACTIVE" : "OFFLINE";
    ImVec2 bSz = titleFont->CalcTextSizeA(15.0f * scale_y, FLT_MAX, 0, badgeTxt);
    draw->AddText(titleFont, 15.0f * scale_y, ImVec2(dotC.x + 14.0f * scale_y, bP1.y + (badgeH - bSz.y) * 0.5f), liveCol, badgeTxt);

    float heroY = lineY + 32.0f * scale_y;
    float nxW = 250.0f * scale_y;
    float nxH = 100.0f * scale_y;
    ImVec2 nxP1 = ImVec2(wp.x + pad, heroY);
    ImVec2 nxP2 = ImVec2(nxP1.x + nxW, nxP1.y + nxH);

    draw->AddRectFilled(ImVec2(nxP1.x - 3, nxP1.y - 3), ImVec2(nxP2.x + 3, nxP2.y + 3), (accent & 0x00FFFFFF) | (40 << 24), 13.0f);
    draw->AddRectFilled(nxP1, nxP2, bgCard, 12.0f);
    draw->AddRect(nxP1, nxP2, accent, 12.0f, 0, 2.0f);

    float tick = 16.0f * scale_y;
    draw->AddLine(nxP1, ImVec2(nxP1.x + tick, nxP1.y), accent2, 3.0f);
    draw->AddLine(nxP1, ImVec2(nxP1.x, nxP1.y + tick), accent2, 3.0f);
    draw->AddLine(ImVec2(nxP2.x, nxP2.y), ImVec2(nxP2.x - tick, nxP2.y), accent2, 3.0f);
    draw->AddLine(ImVec2(nxP2.x, nxP2.y), ImVec2(nxP2.x, nxP2.y - tick), accent2, 3.0f);

    const char* nxs = "N I N M O D S";
    float brandSize = 22.0f * scale_y;
    ImVec2 nxsSz = titleFont->CalcTextSizeA(brandSize, FLT_MAX, 0, nxs);
    draw->AddText(titleFont, brandSize, ImVec2(nxP1.x + (nxW - nxsSz.x) * 0.5f, nxP1.y + (nxH - nxsSz.y) * 0.5f), cyan, nxs);

    float scX = nxP2.x + 46.0f * scale_y;
    draw->AddText(titleFont, 20.0f * scale_y, ImVec2(scX, heroY + 8.0f * scale_y), textMain, "SYSTEM CORE");
    draw->AddRectFilled(ImVec2(scX, heroY + 34.0f * scale_y), ImVec2(scX + 170.0f * scale_y, heroY + 36.5f * scale_y), accent3, 2.0f);

    ImVec2 cDot = ImVec2(scX + 10.0f * scale_y, heroY + 62.0f * scale_y);
    draw->AddCircleFilled(cDot, 7.0f * scale_y, (liveCol & 0x00FFFFFF) | (80 << 24));
    draw->AddCircleFilled(cDot, 4.5f * scale_y, liveCol);
    draw->AddText(titleFont, 15.0f * scale_y, ImVec2(cDot.x + 16.0f * scale_y, cDot.y - 10.0f * scale_y), green, connected ? "CONNECTED" : "DISCONNECTED");

    float tabY = heroY + nxH + 26.0f * scale_y;
    float tabH = 52.0f * scale_y;
    float tabW = W - pad * 2;
    ImVec2 tP1 = ImVec2(wp.x + pad, tabY);
    ImVec2 tP2 = ImVec2(tP1.x + tabW, tP1.y + tabH);

    draw->AddRectFilled(tP1, tP2, bgInset, 10.0f);
    draw->AddRect(tP1, tP2, border, 10.0f, 0, 1.4f);

    const char* tabNames[4] = { "AIM", "VISUALS", "MISC", "CONFIG" };
    ImU32 tabColors[4]      = { redColor, cyan, yellow, purple };
    int tabCount = 4;
    float slotW = tabW / (float)tabCount;

    for (int i = 0; i < tabCount; i++) {
        ImVec2 sP1 = ImVec2(tP1.x + slotW * i, tP1.y);
        ImVec2 sP2 = ImVec2(sP1.x + slotW, tP2.y);
        bool active = (g_CurrentTab == i);
        ImU32 tabCol = tabColors[i];

        ImGui::SetCursorScreenPos(sP1);
        ImGui::PushID(9000 + i);
        if (ImGui::InvisibleButton("##tab", ImVec2(slotW, tabH))) {
            g_CurrentTab = i;
        }
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        if (active) {
            draw->AddRectFilled(ImVec2(sP1.x + 4, sP1.y + 4), ImVec2(sP2.x - 4, sP2.y - 4), (tabCol & 0x00FFFFFF) | (90 << 24), 8.0f);
            draw->AddRectFilled(ImVec2(sP1.x + 6, sP1.y + 6), ImVec2(sP2.x - 6, sP2.y - 6), (tabCol & 0x00FFFFFF) | (180 << 24), 6.0f);
            draw->AddRect(ImVec2(sP1.x + 6, sP1.y + 6), ImVec2(sP2.x - 6, sP2.y - 6), tabCol, 6.0f, 0, 1.8f);
            draw->AddRectFilled(ImVec2(sP1.x + 6, sP2.y - 4), ImVec2(sP2.x - 6, sP2.y - 1), tabCol, 2.0f);
        } else if (hovered) {
            draw->AddRectFilled(ImVec2(sP1.x + 6, sP1.y + 6), ImVec2(sP2.x - 6, sP2.y - 6), (tabCol & 0x00FFFFFF) | (50 << 24), 6.0f);
            draw->AddRect(ImVec2(sP1.x + 6, sP1.y + 6), ImVec2(sP2.x - 6, sP2.y - 6), (tabCol & 0x00FFFFFF) | (140 << 24), 6.0f, 0, 1.4f);
        } else {
            draw->AddRectFilled(ImVec2(sP1.x + 6, sP1.y + 6), ImVec2(sP2.x - 6, sP2.y - 6), (tabCol & 0x00FFFFFF) | (18 << 24), 6.0f);
        }

        float diaX = sP1.x + 30.0f * scale_y;
        float diaY = (sP1.y + sP2.y) * 0.5f;
        ImU32 diaCol = active ? (g_darkMode ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 24, 36, 255)) : tabCol;
        draw->AddQuadFilled(
            ImVec2(diaX, diaY - 6.0f * scale_y),
            ImVec2(diaX + 6.0f * scale_y, diaY),
            ImVec2(diaX, diaY + 6.0f * scale_y),
            ImVec2(diaX - 6.0f * scale_y, diaY),
            diaCol);

        ImVec2 txtSz = titleFont->CalcTextSizeA(17.0f * scale_y, FLT_MAX, 0, tabNames[i]);
        draw->AddText(titleFont, 17.0f * scale_y, ImVec2(diaX + 20.0f * scale_y, diaY - txtSz.y * 0.5f), active ? (g_darkMode ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 24, 36, 255)) : tabCol, tabNames[i]);
    }

    float bodyY = tP2.y + 24.0f * scale_y;
    float bodyH = std::max(180.0f * scale_y, H - (bodyY - wp.y) - 78.0f * scale_y);
    float cardW = (tabW - 24.0f * scale_y) * 0.5f;

    auto DrawPanel = [&](ImVec2 cp1, ImVec2 cp2, const char* pTitle, ImU32 titleCol) {
        draw->AddRectFilled(ImVec2(cp1.x + 3, cp1.y + 5), ImVec2(cp2.x + 3, cp2.y + 5), IM_COL32(0, 0, 0, g_darkMode ? 110 : 35), 14.0f);
        draw->AddRectFilled(ImVec2(cp1.x - 2, cp1.y - 2), ImVec2(cp2.x + 2, cp2.y + 2), (titleCol & 0x00FFFFFF) | (28 << 24), 16.0f);
        draw->AddRectFilled(cp1, cp2, bgCard, 14.0f);
        draw->AddRect(cp1, cp2, border, 14.0f, 0, 1.5f);

        float tLine = 18.0f * scale_y;
        draw->AddLine(cp1, ImVec2(cp1.x + tLine, cp1.y), titleCol, 2.6f);
        draw->AddLine(cp1, ImVec2(cp1.x, cp1.y + tLine), titleCol, 2.6f);
        draw->AddLine(cp1, ImVec2(cp2.x, cp2.y), titleCol, 2.6f);
        draw->AddLine(cp2, ImVec2(cp2.x - tLine, cp2.y), titleCol, 2.6f);
        draw->AddLine(cp2, ImVec2(cp2.x, cp2.y - tLine), titleCol, 2.6f);

        draw->AddText(titleFont, 20.0f * scale_y, ImVec2(cp1.x + 22.0f * scale_y, cp1.y + 18.0f * scale_y), titleCol, pTitle);
        draw->AddRectFilled(ImVec2(cp1.x + 22.0f * scale_y, cp1.y + 50.0f * scale_y), ImVec2(cp2.x - 22.0f * scale_y, cp1.y + 51.5f * scale_y), (titleCol & 0x00FFFFFF) | (150 << 24), 1.0f);
    };

    ImVec2 c1P1 = ImVec2(wp.x + pad, bodyY);
    ImVec2 c1P2 = ImVec2(c1P1.x + cardW, c1P1.y + bodyH);
    ImVec2 c2P1 = ImVec2(c1P2.x + 24.0f * scale_y, bodyY);
    ImVec2 c2P2 = ImVec2(c2P1.x + cardW, c2P1.y + bodyH);

    auto ModuleRow = [&](ImVec2 cp1, ImVec2 cp2, float y, const char* label, bool* value, const char* uid, ImU32 rowAccent) {
        float rowY = cp1.y + y;
        float dotX = cp1.x + 34.0f * scale_y;
        float dotY = rowY + 10.0f * scale_y;

        if (*value) {
            draw->AddCircleFilled(ImVec2(dotX, dotY), 12.0f * scale_y, (green & 0x00FFFFFF) | (70 << 24));
        }
        draw->AddCircleFilled(ImVec2(dotX, dotY), 7.5f * scale_y, *value ? ((green & 0x00FFFFFF) | (80 << 24)) : (g_darkMode ? IM_COL32(60, 60, 72, 90) : IM_COL32(180, 185, 200, 100)));
        draw->AddCircleFilled(ImVec2(dotX, dotY), 4.5f * scale_y, *value ? green : (g_darkMode ? IM_COL32(90, 90, 108, 255) : IM_COL32(150, 155, 175, 255)));

        draw->AddText(titleFont, 17.0f * scale_y, ImVec2(dotX + 22.0f * scale_y, rowY), *value ? textMain : textDim, label);

        const char* st = *value ? "ON" : "OFF";
        float pillW = 62.0f * scale_y;
        float pillH = 28.0f * scale_y;
        ImVec2 pp1(cp2.x - pillW - 24.0f * scale_y, rowY - 4.0f * scale_y);
        ImVec2 pp2(pp1.x + pillW, pp1.y + pillH);

        ImU32 pillCol = *value ? green : rowAccent;
        ImU32 pillBg  = *value ? ((green & 0x00FFFFFF) | (80 << 24)) : ((rowAccent & 0x00FFFFFF) | (60 << 24));

        draw->AddRectFilled(ImVec2(pp1.x - 2, pp1.y - 2), ImVec2(pp2.x + 2, pp2.y + 2), (pillCol & 0x00FFFFFF) | (50 << 24), pillH * 0.5f + 2.0f);
        draw->AddRectFilled(pp1, pp2, pillBg, pillH * 0.5f);
        draw->AddRect(pp1, pp2, pillCol, pillH * 0.5f, 0, 1.8f);

        ImVec2 stSz = titleFont->CalcTextSizeA(14.0f * scale_y, FLT_MAX, 0, st);
        draw->AddText(titleFont, 14.0f * scale_y, ImVec2(pp1.x + (pillW - stSz.x) * 0.5f, pp1.y + 6.0f * scale_y), pillCol, st);

        ImGui::SetCursorScreenPos(ImVec2(cp1.x + 10, rowY - 6.0f * scale_y));
        ImGui::PushID(uid);
        if (ImGui::InvisibleButton("##mod", ImVec2(cp2.x - cp1.x - 20.0f, 34.0f * scale_y))) {
            *value = !(*value);
        }
        ImGui::PopID();
    };

    auto InfoRow = [&](ImVec2 cp1, ImVec2 cp2, float y, const char* key, const char* val, ImU32 valCol) {
        float rowY = cp1.y + y;
        draw->AddText(titleFont, 16.0f * scale_y, ImVec2(cp1.x + 34.0f * scale_y, rowY), textDim, key);
        ImVec2 vSz = titleFont->CalcTextSizeA(16.0f * scale_y, FLT_MAX, 0, val);
        draw->AddText(titleFont, 16.0f * scale_y, ImVec2(cp2.x - 34.0f * scale_y - vSz.x, rowY), valCol, val);
        draw->AddLine(ImVec2(cp1.x + 28.0f * scale_y, rowY + 26.0f * scale_y), ImVec2(cp2.x - 28.0f * scale_y, rowY + 26.0f * scale_y), g_darkMode ? IM_COL32(50, 50, 70, 255) : IM_COL32(210, 215, 230, 255), 1.2f);
    };

    const char* leftTitle  = "";
    ImU32       leftColor  = accent;
    if (g_CurrentTab == 0)      { leftTitle = "AIM PANEL";    leftColor = redColor; }
    else if (g_CurrentTab == 1) { leftTitle = "VISUAL PANEL"; leftColor = cyan; }
    else if (g_CurrentTab == 2) { leftTitle = "MISC PANEL";   leftColor = yellow; }
    else                        { leftTitle = "CONFIG PANEL"; leftColor = purple; }

    DrawPanel(c1P1, c1P2, leftTitle, leftColor);

    float rowStep = 46.0f * scale_y;
    float rowBase = 70.0f * scale_y;

    if (g_CurrentTab == 0) {
        ModuleRow(c1P1, c1P2, rowBase,               "AIM BOT",    &Aimbot,     "n_aim1", redColor);
        ModuleRow(c1P1, c1P2, rowBase + rowStep,     "AIM LOCK",   &AimLock,    "n_aim_lock", redColor);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 2, "SILENT",     &AimSilent,  "n_aim2", redColor);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 3, "VISIBLE",    &AimVisible, "n_aim3", redColor);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 4, "AIM MAGNET", &g_AimMagnetUi, "n_aim_magnet_ui", redColor);

        float fovY = c1P1.y + rowBase + rowStep * 4 + 48.0f * scale_y;
        draw->AddText(titleFont, 16.0f * scale_y, ImVec2(c1P1.x + 34.0f * scale_y, fovY), textDim, "FOV");

        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", AimFov);
        ImVec2 bSz2 = titleFont->CalcTextSizeA(16.0f * scale_y, FLT_MAX, 0, buf);
        draw->AddText(titleFont, 16.0f * scale_y, ImVec2(c1P2.x - 34.0f * scale_y - bSz2.x, fovY), accent2, buf);

        float tx1 = c1P1.x + 34.0f * scale_y;
        float tx2 = c1P2.x - 34.0f * scale_y;
        float ty  = fovY + 26.0f * scale_y;
        float th  = 5.0f;

        draw->AddRectFilled(ImVec2(tx1, ty), ImVec2(tx2, ty + th), g_darkMode ? IM_COL32(55, 55, 75, 255) : IM_COL32(200, 205, 220, 255), 2.5f);
        float tFov = AimFov / 360.0f;
        float kx = tx1 + (tx2 - tx1) * tFov;

        draw->AddRectFilledMultiColor(ImVec2(tx1, ty), ImVec2(kx, ty + th), accent, accent2, accent2, accent);
        draw->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), 11.0f * scale_y, accent);
        draw->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), 7.5f * scale_y, IM_COL32(255, 255, 255, 255));
        draw->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), 4.0f * scale_y, accent);

        float hitH = 20.0f * scale_y;
        ImGui::SetCursorScreenPos(ImVec2(tx1, ty + th * 0.5f - hitH * 0.5f));
        ImGui::PushID("ninmods_fov");
        ImGui::InvisibleButton("##fov", ImVec2(tx2 - tx1, hitH));
        if (ImGui::IsItemActive()) {
            float m = (ImGui::GetMousePos().x - tx1) / (tx2 - tx1);
            m = std::max(0.0f, std::min(1.0f, m));
            AimFov = m * 360.0f;
        }
        ImGui::PopID();
    }
    else if (g_CurrentTab == 1) {
        ModuleRow(c1P1, c1P2, rowBase,               "BOX ESP",   &showBox,      "n_vis1", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep,     "LINE",      &showLine,     "n_vis2", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 2, "SKELETON",  &showSkeleton, "n_vis_skel", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 3, "HEALTH",    &showHealth,   "n_vis3", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 4, "NAME",      &showName,     "n_vis4", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 5, "DISTANCE",  &showDistance, "n_vis5", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 6, "COUNT",     &showCount,    "n_vis_cnt", cyan);
    }
    else if (g_CurrentTab == 2) {
        ModuleRow(c1P1, c1P2, rowBase,               "SPEED",     &SpeedHackV2, "n_msc1", yellow);
        ModuleRow(c1P1, c1P2, rowBase + rowStep,     "GHOST",     &GhostHack,   "n_msc2", yellow);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 2, "NO RELOAD", &NoReload,    "n_msc3", yellow);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 3, "NO RECOIL", &NoRecoil,    "n_msc4", yellow);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 4, "360 ALERT", &is3600,      "n_msc5", yellow);
    }
    else {
        float y0 = c1P1.y + 70.0f * scale_y;
        draw->AddText(titleFont, 16.0f * scale_y, ImVec2(c1P1.x + 34.0f * scale_y, y0), textMain, "Box Color");

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 34.0f * scale_y, y0 + 26.0f * scale_y));
        ImGui::PushID("n_boxcol");
        ImGui::ColorEdit4("##bc", &boxCustomColor.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 88.0f * scale_y, y0 + 26.0f * scale_y));
        ImGui::SetNextItemWidth(cardW - 122.0f * scale_y);
        const char* colorModes[] = { "Custom", "Health", "Distance", "Rainbow", "Cyan",
                                     "Green", "Purple", "Orange", "Red", "White" };
        ImGui::Combo("##bm", &boxColorMode, colorModes, IM_ARRAYSIZE(colorModes));

        float y1 = y0 + 72.0f * scale_y;
        draw->AddText(titleFont, 16.0f * scale_y, ImVec2(c1P1.x + 34.0f * scale_y, y1), textMain, "Line Color");

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 34.0f * scale_y, y1 + 26.0f * scale_y));
        ImGui::PushID("n_linecol");
        ImGui::ColorEdit4("##lc", &lineCustomColor.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 88.0f * scale_y, y1 + 26.0f * scale_y));
        ImGui::SetNextItemWidth(cardW - 122.0f * scale_y);
        ImGui::Combo("##lm", &lineColorMode, colorModes, IM_ARRAYSIZE(colorModes));

        float y2 = y1 + 72.0f * scale_y;
        draw->AddText(titleFont, 16.0f * scale_y, ImVec2(c1P1.x + 34.0f * scale_y, y2), textMain, "Theme");

        for (int i = 0; i < 6; i++) {
            ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 34.0f * scale_y + (float)i * 48.0f * scale_y, y2 + 26.0f * scale_y));
            ImGui::PushID(700 + i);
            if (ImGui::ColorButton("##sw", g_ThemeSwatches[i], 0, ImVec2(38, 38))) {
                g_ThemeColorArr[0] = g_ThemeSwatches[i].x;
                g_ThemeColorArr[1] = g_ThemeSwatches[i].y;
                g_ThemeColorArr[2] = g_ThemeSwatches[i].z;
                g_ThemeColorArr[3] = g_ThemeSwatches[i].w;
            }
            ImGui::PopID();
        }

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 34.0f * scale_y, y2 + 82.0f * scale_y));
        ImGui::PushStyleColor(ImGuiCol_Button, g_darkMode ? IM_COL32(80, 80, 220, 220) : IM_COL32(255, 200, 80, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_darkMode ? IM_COL32(110, 110, 240, 255) : IM_COL32(255, 220, 120, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_darkMode ? IM_COL32(60, 60, 200, 255) : IM_COL32(230, 180, 60, 255));
        if (ImGui::Button(g_darkMode ? "DARK MODE" : "LIGHT MODE", ImVec2(170.0f * scale_y, 45.0f * scale_y))) {
            g_darkMode = !g_darkMode;
        }
        ImGui::PopStyleColor(3);

        ImGui::SetCursorScreenPos(ImVec2(c1P2.x - 204.0f * scale_y, y2 + 82.0f * scale_y));
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(220, 60, 60, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 90, 90, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(180, 40, 40, 255));
        if (ImGui::Button("EXIT", ImVec2(170.0f * scale_y, 45.0f * scale_y))) {
            main_thread_flag = false;
            g_ShowMenu = false;
        }
        ImGui::PopStyleColor(3);
    }

    DrawPanel(c2P1, c2P2, "SYSTEM INFO", accent2);

    float infoStart = 76.0f * scale_y;
    float infoStep  = 46.0f * scale_y;

    InfoRow(c2P1, c2P2, infoStart, "STATUS", connected ? "READY" : "OFFLINE", connected ? green : redColor);

    char pidBuf[32];
    snprintf(pidBuf, sizeof(pidBuf), "%d", game_pid);
    InfoRow(c2P1, c2P2, infoStart + infoStep, "PID", (game_pid > 0) ? pidBuf : "NONE", (game_pid > 0) ? accent3 : textDim);

    char libBuf[32];
    snprintf(libBuf, sizeof(libBuf), "0x%lx", (unsigned long)lib_base);
    InfoRow(c2P1, c2P2, infoStart + infoStep * 2, "LIB BASE", (lib_base > 0) ? libBuf : "NONE", (lib_base > 0) ? cyan : textDim);

    const char* dNames[] = { "NONE", "RT HOOK", "RT DEV", "DEV COMPAT", "QX 10-11", "KPM NEO" };
    const char* dName = (选择值 >= 1 && 选择值 <= 5) ? dNames[选择值] : "ARMED";
    InfoRow(c2P1, c2P2, infoStart + infoStep * 3, "DRIVER", dName, green);

    InfoRow(c2P1, c2P2, infoStart + infoStep * 4, "MODE", (const char*[]){ "AIM", "VISUALS", "MISC", "CONFIG" }[g_CurrentTab], purple);

    const char* coreTxt = "N I N M O D S";
    float coreSize = 30.0f * scale_y;
    ImVec2 coreSz = titleFont->CalcTextSizeA(coreSize, FLT_MAX, 0, coreTxt);
    ImVec2 corePos = ImVec2(c2P1.x + (cardW - coreSz.x) * 0.5f, c2P2.y - 80.0f * scale_y);
    draw->AddText(titleFont, coreSize, corePos, cyan, coreTxt);

    const char* cap = "K E R N E L   C O R E";
    float capSize = 12.0f * scale_y;
    ImVec2 capSz = titleFont->CalcTextSizeA(capSize, FLT_MAX, 0, cap);
    draw->AddText(titleFont, capSize, ImVec2(c2P1.x + (cardW - capSz.x) * 0.5f, c2P2.y - 40.0f * scale_y), textDim, cap);

    const char* foot = "N I N M O D S   K E R N E L";
    float footSize = 16.0f * scale_y;
    ImVec2 fSz = titleFont->CalcTextSizeA(footSize, FLT_MAX, 0, foot);
    float fx = wp.x + (W - fSz.x) * 0.5f;
    float fy = H - 44.0f * scale_y;

    draw->AddLine(ImVec2(wp.x + pad, fy + 12.0f * scale_y), ImVec2(wp.x + W * 0.30f, fy + 12.0f * scale_y), accent2, 1.5f);
    draw->AddLine(ImVec2(wp.x + W * 0.70f, fy + 12.0f * scale_y), ImVec2(wp.x + W - pad, fy + 12.0f * scale_y), accent2, 1.5f);

    ImVec2 fDot  = ImVec2(wp.x + W * 0.30f + 8.0f * scale_y, fy + 12.0f * scale_y);
    ImVec2 fDot2 = ImVec2(wp.x + W * 0.70f - 8.0f * scale_y, fy + 12.0f * scale_y);
    draw->AddCircleFilled(fDot,  6.0f * scale_y, accent);
    draw->AddCircleFilled(fDot2, 6.0f * scale_y, accent2);

    float ftx = fx;
    ImU32 fcols[3] = { accent, accent2, purple };
    for (int i = 0; foot[i]; i++) {
        if (foot[i] == ' ') { ftx += footSize * 0.5f; continue; }
        char buf[2] = { foot[i], 0 };
        draw->AddText(titleFont, footSize, ImVec2(ftx, fy), fcols[i % 3], buf);
        ftx += titleFont->CalcTextSizeA(footSize, FLT_MAX, 0, buf).x;
    }

    }
    ImGui::End();
    ImGui::PopStyleVar();
}

namespace ui::menu {

void render() {
    RenderFloatingLogo();
    if (g_ShowMenu) {
        RenderMenu();
    }
}

}
namespace ui::menu {
