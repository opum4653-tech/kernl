// src/globals.cpp
#include <atomic>
#include <cstdint>

float global_anim_time = 0.0f;
float g_ui_scale = 1.0f;
float g_sw = 1920.0f;
float g_sh = 1080.0f;
std::atomic<bool> menu_visible{false};

int abs_ScreenX = 1920;
int abs_ScreenY = 1080;
bool main_thread_flag = true;
