#pragma once
#include <atomic>
#include "imgui.h"
#include "../globals.hpp"

namespace cfg {
    inline std::atomic<bool> master_enabled{false}; // Global Master Switch

namespace esp {
    inline std::atomic<bool>  box              {false}; // ESP Box (Chaturbhuj)
    inline std::atomic<bool>  armor            {false}; // ESP Armor Bar
    inline std::atomic<bool>  tracer           {false}; // ESP Laser Line
    inline std::atomic<bool>  bot_tag          {false}; // ESP Bot Tag (AI)
    inline std::atomic<float> max_range        {250.f}; // Max Render Distance Slider (50m - 500m)

    // Visual styling constants
    inline std::atomic<int>   box_type         {1};     // 1 = Full Chaturbhuj Box
    inline std::atomic<float> box_rounding     {3.f};
    inline std::atomic<float> box_thickness    {2.0f};
    inline std::atomic<float> tracer_thickness {1.2f};
    inline std::atomic<int>   tracer_pos       {0};     // 0 = Top, 1 = Center, 2 = Bottom

    // ESP Accent Color (Unified Signature Purple #E10098)
    inline ImVec4 box_col                      = ImVec4(0.882f, 0.000f, 0.596f, 1.0f);
}

namespace aim {
    inline std::atomic<int>   read_interval_ms {16};   // Syscall cadence (ms)
    inline std::atomic<bool>  silent           {false}; // Aim Silent
    inline std::atomic<bool>  scope_aim        {false}; // Scope Aim (Instant Head)
    inline std::atomic<bool>  auto_headshot    {false}; // Auto Headshot (100% Red)
    inline std::atomic<bool>  ignore_knocked   {false}; // Ignore Knocked
    inline std::atomic<bool>  show_fov         {false}; // Draw FOV Circle
    inline std::atomic<float> head_bias        {0.22f}; // Headshot Elevation Bias Slider (0.10m - 0.35m)
    inline std::atomic<float> aim_fov          {35.0f}; // FOV range in degrees
    inline std::atomic<float> fov_radius       {180.0f};

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

namespace motion {
    inline std::atomic<bool>  speed_hack       {false}; // Speed Hack (Anti-Ban Safe)
    inline std::atomic<float> speed_mult       {1.20f}; // 1.00x to 1.30x clamped
    inline std::atomic<bool>  no_recoil        {false}; // No Recoil
}

inline bool any_feature_enabled() {
    if (!master_enabled) return false;
    return esp::box || esp::armor || esp::tracer || esp::bot_tag ||
           aim::silent || aim::scope_aim || aim::auto_headshot ||
           motion::speed_hack || motion::no_recoil;
}

inline bool Checkbox(const char* label, std::atomic<bool>& v) {
    bool tmp = v.load(std::memory_order_relaxed);
    if (ImGui::Checkbox(label, &tmp)) {
        v.store(tmp, std::memory_order_relaxed);
        return true;
    }
    return false;
}

inline bool SliderFloat(const char* id, std::atomic<float>& v, float lo, float hi, const char* fmt) {
    float tmp = v.load(std::memory_order_relaxed);
    if (ImGui::SliderFloat(id, &tmp, lo, hi, fmt)) {
        v.store(tmp, std::memory_order_relaxed);
        return true;
    }
    return false;
}

inline bool SliderInt(const char* id, std::atomic<int>& v, int lo, int hi, const char* fmt) {
    int tmp = v.load(std::memory_order_relaxed);
    if (ImGui::SliderInt(id, &tmp, lo, hi, fmt)) {
        v.store(tmp, std::memory_order_relaxed);
        return true;
    }
    return false;
}

} // namespace cfg
