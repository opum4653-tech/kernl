// src/globals.cpp
#include "globals.hpp"

std::atomic<float> g_sw{1920.0f};
std::atomic<float> g_sh{1080.0f};

// Registered by initGUI_draw() in draw.cpp; null until then.
ImFont* g_UiFont = nullptr;
ImFont* g_EspFont = nullptr;
ImFont* g_LogoFont = nullptr;

std::atomic<bool> main_thread_flag{true};
