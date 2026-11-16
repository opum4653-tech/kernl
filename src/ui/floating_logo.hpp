#pragma once
#include "imgui.h"

namespace ui::floating_logo {
    void render(float realW, float realH, float ui_scale);
    void save_pos();
    void load_pos();
    ImVec2 get_pos();
    void set_pos(ImVec2 pos);
}
