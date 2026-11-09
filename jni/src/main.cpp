#include "crash_handler.hpp"
#include "display_manager.hpp"
#include "logger.hpp"
#include "main.h"

// FONTS
#include "ImGui/FONTS/MAIN_FONT.h"
#include "ImGui/FONTS/ICONS.h"
#include "ImGui/FONTS/SPECIAL.h"
#include "ImGui/FONTS/font.h"

#include <iomanip>
#include <map>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <thread>
#include <cmath>
#include <chrono>
#include <linux/input.h>

#include "ENC/oxorany_include.h"
#include "ENC/oxorany.h"
#include "driver.h"

//  GLOBALS

float g_ThemeColorArr[4] = { 0.68f, 0.28f, 0.98f, 1.00f }; // NinMods Cyber Purple
#define g_ThemeColor ImVec4(g_ThemeColorArr[0], g_ThemeColorArr[1], g_ThemeColorArr[2], g_ThemeColorArr[3])

float g_GlowIntensity = 1.0f;
float g_MenuSize      = 1250.0f;
float g_MenuWidth     = 1250.0f;
float g_MenuHeight    = 850.0f;
bool  g_darkMode      = true;
int   currentTab      = 0;
bool  showMenu        = true;
int   g_menuMode      = 1;
int   g_NexusTab      = 0;   // 0=AIM  1=VISUALS  2=MISC  3=CONFIG

bool  g_ShowNexusMenu = true;
// UI-only Aim Magnet toggle; does not alter game aiming or memory.
bool  g_AimMagnetUi = false;

// Floating logo position (draggable overlay anchor)
float  g_LogoX = 30.0f;        // Pixel (X position)
float  g_LogoY = 0.38f;        // Ratio of screen.y (0.0 - 1.0)
bool   g_LogoDragging = false;
ImVec2 g_LogoDragOffset(0, 0);
ImVec2 g_LogoDragStart(0, 0);
float  g_LogoSize = 130.0f;
float  g_LogoDragThreshold = 24.0f;

// Drag / Tap state machine
static bool s_logoWasDraggingLastFrame = false;
static bool s_logoJustMoved            = false;

ImFont* g_IconFont = nullptr;
ImFont* g_BoldFont = nullptr;

// Global theme swatches
static const ImVec4 g_ThemeSwatches[6] = {
    ImVec4(0.68f, 0.28f, 0.98f, 1.00f), // NinMods Vibrant Purple
    ImVec4(0.85f, 0.25f, 0.95f, 1.00f), // Neon Magenta
    ImVec4(0.50f, 0.20f, 0.92f, 1.00f), // Deep Violet
    ImVec4(0.30f, 0.55f, 1.00f, 1.00f), // Cyber Blue
    ImVec4(0.00f, 0.85f, 0.45f, 1.00f), // Emerald
    ImVec4(1.00f, 0.20f, 0.20f, 1.00f), // Crimson
};


#include "Scarecrow/hook.h"

//  LOGO POSITION SAVE / LOAD

static const char* LOGO_POS_FILE = "/dev/.nin_logo_pos.txt";

void SaveLogoPosition() {
    std::ofstream f(LOGO_POS_FILE);
    if (f.is_open()) {
        f << g_LogoX << " " << g_LogoY << " " << g_LogoSize;
        f.close();
    }
}

