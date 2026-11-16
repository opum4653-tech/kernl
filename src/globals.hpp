#pragma once
#include <atomic>
#include <cstdint>

// Screen size is written by the render thread and read by the data and aim
// worker threads (WorldToScreenPoint, FOV ring). Plain floats there meant a
// projected coordinate could be computed against a half-updated pair during a
// rotation, so the value is atomic. Implicit conversion keeps every existing
// arithmetic site unchanged.
extern std::atomic<float> g_sw;
extern std::atomic<float> g_sh;

// Written by the render thread when the menu requests shutdown, read by the
// main loop each frame. Plain bool here is a cross-thread data race.
extern std::atomic<bool> main_thread_flag;

// Font faces registered by initGUI_draw() in draw.cpp. Each is rasterised at
// the size its consumers actually draw at, instead of sampling the 32px menu
// face (Fonts[0]) up or down:
//   g_UiFont   - 18px, menu body labels (toggle rows draw at 17px)
//   g_EspFont  - 14px, ESP labels and header sub-text (draw at 10..14px)
//   g_LogoFont - 64px, the floating logo "N" (drawn at radius*1.15, 44..63px)
// Forward declared so this header stays free of imgui.h.
struct ImFont;
extern ImFont* g_UiFont;
extern ImFont* g_EspFont;
extern ImFont* g_LogoFont;
