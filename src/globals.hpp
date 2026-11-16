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
