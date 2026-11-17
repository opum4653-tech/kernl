#pragma once
#include <atomic>
#include "imgui.h"
#include "../globals.hpp"

// Feature knobs are toggled by the render thread and read by the data and aim
// worker threads every tick (any_feature_enabled, target acquisition, motion).
// Plain bool/float here meant a worker could observe a torn float (max_range,
// aim_fov) or a value the UI had already replaced. Atomic storage makes each
// knob a single load. Implicit conversion keeps every arithmetic read site
// unchanged; the widgets go through the small helpers below.
namespace cfg {
    inline std::atomic<bool> master_enabled{false}; // Global Master Switch: controls all features, preserves selection state

namespace esp {
// Master
    inline std::atomic<bool> box              {false};
    inline std::atomic<int>  box_type         {1}; // 1 = Full Chaturbhuj (Rectangular) Box
    inline std::atomic<bool> name             {false};
    inline std::atomic<bool> health           {false};
    inline std::atomic<bool> armor            {false};
    inline std::atomic<bool> distance         {false};
    inline std::atomic<bool> weapon           {false};
    inline std::atomic<bool> bot_tag          {false};
    inline std::atomic<bool> ignore_bots      {false};
    inline std::atomic<bool> tracer           {false};
    inline std::atomic<int>  tracer_pos       {0}; // 0 = Top, 1 = Center / Crosshair, 2 = Bottom
    inline std::atomic<bool> box_filled       {false}; // Subtle filled background box
    inline std::atomic<bool> crosshair        {false};
    inline std::atomic<bool> firing_ind       {false};
    inline std::atomic<bool> loot             {false};

// Setting
    inline std::atomic<float> max_range        {250.f};
    inline std::atomic<float> loot_range       {150.f};
    inline std::atomic<float> box_rounding     {3.f};
    inline std::atomic<float> box_thickness    {2.0f};
    inline std::atomic<float> tracer_thickness {1.2f};
    inline std::atomic<float> bracket_frac     {0.28f};

// Warna — written and read on the render thread only, plain storage.
    inline ImVec4 box_col         = ImVec4(0.0f, 0.90f, 1.0f, 1.0f);
    inline ImVec4 box_visible     = ImVec4(0.30f, 1.0f, 0.55f, 1.0f);
    inline ImVec4 name_col        = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    inline ImVec4 distance_col    = ImVec4(0.95f, 0.95f, 0.30f, 1.0f);
    inline ImVec4 weapon_col      = ImVec4(1.0f, 0.85f, 0.30f, 1.0f);
}

namespace aim {
    inline std::atomic<int>   read_interval_ms{16};   // Read loop cadence (ms). Higher = lower syscall frequency.
    inline std::atomic<bool>  enable          {false}; // Traditional Aim Assist
    inline std::atomic<bool>  auto_headshot   {false}; // 100% Auto Headshot (Locks HeadCollider, HitGroup=1, Head 3D)
    inline std::atomic<bool>  silent          {false}; // Aim Silent: direct ray trajectory steering, zero camera movement
    inline std::atomic<bool>  scope_aim       {false}; // Off by default. Enabling adds a write path on scope.
    inline std::atomic<bool>  sniper_aim      {false}; // Off by default. Enabling forces head lock, ignoring headshot_rate.
    inline std::atomic<bool>  show_fov        {false};
    inline std::atomic<float> aim_fov         {35.0f};  // AimFov (0° - 180°), matching reference calculation
    inline std::atomic<float> fov_radius      {180.0f}; // Fallback pixel radius
    inline std::atomic<float> smooth          {5.0f};   // Safe default: smooth lerp (1=snap, 10=slow). Range 3-8 looks natural.
    inline std::atomic<float> headshot_rate   {100.0f}; // 100% default headshot rate when active.
    inline std::atomic<int>   target_bone     {0}; // 0 = Head, 1 = Neck/Chest, 2 = Pelvis/Body
    inline std::atomic<int>   trigger_mode    {0}; // 0 = When Firing, 1 = Always In FOV
    inline std::atomic<int>   priority_mode   {0}; // 0 = Crosshair Distance, 1 = Low HP, 2 = 3D Distance
    inline std::atomic<bool>  ignore_bots     {false};
    inline std::atomic<bool>  ignore_knocked  {false};
    inline std::atomic<bool>  visible_check   {false};
    inline std::atomic<float> max_distance    {200.0f};

    inline float get_fov_radius() {
        if (aim_fov > 0.0f && g_sw > 0.0f) {
            return (aim_fov / 180.0f) * (g_sw * 0.55f);
        }
        return fov_radius;
    }
}

namespace motion {
    inline std::atomic<bool>  air_flight    {false}; // Air Flight elevation modifier
    inline std::atomic<float> flight_height {4.5f};  // Elevation height in meters
    inline std::atomic<bool>  phantom_mode  {false}; // Phantom Mode entity visibility
    inline std::atomic<bool>  speed_hack    {false}; // Kernel-driven speed multiplier
    inline std::atomic<float> speed_mult    {1.20f}; // 1.00x to 1.30x (Anti-Ban Safe)
    inline std::atomic<bool>  no_recoil     {false}; // Pure kernel driver recoil reduction
    inline std::atomic<bool>  no_reload     {false}; // Instant shot / zero reload wait
    inline std::atomic<bool>  ghost_hack    {false}; // Ghost mode
    inline std::atomic<bool>  ipad_view     {false}; // Wide FOV / iPad View (0x36C9BD0)
    inline std::atomic<float> ipad_val      {1.4f};  // 1.0x to 2.5x camera FOV scale
    inline std::atomic<bool>  pickup_range  {false}; // Extended Auto-Loot range (0x1EB4)
    inline std::atomic<float> pickup_val    {15.0f}; // 2.0 to 30.0 meters
    inline std::atomic<bool>  hitbox_scale  {false}; // Hitbox & Player Size Scaler (0x1FC, 0x200, 0x204)
    inline std::atomic<float> hitbox_size   {1.5f};  // 1.0x to 3.0x
    inline std::atomic<bool>  high_jump     {false}; // High Jump elevation (0x21C)
    inline std::atomic<float> high_jump_val {2.5f};  // 1.0x to 5.0x
}

inline bool any_feature_enabled() {
    if (!master_enabled) return false;
    return aim::enable || aim::silent || esp::box || esp::health || esp::armor ||
           esp::name || esp::distance || esp::weapon ||
           esp::tracer || esp::loot || esp::crosshair || esp::firing_ind ||
           motion::air_flight || motion::phantom_mode || motion::speed_hack ||
           motion::no_recoil || motion::no_reload || motion::ghost_hack ||
           motion::ipad_view || motion::pickup_range || motion::hitbox_scale || motion::high_jump;
}

// ImGui widgets take a raw pointer, which atomic storage cannot hand out. These
// overloads edit a stack temporary and store the result back as one relaxed
// write, so a worker thread never sees an in-flight knob value. The custom pill
// toggle and segmented control take atomic storage directly (menu.cpp).
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
