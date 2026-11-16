// src/globals.cpp
#include "globals.hpp"

std::atomic<float> g_sw{1920.0f};
std::atomic<float> g_sh{1080.0f};

std::atomic<bool> main_thread_flag{true};
