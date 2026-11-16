#pragma once

#include "imgui.h"
#include "theme.hpp"
#include "../globals.hpp"
#include <cstdint>

namespace ui::menu {
    void render();
    void load_logo_pos();
    void save_logo_pos();
}

extern bool g_ShowMenu;
