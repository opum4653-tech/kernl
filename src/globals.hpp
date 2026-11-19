#pragma once
#include <atomic>
#include <cstdint>

// ==============================================================================================
// FILE: src/globals.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Global atomic screen dimensions, engine lifecycle flags, and typography font handles.
//
// CROSS-FILE USAGE & DATA FLOW:
// ----------------------------------------------------------------------------------------------
// 1. ATOMIC SCREEN RESOLUTION (g_sw, g_sh):
//    - Writer: src/main.cpp -> start_overlay() updates on display initialization and device rotation.
//    - Readers:
//      * src/game/math.hpp -> WorldToScreenPoint() uses snapshot to compute perspective NDC projection.
//      * src/func/aim/fov.hpp -> render_fov() calculates screen center (sw/2, sh/2).
//      * src/func/aim/target.hpp -> acquire_target() calculates crosshair center.
//      * src/func/esp/common.hpp -> computePlayerBox() bounds checks.
//
// 2. ENGINE RUNTIME LIFECYCLE (main_thread_flag):
//    - Checked by main loop in src/main.cpp. Set to false when EXIT OVERLAY button is pressed.
//
// 3. TYPOGRAPHY HANDLES (g_UiFont, g_EspFont, g_LogoFont):
//    - Built and populated during overlay init in includes/draw/Android_draw/draw.cpp.
// ==============================================================================================

// Atomic viewport width and height in physical display pixels
extern std::atomic<float> g_sw;
extern std::atomic<float> g_sh;

// Global engine running flag: controls main render while loop
extern std::atomic<bool> main_thread_flag;

// Typography font handles rasterized at native rendering sizes
struct ImFont;
extern ImFont* g_UiFont;    // 18px body font for menu text and controls
extern ImFont* g_EspFont;   // 14px font for ESP tags and distance markers
extern ImFont* g_LogoFont;  // 64px display font for floating logo "N" glyph
