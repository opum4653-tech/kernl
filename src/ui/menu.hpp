#pragma once

#include "imgui.h"
#include <cstdint>

namespace ui::menu {
    void render();
    void load_logo_pos();
    void save_logo_pos();
}

extern bool g_ShowMenu;
extern float g_ThemeColorArr[4];
#define g_ThemeColor ImVec4(g_ThemeColorArr[0], g_ThemeColorArr[1], g_ThemeColorArr[2], g_ThemeColorArr[3])

extern bool main_thread_flag;
