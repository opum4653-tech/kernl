#pragma once
#include <atomic>
#include <cstdint>

extern float global_anim_time;
extern float g_ui_scale;
extern float g_sw;
extern float g_sh;
extern std::atomic<bool> menu_visible;

extern int abs_ScreenX;
extern int abs_ScreenY;
extern bool main_thread_flag;
