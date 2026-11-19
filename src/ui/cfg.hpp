#pragma once
#include <atomic>
#include "imgui.h"
#include "../globals.hpp"

// ==============================================================================================
// FILE: src/ui/cfg.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Central atomic configuration registry governing all 16 active features.
//
// CROSS-FILE CALL GRAPH & USAGE MAPPINGS:
// ----------------------------------------------------------------------------------------------
// 1. WRITERS:
//    - src/ui/menu.cpp -> ui::menu::render():
//      * Binds UI widgets directly to atomic values via cfg::Checkbox() and cfg::SliderFloat().
//
// 2. READERS (Lock-Free Cross-Thread Consumers):
//    - Visuals (ESP Subsystem):
//      * src/func/esp/esp.hpp    -> Reads box, armor, tracer, bot_tag, max_range.
//      * src/func/esp/health.hpp -> Reads cfg::esp::armor.
//      * src/func/esp/info.hpp   -> Reads cfg::esp::bot_tag.
//      * src/func/esp/line.hpp   -> Reads cfg::esp::tracer, tracer_pos, tracer_thickness.
//    - Combat (Aimbot Subsystem):
//      * src/func/aim/aim.hpp    -> Reads silent, scope_aim, auto_headshot.
//      * src/func/aim/target.hpp -> Reads ignore_knocked, get_fov_radius(), scope_aim.
//      * src/func/aim/silent.hpp -> Reads head_bias, silent, auto_headshot, scope_aim.
//      * src/func/aim/fov.hpp    -> Reads show_fov, get_fov_radius().
//    - Motion & Memory Mods:
//      * src/func/speed.hpp      -> Reads speed_hack, speed_mult, no_recoil.
//    - Memory Scraper Engine:
//      * src/game/data.cpp       -> Reads any_feature_enabled() and cfg::esp::max_range.
// ==============================================================================================

namespace cfg {
    // Master System Switch: master enable gate for all drawing and aim features
    inline std::atomic<bool> master_enabled{false};

// ----------------------------------------------------------------------------------------------
// TAB 0: VISUALS (ESP) CONFIGURATION
// ----------------------------------------------------------------------------------------------
namespace esp {
    inline std::atomic<bool>  box              {false}; // Feature 2: ESP Box (Chaturbhuj)
    inline std::atomic<bool>  armor            {false}; // Feature 3: ESP Armor Bar
    inline std::atomic<bool>  tracer           {false}; // Feature 4: ESP Laser Line
    inline std::atomic<bool>  bot_tag          {false}; // Feature 5: ESP Bot Tag (AI)
    inline std::atomic<float> max_range        {250.f}; // Feature 6: Max Render Distance Slider (50m - 500m)

    // Visual geometry and style parameters
    inline std::atomic<int>   box_type         {1};     // 1 = Full Chaturbhuj 2D Box
    inline std::atomic<float> box_rounding     {3.f};   // Corner rounding radius
    inline std::atomic<float> box_thickness    {2.0f};  // Border stroke thickness
    inline std::atomic<float> tracer_thickness {1.2f};  // Laser line stroke thickness
    inline std::atomic<int>   tracer_pos       {0};     // 0 = Screen Top, 1 = Center, 2 = Bottom

    // Feature 7: ESP Accent Color (Unified Signature Purple #E10098)
    inline ImVec4 box_col                      = ImVec4(0.882f, 0.000f, 0.596f, 1.0f);
}

// ----------------------------------------------------------------------------------------------
// TAB 1: COMBAT (AIMBOT) CONFIGURATION
// ----------------------------------------------------------------------------------------------
namespace aim {
    inline std::atomic<int>   read_interval_ms {16};    // Syscall polling cadence (ms)
    inline std::atomic<bool>  silent           {false}; // Feature 8: Aim Silent (Unnormalized Vector)
    inline std::atomic<bool>  scope_aim        {false}; // Feature 9: Scope Aim (Instant Head)
    inline std::atomic<bool>  auto_headshot    {false}; // Feature 10: Auto Headshot (100% Red)
    inline std::atomic<bool>  ignore_knocked   {false}; // Feature 11: Ignore Knocked
    inline std::atomic<bool>  show_fov         {false}; // Feature 12: Draw FOV Circle
    inline std::atomic<float> head_bias        {0.00f}; // Feature 13: Headshot Elevation Bias (-0.15m to +0.15m)
    inline std::atomic<float> aim_fov          {35.0f}; // FOV kill-zone range in degrees
    inline std::atomic<float> fov_radius       {180.0f};// Computed pixel radius

    // Calculates screen pixel radius corresponding to current FOV degree angle
    inline float get_fov_radius() {
        ImGuiIO& io = ImGui::GetIO();
        float currentSw = g_sw.load(std::memory_order_relaxed);
        float sw = (currentSw > 0.0f) ? currentSw : ((io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : 2400.0f);
        float deg = aim_fov.load(std::memory_order_relaxed);
        if (deg < 10.0f) deg = 10.0f;
        if (deg > 180.0f) deg = 180.0f;
        if (sw > 0.0f) {
            float r = (deg / 180.0f) * (sw * 0.50f);
            if (r < 30.0f) r = 30.0f;
            if (r > sw * 0.50f) r = sw * 0.50f;
            return r;
        }
        return 180.0f;
    }
}

// ----------------------------------------------------------------------------------------------
// TAB 2: MODS & SYSTEM CONFIGURATION
// ----------------------------------------------------------------------------------------------
namespace motion {
    inline std::atomic<bool>  speed_hack       {false}; // Feature 14: Speed Hack (Anti-Ban Safe)
    inline std::atomic<float> speed_mult       {1.20f}; // Clamped to 1.00x - 1.30x
    inline std::atomic<bool>  no_recoil        {false}; // Feature 15: No Recoil
}

// Returns true if at least one drawing or memory modification feature is toggled on
inline bool any_feature_enabled() {
    if (!master_enabled) return false;
    return esp::box || esp::armor || esp::tracer || esp::bot_tag ||
           aim::silent || aim::scope_aim || aim::auto_headshot ||
           motion::speed_hack || motion::no_recoil;
}

// ----------------------------------------------------------------------------------------------
// IMGUI ATOMIC BINDING HELPERS
// ----------------------------------------------------------------------------------------------
// Seamlessly bridges std::atomic<bool> to ImGui::Checkbox without data races
inline bool Checkbox(const char* label, std::atomic<bool>& v) {
    bool tmp = v.load(std::memory_order_relaxed);
    if (ImGui::Checkbox(label, &tmp)) {
        v.store(tmp, std::memory_order_relaxed);
        return true;
    }
    return false;
}

// Seamlessly bridges std::atomic<float> to ImGui::SliderFloat
inline bool SliderFloat(const char* id, std::atomic<float>& v, float lo, float hi, const char* fmt) {
    float tmp = v.load(std::memory_order_relaxed);
    if (ImGui::SliderFloat(id, &tmp, lo, hi, fmt)) {
        v.store(tmp, std::memory_order_relaxed);
        return true;
    }
    return false;
}

// Seamlessly bridges std::atomic<int> to ImGui::SliderInt
inline bool SliderInt(const char* id, std::atomic<int>& v, int lo, int hi, const char* fmt) {
    int tmp = v.load(std::memory_order_relaxed);
    if (ImGui::SliderInt(id, &tmp, lo, hi, fmt)) {
        v.store(tmp, std::memory_order_relaxed);
        return true;
    }
    return false;
}

} // namespace cfg
