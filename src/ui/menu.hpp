#pragma once

#include "imgui.h"

namespace ui::menu {
    void render();
    void load_logo_pos();
    void save_logo_pos();
}

extern bool g_ShowNexusMenu;
extern float g_ThemeColorArr[4];
#define g_ThemeColor ImVec4(g_ThemeColorArr[0], g_ThemeColorArr[1], g_ThemeColorArr[2], g_ThemeColorArr[3])

// Scarecrow cheat toggles defined in hook.h (linked via main.cpp)
extern bool Aimbot;
extern bool AimLock;
extern bool AimSilent;
extern bool AimVisible;
extern float AimFov;

extern bool showBox;
extern bool showLine;
extern bool showSkeleton;
extern bool showHealth;
extern bool showDistance;
extern bool showName;
extern bool showCount;
extern bool is3600;

extern int boxColorMode;
extern ImVec4 boxCustomColor;
extern int lineColorMode;
extern ImVec4 lineCustomColor;

extern bool SpeedHackV2;
extern bool GhostHack;
extern bool NoReload;
extern bool NoRecoil;

extern int game_pid;
extern uint64_t lib_base;