void LoadLogoPosition() {
    std::ifstream f(LOGO_POS_FILE);
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

//  COLOR HELPER

ImU32 GetESPColor(int colorMode, ImVec4* customColor, float health = 100.0f, float distance = 0.0f) {
    float r = 0, g = 0, b = 0, a = 1.0f;
    switch (colorMode) {
        case 0:
            r = customColor->x; g = customColor->y; b = customColor->z; a = customColor->w;
            break;
        case 1: {
            float hp = health / 100.0f;
            if (hp < 0) hp = 0; if (hp > 1) hp = 1;
            r = 1.0f - hp; g = hp; b = 0.0f; break;
        }
        case 2: {
            float d = distance / 200.0f;
            if (d > 1.0f) d = 1.0f;
            r = 1.0f; g = 1.0f - d; b = 1.0f - d; break;
        }
        case 3: {
            static float hue = 0.0f;
            hue += 0.005f;
            if (hue > 1.0f) hue -= 1.0f;
            ImVec4 rb = ImColor::HSV(hue, 1.0f, 1.0f);
            r = rb.x; g = rb.y; b = rb.z; break;
        }
        case 4:  r = 0.0f; g = 1.0f; b = 1.0f; break;
        case 5:  r = 0.0f; g = 1.0f; b = 0.4f; break;
        case 6:  r = 0.6f; g = 0.0f; b = 1.0f; break;
        case 7:  r = 1.0f; g = 0.5f; b = 0.0f; break;
        case 8:  r = 1.0f; g = 0.0f; b = 0.0f; break;
        case 9:  r = 1.0f; g = 1.0f; b = 1.0f; break;
        case 10: r = 1.0f; g = 1.0f; b = 0.0f; break;
        case 11: r = 1.0f; g = 0.0f; b = 1.0f; break;
        case 12: r = 0.0f; g = 0.5f; b = 1.0f; break;
        default: r = 1.0f; g = 1.0f; b = 1.0f;
    }
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

//  THEME

void ApplyCustomTheme(float scale_y = 1.0f) {
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

//  FLOATING LOGO BUTTON — FULLY DRAGGABLE
//  Tap = Toggle Menu | Drag = Relocate anywhere on screen

void RenderFloatingLogo() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen = io.DisplaySize;

    const float logoSize = g_LogoSize;
    const float margin   = 10.0f;

    // Position calculation (g_LogoX = pixel, g_LogoY = 0..1 ratio)
    ImVec2 logoPos(g_LogoX, screen.y * g_LogoY);

    // Clamp within viewport bounds
    logoPos.x = std::max(margin, std::min(logoPos.x, screen.x - logoSize - margin));
    logoPos.y = std::max(margin, std::min(logoPos.y, screen.y - logoSize - margin));

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
    ImGui::Begin("##NINMODS_LOGO_BTN", nullptr, flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 wp  = ImGui::GetWindowPos();
    ImVec2 ctr = ImVec2(wp.x + logoSize * 0.5f, wp.y + logoSize * 0.5f);
    float  rad = logoSize * 0.44f;

    // Clickable / Draggable hit-test bounds
    ImGui::SetCursorScreenPos(wp);
    bool clicked = ImGui::InvisibleButton("##logoClick", ImVec2(logoSize, logoSize));

    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    const ImVec2 mouse = ImGui::GetMousePos();

// Drag vs Tap discrimination logic
    if (ImGui::IsItemActivated()) {
        g_LogoDragStart  = mouse;
        g_LogoDragOffset = ImVec2(mouse.x - logoPos.x, mouse.y - logoPos.y);
        g_LogoDragging   = false;
        s_logoJustMoved  = false;
    }

    // Initiate dragging past motion threshold
    if (active && !g_LogoDragging) {
        const float dx = mouse.x - g_LogoDragStart.x;
        const float dy = mouse.y - g_LogoDragStart.y;
        if (sqrtf(dx * dx + dy * dy) > g_LogoDragThreshold) {
            g_LogoDragging  = true;
            s_logoJustMoved = true;
        }
    }

    // Dragging in progress — update anchor position
    if (g_LogoDragging && active) {
        float newX = mouse.x - g_LogoDragOffset.x;
        float newY = mouse.y - g_LogoDragOffset.y;

        newX = std::max(margin, std::min(newX, screen.x - logoSize - margin));
        newY = std::max(margin, std::min(newY, screen.y - logoSize - margin));

        g_LogoX = newX;
        g_LogoY = newY / screen.y;
    }

// Drag release — check tap vs drag completion
    if (clicked || (ImGui::IsItemDeactivated() && !s_logoJustMoved)) {
        g_ShowNexusMenu = !g_ShowNexusMenu;
        g_LogoDragging  = false;
        s_logoJustMoved = false;
        s_logoWasDraggingLastFrame = false;
    } else if (ImGui::IsItemDeactivated()) {
        if (g_LogoDragging) {
            SaveLogoPosition();
        }
        g_LogoDragging  = false;
        s_logoJustMoved = false;
        s_logoWasDraggingLastFrame = false;
    }

    s_logoWasDraggingLastFrame = g_LogoDragging;

// Color palette
    ImU32 accent = ImGui::ColorConvertFloat4ToU32(g_ThemeColor);
    ImU32 cyan   = IM_COL32(80, 220, 255, 255);
    ImU32 purple = IM_COL32(180, 120, 255, 255);
    ImU32 white  = IM_COL32(255, 255, 255, 255);

    float t     = (float)ImGui::GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * 2.5f);
    float scale = (hovered || g_LogoDragging) ? (1.0f + 0.08f * pulse) : 1.0f;
    float r     = rad * scale;

    // Glow
    draw->AddCircleFilled(ctr, r + 22.0f,
                          (accent & 0x00FFFFFF) | ((int)(45 * pulse) << 24), 64);
    draw->AddCircleFilled(ctr, r + 12.0f,
                          (accent & 0x00FFFFFF) | ((int)(75 * pulse) << 24), 56);
    draw->AddCircleFilled(ctr, r + 5.0f,
                          (accent & 0x00FFFFFF) | ((int)(90 * pulse) << 24), 48);

    // Body
    draw->AddCircleFilled(ctr, r,        IM_COL32(15, 15, 24, 252), 64);
    draw->AddCircleFilled(ctr, r - 3.0f, IM_COL32(22, 22, 36, 250), 64);

    // Border rings
    draw->AddCircle(ctr, r,        accent, 64, 4.5f);
    draw->AddCircle(ctr, r - 3.0f, (cyan   & 0x00FFFFFF) | (160 << 24), 64, 2.0f);
    draw->AddCircle(ctr, r - 7.0f, (purple & 0x00FFFFFF) | (100 << 24), 60, 1.5f);

    draw->AddCircle(ctr, r * 0.82f, (accent & 0x00FFFFFF) | (200 << 24), 48, 2.0f);
    draw->AddCircle(ctr, r * 0.66f, (cyan   & 0x00FFFFFF) | (150 << 24), 44, 1.5f);
    draw->AddCircle(ctr, r * 0.50f, (purple & 0x00FFFFFF) | (130 << 24), 40, 1.5f);

    // Rotating accent arcs
    float spin = g_LogoDragging ? 3.2f : 1.6f;
    float ang  = t * spin;
    for (int i = 0; i < 6; i++) {
        float a1 = ang + i * (IM_PI * 2.0f / 6.0f);
        float a2 = a1 + 0.42f;
        ImVec2 pa(ctr.x + cosf(a1) * (r - 6.0f), ctr.y + sinf(a1) * (r - 6.0f));
        ImVec2 pb(ctr.x + cosf(a2) * (r - 6.0f), ctr.y + sinf(a2) * (r - 6.0f));
        ImU32 arcCol = (i % 2 == 0) ? cyan : accent;
        draw->AddLine(pa, pb, arcCol, 4.0f);
    }

    // Internal brand icon "N"
    ImFont* bold = g_BoldFont ? g_BoldFont : ImGui::GetFont();
    const char* txt = "N";
    float txtSize = 54.0f;
    ImVec2 tsz = bold->CalcTextSizeA(txtSize, FLT_MAX, 0, txt);

    draw->AddText(bold, txtSize,
                  ImVec2(ctr.x - tsz.x * 0.5f, ctr.y - tsz.y * 0.5f - 3.0f),
                  (white & 0x00FFFFFF) | (80 << 24), txt);
    draw->AddText(bold, txtSize,
                  ImVec2(ctr.x - tsz.x * 0.5f, ctr.y - tsz.y * 0.5f - 4.0f),
                  (white & 0x00FFFFFF) | (140 << 24), txt);
    draw->AddText(bold, txtSize,
                  ImVec2(ctr.x - tsz.x * 0.5f, ctr.y - tsz.y * 0.5f - 5.0f),
                  white, txt);

    // Status dot
    bool connected = (game_pid > 0 && lib_base > 0);
    ImU32 stCol = connected ? IM_COL32(0, 240, 140, 255) : IM_COL32(255, 90, 90, 255);
    ImVec2 stDot(ctr.x + r * 0.68f, ctr.y - r * 0.68f);

    draw->AddCircleFilled(stDot, 12.0f, (stCol & 0x00FFFFFF) | (100 << 24));
    draw->AddCircleFilled(stDot,  7.0f, (stCol & 0x00FFFFFF) | (150 << 24));
    draw->AddCircleFilled(stDot,  4.5f, stCol);

    // Render "MOVE" tooltip during active drag
    if (g_LogoDragging) {
        const char* moveTxt = "MOVE";
        float mSize = 14.0f;
        ImVec2 mSz = bold->CalcTextSizeA(mSize, FLT_MAX, 0, moveTxt);
        float iy = ctr.y + r + 6.0f;
        draw->AddRectFilled(
            ImVec2(ctr.x - mSz.x * 0.5f - 12.0f, iy),
            ImVec2(ctr.x + mSz.x * 0.5f + 12.0f, iy + 24.0f),
            IM_COL32(0, 0, 0, 220), 6.0f);
        draw->AddRect(
            ImVec2(ctr.x - mSz.x * 0.5f - 12.0f, iy),
            ImVec2(ctr.x + mSz.x * 0.5f + 12.0f, iy + 24.0f),
            cyan, 6.0f, 0, 1.5f);
        draw->AddText(bold, mSize,
            ImVec2(ctr.x - mSz.x * 0.5f, iy + 4.0f),
            cyan, moveTxt);
    }
    // Hover hint
    else if (hovered) {
        const char* hintTxt = "TAP=OPEN  DRAG=MOVE";
        float hSize = 11.0f;
        ImVec2 hSz = bold->CalcTextSizeA(hSize, FLT_MAX, 0, hintTxt);
        float iy = ctr.y + r + 6.0f;
        draw->AddRectFilled(
            ImVec2(ctr.x - hSz.x * 0.5f - 10.0f, iy),
            ImVec2(ctr.x + hSz.x * 0.5f + 10.0f, iy + 20.0f),
            IM_COL32(0, 0, 0, 180), 5.0f);
        draw->AddText(bold, hSize,
            ImVec2(ctr.x - hSz.x * 0.5f, iy + 4.0f),
            (cyan & 0x00FFFFFF) | (200 << 24), hintTxt);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

//  RENDER MENU — NEXUS CORE

void RenderMenu() {
    g_MenuWidth  = 1250.0f;
    g_MenuHeight = 850.0f;

    float scale_y = g_MenuHeight / 900.0f;
    scale_y = std::max(0.70f, std::min(scale_y, 1.10f));

    ApplyCustomTheme(scale_y);

    ImGui::SetNextWindowPos(ImVec2(100.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(g_MenuWidth, g_MenuHeight), ImGuiCond_Always);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("NinMods", nullptr, flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    float  W  = ws.x;
    float  H  = ws.y;

    // Palette (NinMods Cyber Purple Theme)
    ImU32 bgDeep     = IM_COL32(12, 10, 22, 252);
    ImU32 bgCard     = IM_COL32(22, 18, 38, 250);
    ImU32 bgInset    = IM_COL32(32, 26, 52, 255);
    ImU32 border     = IM_COL32(75, 55, 115, 255);
    ImU32 borderGlow = IM_COL32(110, 80, 165, 255);
    ImU32 accent     = ImGui::ColorConvertFloat4ToU32(g_ThemeColor);
    ImU32 accent2    = IM_COL32(190, 130, 255, 255);
    ImU32 accent3    = IM_COL32(230, 160, 255, 255);

    ImU32 textMain   = IM_COL32(245, 240, 255, 255);
    ImU32 textDim    = IM_COL32(170, 160, 200, 255);

    ImU32 green      = IM_COL32(0, 240, 140, 255);
    ImU32 redColor   = IM_COL32(255, 90, 90, 255);
    ImU32 purple     = IM_COL32(185, 110, 255, 255);
    ImU32 cyan       = IM_COL32(160, 130, 255, 255);
    ImU32 yellow     = IM_COL32(255, 220, 100, 255);
    ImU32 pink       = IM_COL32(255, 120, 220, 255);

    // MAIN FRAME
    ImVec2 p1 = wp;
    ImVec2 p2 = ImVec2(wp.x + W, wp.y + H);

    draw->AddRectFilled(ImVec2(p1.x - 5, p1.y - 5),
                        ImVec2(p2.x + 5, p2.y + 5),
                        (accent & 0x00FFFFFF) | (25 << 24), 22.0f);

    draw->AddRectFilled(ImVec2(p1.x + 5, p1.y + 8),
                        ImVec2(p2.x + 5, p2.y + 8),
                        IM_COL32(0, 0, 0, 140), 18.0f);

    draw->AddRectFilled(p1, p2, bgDeep, 18.0f);

    draw->AddRect(ImVec2(p1.x + 1, p1.y + 1),
                  ImVec2(p2.x - 1, p2.y - 1),
                  borderGlow, 17.0f, 0, 1.6f);

    draw->AddRectFilledMultiColor(
        ImVec2(p1.x + 20, p1.y),
        ImVec2(p2.x - 20, p1.y + 3),
        accent, accent2, accent2, accent);

    // HEADER
    float pad   = 34.0f * scale_y;
    float headY = wp.y + pad * 0.85f;

    ImFont* titleFont = g_BoldFont ? g_BoldFont : ImGui::GetFont();

    const char* title = "N I N M O D S";
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
        draw->AddLine(ImVec2(x, lineY),
                      ImVec2(x + 9.0f * scale_y, lineY),
                      dc, 2.0f);
    }

    // CLOSE (X)
    float closeSize = 42.0f * scale_y;
    ImVec2 closePos(wp.x + W - pad - closeSize, headY - 6.0f * scale_y);

    ImGui::SetCursorScreenPos(closePos);
    ImGui::PushID("nexus_close");
    ImGui::InvisibleButton("##close", ImVec2(closeSize, closeSize));
    bool closeHov = ImGui::IsItemHovered();
    bool closeClk = ImGui::IsItemClicked();
    ImGui::PopID();

    ImVec2 closeCenter(closePos.x + closeSize * 0.5f, closePos.y + closeSize * 0.5f);
    float  closeRad = closeSize * 0.5f;

    ImU32 closeCol = closeHov ? IM_COL32(255, 90, 90, 255) : IM_COL32(200, 60, 60, 220);

    if (closeHov) {
        draw->AddCircleFilled(closeCenter, closeRad + 4.0f,
                              (closeCol & 0x00FFFFFF) | (60 << 24), 32);
    }
    draw->AddCircleFilled(closeCenter, closeRad, closeCol, 32);
    draw->AddCircle(closeCenter, closeRad, IM_COL32(255, 255, 255, 100), 32, 1.4f);

    float xOff = closeRad * 0.42f;
    ImU32 xCol = IM_COL32(255, 255, 255, 255);
    draw->AddLine(ImVec2(closeCenter.x - xOff, closeCenter.y - xOff),
                  ImVec2(closeCenter.x + xOff, closeCenter.y + xOff), xCol, 3.0f);
    draw->AddLine(ImVec2(closeCenter.x + xOff, closeCenter.y - xOff),
                  ImVec2(closeCenter.x - xOff, closeCenter.y + xOff), xCol, 3.0f);

    if (closeClk) g_ShowNexusMenu = false;

    // ACTIVE badge
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
    draw->AddText(titleFont, 15.0f * scale_y,
        ImVec2(dotC.x + 14.0f * scale_y, bP1.y + (badgeH - bSz.y) * 0.5f),
        liveCol, badgeTxt);

    // HERO
    float heroY = lineY + 32.0f * scale_y;

    float nxW = 250.0f * scale_y;
    float nxH = 100.0f * scale_y;
    ImVec2 nxP1 = ImVec2(wp.x + pad, heroY);
    ImVec2 nxP2 = ImVec2(nxP1.x + nxW, nxP1.y + nxH);

    draw->AddRectFilled(ImVec2(nxP1.x - 3, nxP1.y - 3),
                        ImVec2(nxP2.x + 3, nxP2.y + 3),
                        (accent & 0x00FFFFFF) | (40 << 24), 13.0f);

    draw->AddRectFilled(nxP1, nxP2, bgCard, 12.0f);
    draw->AddRect(nxP1, nxP2, accent, 12.0f, 0, 2.0f);

    float tick = 16.0f * scale_y;
    draw->AddLine(nxP1, ImVec2(nxP1.x + tick, nxP1.y), accent2, 3.0f);
    draw->AddLine(nxP1, ImVec2(nxP1.x, nxP1.y + tick), accent2, 3.0f);
    draw->AddLine(ImVec2(nxP2.x, nxP2.y), ImVec2(nxP2.x - tick, nxP2.y), accent2, 3.0f);
    draw->AddLine(ImVec2(nxP2.x, nxP2.y), ImVec2(nxP2.x, nxP2.y - tick), accent2, 3.0f);

    const char* nxs = "NinMods";
    float nexusSize = 26.0f * scale_y;
    ImVec2 nxsSz = titleFont->CalcTextSizeA(nexusSize, FLT_MAX, 0, nxs);
    draw->AddText(titleFont, nexusSize,
        ImVec2(nxP1.x + (nxW - nxsSz.x) * 0.5f,
               nxP1.y + (nxH - nxsSz.y) * 0.5f),
        purple, nxs);

    float scX = nxP2.x + 46.0f * scale_y;
    draw->AddText(titleFont, 20.0f * scale_y,
        ImVec2(scX, heroY + 8.0f * scale_y), textMain, "SYSTEM CORE");

    draw->AddRectFilled(
        ImVec2(scX, heroY + 34.0f * scale_y),
        ImVec2(scX + 170.0f * scale_y, heroY + 36.5f * scale_y),
        accent3, 2.0f);

    ImVec2 cDot = ImVec2(scX + 10.0f * scale_y, heroY + 62.0f * scale_y);
    draw->AddCircleFilled(cDot, 7.0f * scale_y, (liveCol & 0x00FFFFFF) | (80 << 24));
    draw->AddCircleFilled(cDot, 4.5f * scale_y, liveCol);
    draw->AddText(titleFont, 15.0f * scale_y,
        ImVec2(cDot.x + 16.0f * scale_y, cDot.y - 10.0f * scale_y),
        green, connected ? "CONNECTED" : "DISCONNECTED");

    // TAB BAR
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
        bool active = (g_NexusTab == i);
        ImU32 tabCol = tabColors[i];

        ImGui::SetCursorScreenPos(sP1);
        ImGui::PushID(9000 + i);
        ImGui::InvisibleButton("##tab", ImVec2(slotW, tabH));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g_NexusTab = i;
        ImGui::PopID();

        if (active) {
            draw->AddRectFilled(
                ImVec2(sP1.x + 4, sP1.y + 4),
                ImVec2(sP2.x - 4, sP2.y - 4),
                (tabCol & 0x00FFFFFF) | (90 << 24), 8.0f);
            draw->AddRectFilled(
                ImVec2(sP1.x + 6, sP1.y + 6),
                ImVec2(sP2.x - 6, sP2.y - 6),
                (tabCol & 0x00FFFFFF) | (180 << 24), 6.0f);
            draw->AddRect(
                ImVec2(sP1.x + 6, sP1.y + 6),
                ImVec2(sP2.x - 6, sP2.y - 6),
                tabCol, 6.0f, 0, 1.8f);
            draw->AddRectFilled(
                ImVec2(sP1.x + 6, sP2.y - 4),
                ImVec2(sP2.x - 6, sP2.y - 1),
                tabCol, 2.0f);
        } else if (hovered) {
            draw->AddRectFilled(
                ImVec2(sP1.x + 6, sP1.y + 6),
                ImVec2(sP2.x - 6, sP2.y - 6),
                (tabCol & 0x00FFFFFF) | (50 << 24), 6.0f);
            draw->AddRect(
                ImVec2(sP1.x + 6, sP1.y + 6),
                ImVec2(sP2.x - 6, sP2.y - 6),
                (tabCol & 0x00FFFFFF) | (140 << 24), 6.0f, 0, 1.4f);
        } else {
            draw->AddRectFilled(
                ImVec2(sP1.x + 6, sP1.y + 6),
                ImVec2(sP2.x - 6, sP2.y - 6),
                (tabCol & 0x00FFFFFF) | (18 << 24), 6.0f);
        }

        float diaX = sP1.x + 30.0f * scale_y;
        float diaY = (sP1.y + sP2.y) * 0.5f;
        ImU32 diaCol = active ? IM_COL32(255, 255, 255, 255) : tabCol;
        draw->AddQuadFilled(
            ImVec2(diaX, diaY - 6.0f * scale_y),
            ImVec2(diaX + 6.0f * scale_y, diaY),
            ImVec2(diaX, diaY + 6.0f * scale_y),
            ImVec2(diaX - 6.0f * scale_y, diaY),
            diaCol);

        ImVec2 txtSz = titleFont->CalcTextSizeA(17.0f * scale_y, FLT_MAX, 0, tabNames[i]);
        draw->AddText(titleFont, 17.0f * scale_y,
            ImVec2(diaX + 20.0f * scale_y, diaY - txtSz.y * 0.5f),
            active ? IM_COL32(255, 255, 255, 255) : tabCol,
            tabNames[i]);
    }

    // CONTENT PANELS
    float bodyY = tP2.y + 24.0f * scale_y;
    float bodyH = H - (bodyY - wp.y) - 78.0f * scale_y;
    float cardW = (tabW - 24.0f * scale_y) * 0.5f;

    auto DrawPanel = [&](ImVec2 cp1, ImVec2 cp2, const char* title, ImU32 titleCol) {
        draw->AddRectFilled(ImVec2(cp1.x + 3, cp1.y + 5),
                            ImVec2(cp2.x + 3, cp2.y + 5),
                            IM_COL32(0, 0, 0, 110), 14.0f);
        draw->AddRectFilled(ImVec2(cp1.x - 2, cp1.y - 2),
                            ImVec2(cp2.x + 2, cp2.y + 2),
                            (titleCol & 0x00FFFFFF) | (28 << 24), 16.0f);
        draw->AddRectFilled(cp1, cp2, bgCard, 14.0f);
        draw->AddRect(cp1, cp2, border, 14.0f, 0, 1.5f);

        float t = 18.0f * scale_y;
        draw->AddLine(cp1, ImVec2(cp1.x + t, cp1.y), titleCol, 2.6f);
        draw->AddLine(cp1, ImVec2(cp1.x, cp1.y + t), titleCol, 2.6f);
        draw->AddLine(ImVec2(cp2.x, cp2.y), ImVec2(cp2.x - t, cp2.y), titleCol, 2.6f);
        draw->AddLine(ImVec2(cp2.x, cp2.y), ImVec2(cp2.x, cp2.y - t), titleCol, 2.6f);

        draw->AddText(titleFont, 20.0f * scale_y,
            ImVec2(cp1.x + 22.0f * scale_y, cp1.y + 18.0f * scale_y),
            titleCol, title);

        draw->AddRectFilled(
            ImVec2(cp1.x + 22.0f * scale_y, cp1.y + 50.0f * scale_y),
            ImVec2(cp2.x - 22.0f * scale_y, cp1.y + 51.5f * scale_y),
            (titleCol & 0x00FFFFFF) | (150 << 24), 1.0f);
    };

    ImVec2 c1P1 = ImVec2(wp.x + pad, bodyY);
    ImVec2 c1P2 = ImVec2(c1P1.x + cardW, c1P1.y + bodyH);
    ImVec2 c2P1 = ImVec2(c1P2.x + 24.0f * scale_y, bodyY);
    ImVec2 c2P2 = ImVec2(c2P1.x + cardW, c2P1.y + bodyH);

    auto ModuleRow = [&](ImVec2 cp1, ImVec2 cp2, float y,
                         const char* label, bool* value,
                         const char* uid, ImU32 rowAccent) {
        float rowY = cp1.y + y;
        float dotX = cp1.x + 34.0f * scale_y;
        float dotY = rowY + 10.0f * scale_y;

        if (*value) {
            draw->AddCircleFilled(ImVec2(dotX, dotY), 12.0f * scale_y,
                                  (green & 0x00FFFFFF) | (70 << 24));
        }
        draw->AddCircleFilled(ImVec2(dotX, dotY), 7.5f * scale_y,
            *value ? ((green & 0x00FFFFFF) | (80 << 24))
                   : IM_COL32(60, 60, 72, 90));
        draw->AddCircleFilled(ImVec2(dotX, dotY), 4.5f * scale_y,
            *value ? green : IM_COL32(90, 90, 108, 255));

        draw->AddText(titleFont, 17.0f * scale_y,
            ImVec2(dotX + 22.0f * scale_y, rowY),
            *value ? textMain : textDim, label);

        const char* st = *value ? "ON" : "OFF";
        float pillW = 62.0f * scale_y;
        float pillH = 28.0f * scale_y;
        ImVec2 pp1(cp2.x - pillW - 24.0f * scale_y, rowY - 4.0f * scale_y);
        ImVec2 pp2(pp1.x + pillW, pp1.y + pillH);

        ImU32 pillCol = *value ? green : rowAccent;
        ImU32 pillBg  = *value ? ((green & 0x00FFFFFF) | (80 << 24))
                               : ((rowAccent & 0x00FFFFFF) | (60 << 24));

        draw->AddRectFilled(
            ImVec2(pp1.x - 2, pp1.y - 2),
            ImVec2(pp2.x + 2, pp2.y + 2),
            (pillCol & 0x00FFFFFF) | (50 << 24), pillH * 0.5f + 2.0f);

        draw->AddRectFilled(pp1, pp2, pillBg, pillH * 0.5f);
        draw->AddRect(pp1, pp2, pillCol, pillH * 0.5f, 0, 1.8f);

        ImVec2 stSz = titleFont->CalcTextSizeA(14.0f * scale_y, FLT_MAX, 0, st);
        draw->AddText(titleFont, 14.0f * scale_y,
            ImVec2(pp1.x + (pillW - stSz.x) * 0.5f, pp1.y + 6.0f * scale_y),
            pillCol, st);

        ImGui::SetCursorScreenPos(ImVec2(cp1.x + 10, rowY - 6.0f * scale_y));
        ImGui::PushID(uid);
        if (ImGui::InvisibleButton("##mod",
            ImVec2(cp2.x - cp1.x - 20.0f, 34.0f * scale_y)))
            *value = !(*value);
        ImGui::PopID();
    };

    auto InfoRow = [&](ImVec2 cp1, ImVec2 cp2, float y,
                       const char* key, const char* val, ImU32 valCol) {
        float rowY = cp1.y + y;
        draw->AddText(titleFont, 16.0f * scale_y,
            ImVec2(cp1.x + 34.0f * scale_y, rowY), textDim, key);
        ImVec2 vSz = titleFont->CalcTextSizeA(16.0f * scale_y, FLT_MAX, 0, val);
        draw->AddText(titleFont, 16.0f * scale_y,
            ImVec2(cp2.x - 34.0f * scale_y - vSz.x, rowY),
            valCol, val);

        draw->AddLine(
            ImVec2(cp1.x + 28.0f * scale_y, rowY + 26.0f * scale_y),
            ImVec2(cp2.x - 28.0f * scale_y, rowY + 26.0f * scale_y),
            IM_COL32(50, 50, 70, 255), 1.2f);
    };

    // LEFT PANEL
    const char* leftTitle  = "";
    ImU32       leftColor  = accent;
    if (g_NexusTab == 0)      { leftTitle = "AIM PANEL";    leftColor = purple; }
    else if (g_NexusTab == 1) { leftTitle = "VISUAL PANEL"; leftColor = accent2; }
    else if (g_NexusTab == 2) { leftTitle = "MISC PANEL";   leftColor = accent3; }
    else                       { leftTitle = "CONFIG PANEL"; leftColor = purple; }

    DrawPanel(c1P1, c1P2, leftTitle, leftColor);

    float rowStep = 50.0f * scale_y;
    float rowBase = 76.0f * scale_y;

    if (g_NexusTab == 0) {
        ModuleRow(c1P1, c1P2, rowBase,               "AIM BOT", &Aimbot,     "n_aim1", purple);
        ModuleRow(c1P1, c1P2, rowBase + rowStep,     "SILENT",  &AimSilent,  "n_aim2", purple);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 2, "VISIBLE", &AimVisible, "n_aim3", purple);
        // UI-only toggle placeholder: intentionally not connected to game-memory/aim logic.
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 3, "AIM MAGNET", &g_AimMagnetUi, "n_aim_magnet_ui", purple);

        float fovY = c1P1.y + rowBase + rowStep * 3 + 64.0f * scale_y;
        draw->AddText(titleFont, 16.0f * scale_y,
            ImVec2(c1P1.x + 34.0f * scale_y, fovY), textDim, "FOV");

        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", AimFov);
        ImVec2 bSz2 = titleFont->CalcTextSizeA(16.0f * scale_y, FLT_MAX, 0, buf);
        draw->AddText(titleFont, 16.0f * scale_y,
            ImVec2(c1P2.x - 34.0f * scale_y - bSz2.x, fovY), accent2, buf);

        float tx1 = c1P1.x + 34.0f * scale_y;
        float tx2 = c1P2.x - 34.0f * scale_y;
        float ty  = fovY + 30.0f * scale_y;
        float th  = 5.0f;

        draw->AddRectFilled(ImVec2(tx1, ty), ImVec2(tx2, ty + th),
                            IM_COL32(55, 55, 75, 255), 2.5f);

        float t = AimFov / 360.0f;
        float kx = tx1 + (tx2 - tx1) * t;

        draw->AddRectFilledMultiColor(
            ImVec2(tx1, ty), ImVec2(kx, ty + th),
            accent, accent2, accent2, accent);

        draw->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), 11.0f * scale_y, accent);
        draw->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), 7.5f * scale_y, IM_COL32(255, 255, 255, 255));
        draw->AddCircleFilled(ImVec2(kx, ty + th * 0.5f), 4.0f * scale_y, accent);

        ImGui::SetCursorScreenPos(ImVec2(tx1, ty - 12.0f));
        ImGui::PushID("nexus_fov");
        ImGui::InvisibleButton("##fov", ImVec2(tx2 - tx1, 30.0f));
        if (ImGui::IsItemActive()) {
            float m = (ImGui::GetMousePos().x - tx1) / (tx2 - tx1);
            m = std::max(0.0f, std::min(1.0f, m));
            AimFov = m * 360.0f;
        }
        ImGui::PopID();
    }
    else if (g_NexusTab == 1) {
        ModuleRow(c1P1, c1P2, rowBase,               "BOX ESP",  &showBox,      "n_vis1", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep,     "LINE",     &showLine,     "n_vis2", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 2, "HEALTH",   &showHealth,   "n_vis3", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 3, "NAME",     &showName,     "n_vis4", cyan);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 4, "DISTANCE", &showDistance, "n_vis5", cyan);
    }
    else if (g_NexusTab == 2) {
        ModuleRow(c1P1, c1P2, rowBase,               "SPEED",     &SpeedHackV2, "n_msc1", yellow);
        ModuleRow(c1P1, c1P2, rowBase + rowStep,     "GHOST",     &GhostHack,   "n_msc2", yellow);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 2, "NO RELOAD", &NoReload,    "n_msc3", yellow);
        ModuleRow(c1P1, c1P2, rowBase + rowStep * 3, "NO RECOIL", &NoRecoil,    "n_msc4", yellow);
    }
    else {
        float y0 = c1P1.y + 80.0f * scale_y;

        draw->AddText(titleFont, 16.0f * scale_y,
            ImVec2(c1P1.x + 34.0f * scale_y, y0), textMain, "Box Color");

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 34.0f * scale_y, y0 + 26.0f * scale_y));
        ImGui::PushID("n_boxcol");
        ImGui::ColorEdit4("##bc", (float*)&boxCustomColor,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 88.0f * scale_y, y0 + 26.0f * scale_y));
        ImGui::SetNextItemWidth(cardW - 122.0f * scale_y);
        const char* colorModes[] = { "Custom", "Health", "Distance", "Rainbow", "Cyan",
                                     "Green", "Purple", "Orange", "Red", "White" };
        ImGui::Combo("##bm", &boxColorMode, colorModes, IM_ARRAYSIZE(colorModes));

        float y1 = y0 + 80.0f * scale_y;
        draw->AddText(titleFont, 16.0f * scale_y,
            ImVec2(c1P1.x + 34.0f * scale_y, y1), textMain, "Line Color");

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 34.0f * scale_y, y1 + 26.0f * scale_y));
        ImGui::PushID("n_linecol");
        ImGui::ColorEdit4("##lc", (float*)&lineCustomColor,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(ImVec2(c1P1.x + 88.0f * scale_y, y1 + 26.0f * scale_y));
        ImGui::SetNextItemWidth(cardW - 122.0f * scale_y);
        ImGui::Combo("##lm", &lineColorMode, colorModes, IM_ARRAYSIZE(colorModes));

        float y2 = y1 + 80.0f * scale_y;
        draw->AddText(titleFont, 16.0f * scale_y,
            ImVec2(c1P1.x + 34.0f * scale_y, y2), textMain, "Theme");

        for (int i = 0; i < 6; i++) {
            ImGui::SetCursorScreenPos(ImVec2(
                c1P1.x + 34.0f * scale_y + (float)i * 48.0f * scale_y,
                y2 + 26.0f * scale_y));
            ImGui::PushID(700 + i);
            if (ImGui::ColorButton("##sw", g_ThemeSwatches[i], 0, ImVec2(38, 38))) {
                g_ThemeColorArr[0] = g_ThemeSwatches[i].x;
                g_ThemeColorArr[1] = g_ThemeSwatches[i].y;
                g_ThemeColorArr[2] = g_ThemeSwatches[i].z;
                g_ThemeColorArr[3] = g_ThemeSwatches[i].w;
            }
            ImGui::PopID();
        }

        // DARK MODE
        ImGui::SetCursorScreenPos(ImVec2(
            c1P1.x + 34.0f * scale_y,
            y2 + 90.0f * scale_y));
        ImGui::PushStyleColor(ImGuiCol_Button,
            g_darkMode ? IM_COL32(80, 80, 220, 220) : IM_COL32(255, 200, 80, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            g_darkMode ? IM_COL32(110, 110, 240, 255) : IM_COL32(255, 220, 120, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            g_darkMode ? IM_COL32(60, 60, 200, 255) : IM_COL32(230, 180, 60, 255));
        if (ImGui::Button(g_darkMode ? "DARK MODE" : "LIGHT MODE",
                          ImVec2(170.0f * scale_y, 45.0f * scale_y))) {
            g_darkMode = !g_darkMode;
        }
        ImGui::PopStyleColor(3);

        // EXIT
        ImGui::SetCursorScreenPos(ImVec2(
            c1P2.x - 204.0f * scale_y,
            y2 + 90.0f * scale_y));
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(220, 60, 60, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 90, 90, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(180, 40, 40, 255));
        if (ImGui::Button("EXIT", ImVec2(170.0f * scale_y, 45.0f * scale_y))) {
            exit(0);
        }
        ImGui::PopStyleColor(3);
    }

    // RIGHT PANEL — SYSTEM INFO
    DrawPanel(c2P1, c2P2, "SYSTEM INFO", accent2);

    float infoStart = 76.0f * scale_y;
    float infoStep  = 50.0f * scale_y;

    InfoRow(c2P1, c2P2, infoStart,
            "STATUS", connected ? "READY" : "OFFLINE",
            connected ? green : redColor);

    InfoRow(c2P1, c2P2, infoStart + infoStep,
            "TARGET", connected ? "LOCKED" : "NONE",
            connected ? accent3 : textDim);

    InfoRow(c2P1, c2P2, infoStart + infoStep * 2,
            "ENGINE", "ninmods-kernel", purple);

    InfoRow(c2P1, c2P2, infoStart + infoStep * 3,
            "PING", "12 ms", green);

    InfoRow(c2P1, c2P2, infoStart + infoStep * 4,
            "MODE", (const char*[]){ "AIM", "VISUALS", "MISC", "CONFIG" }[g_NexusTab],
            purple);

    const char* coreTxt = "N I N M O D S";
    float coreSize = 30.0f * scale_y;
    ImVec2 coreSz = titleFont->CalcTextSizeA(coreSize, FLT_MAX, 0, coreTxt);
    ImVec2 corePos = ImVec2(
        c2P1.x + (cardW - coreSz.x) * 0.5f,
        c2P2.y - 80.0f * scale_y);
    draw->AddText(titleFont, coreSize, corePos, purple, coreTxt);

    const char* cap = "K E R N E L   C O R E";
    float capSize = 12.0f * scale_y;
    ImVec2 capSz = titleFont->CalcTextSizeA(capSize, FLT_MAX, 0, cap);
    draw->AddText(titleFont, capSize,
        ImVec2(c2P1.x + (cardW - capSz.x) * 0.5f, c2P2.y - 40.0f * scale_y),
        textDim, cap);

    // FOOTER
    const char* foot = "N E X U S   C O R E";
    float footSize = 16.0f * scale_y;
    ImVec2 fSz = titleFont->CalcTextSizeA(footSize, FLT_MAX, 0, foot);
    float fx = wp.x + (W - fSz.x) * 0.5f;
    float fy = H - 44.0f * scale_y;

    draw->AddLine(
        ImVec2(wp.x + pad, fy + 12.0f * scale_y),
        ImVec2(wp.x + W * 0.30f, fy + 12.0f * scale_y),
        accent2, 1.5f);
    draw->AddLine(
        ImVec2(wp.x + W * 0.70f, fy + 12.0f * scale_y),
        ImVec2(wp.x + W - pad, fy + 12.0f * scale_y),
        accent2, 1.5f);

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

    ImGui::End();
}

//  FONTS

void LoadCustomIconFonts(ImGuiIO& io) {
    io.Fonts->AddFontFromMemoryTTF((void*)inter_medium, sizeof(inter_medium), 20.f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    g_BoldFont = io.Fonts->AddFontFromMemoryTTF((void*)inter_bold, sizeof(inter_bold), 23.f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    g_IconFont = io.Fonts->AddFontFromMemoryTTF((void*)F107_data, F107_size, 25.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    io.Fonts->Build();
}

//  PROCESS HELPER

bool isProcessAlive(int pid) {
    if (pid <= 0) return false;
    char path[64];
    sprintf(path, oxorany("/proc/%d"), pid);
    struct stat st;
    return (stat(path, &st) == 0);
}

//  MAIN

int main(int argc, char *argv[]) {
    crash_guard::install();
    game_pid = 0;
    lib_base = 0;
    lib_base2 = 0;

    printf("[*] Probing display dimensions and screen orientation...\n");
    fflush(stdout);

    screen_config();
    if (displayInfo.orientation == 0) {
        displayInfo.orientation = 1; // Default to landscape for gaming overlay
    }

    ::abs_ScreenX = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::abs_ScreenY = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_x = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    ::native_window_screen_y = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    printf("[*] Screen dimensions: %dx%d (Orientation: %u)\n", native_window_screen_x, native_window_screen_y, displayInfo.orientation);
    printf("[*] Configuring Android system touch security policies...\n");
    fflush(stdout);
    system("settings put global block_untrusted_touches 0 2>/dev/null; settings put system block_untrusted_touches 0 2>/dev/null");

    printf("[*] Creating SurfaceComposerClient Native Window and initializing EGL...\n");
    fflush(stdout);

    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) {
        printf("[-] Failed to initialize overlay GUI\n");
        fflush(stdout);
        return -1;
    }
    printf("[+] Floating overlay engine initialized successfully!\n");
    printf("[*] Attaching Linux input touch listener...\n");
    printf("[+] ninmods starting - daemon active in RAM\n");
    printf("[+] Kernel engine active. Awaiting target attach...\n");
    fflush(stdout);

    ImGuiIO& io = ImGui::GetIO();
    LoadCustomIconFonts(io);

    Touch_Init(native_window_screen_x, native_window_screen_y, displayInfo.orientation, true);

    InitTouchAimbot();

    // Load persisted logo coordinates
    LoadLogoPosition();

    std::thread attach_worker([]() {
        LOG_INFO("Kernel attach daemon worker active. Polling game process and screen orientation.");
        while (main_thread_flag) {
            display_safe::probe_orientation();
            if (game_pid <= 0) {
                game_pid = getProcessID(packageName);
                if (game_pid > 0) {
                    Memory::g_pid = (pid_t)game_pid;
                    driver->initialize((pid_t)game_pid);
                    lib_base = 0;
                    lib_base2 = 0;
                    LOG_INFO("Runtime subsystem attached (PID: %d)", game_pid);
                    printf("[+] Runtime subsystem attached (PID: %d)\n", game_pid);
                    fflush(stdout);
                }
            } else {
                if (!isProcessAlive(game_pid)) {
                    LOG_INFO("Session detached (target game terminated). Waiting for reconnect...");
                    printf("[*] Session detached. Waiting for reconnect...\n");
                    fflush(stdout);
                    game_pid = 0;
                    lib_base = 0;
                    lib_base2 = 0;
                }
            }
            if (game_pid > 0 && lib_base == 0) {
                lib_base = get_module_base(targetaLibName, PERM_EXEC);
                if (lib_base > 0) {
                    lib_base2 = get_module_base(targetaLibName2, PERM_EXEC);
                    LOG_INFO("Target game libraries mapped: lib1=0x%lx, lib2=0x%lx", (unsigned long)lib_base, (unsigned long)lib_base2);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
    attach_worker.detach();

    while (main_thread_flag) {
        drawBegin();
        {
            // Floating logo button remains visible (draggable overlay button)
            RenderFloatingLogo();

            // Render main menu when toggled visible
            if (g_ShowNexusMenu) RenderMenu();

            // ESP / Aimbot
            if (game_pid > 0 && lib_base > 0) {
                DrawESP(abs_ScreenX, abs_ScreenY, lib_base);
                AimbotMenu(abs_ScreenX, abs_ScreenY, lib_base);
                TouchAimbotTick(g_ShowNexusMenu);
            }
        }
        drawEnd();
        usleep(16000);
    }

    // Persist anchor position on shutdown
    SaveLogoPosition();

    shutdown();
    Touch_Close();
    return 0;
}
