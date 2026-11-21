#pragma once
#include "common.hpp"
#include "target.hpp"
#include "headshot.hpp"
#include "silent.hpp"
#include "fov.hpp"
#include <chrono>
#include <cstdlib>

// ==============================================================================================
// FILE: src/func/aim/aim.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Master coordinator for the Combat, Silent Aim, and Trajectory Steering subsystem.
//
// CROSS-FILE CALL GRAPH & WORKFLOW:
// ----------------------------------------------------------------------------------------------
// 1. LIFECYCLE MANAGEMENT:
//    - src/main.cpp -> start_overlay():
//      * Calls trajectory::thread_start() the first time a libil2cpp.so base is resolved.
//      * Does not call trajectory::thread_stop() on game exit. That join runs on overlay shutdown.
//
// 2. INTERNAL DISPATCH PIPELINE (trajectory::update()):
//    Step 1: Check master enable & aim toggles (cfg::master_enabled, cfg::aim::silent, etc.).
//    Step 2: Check match state (data::inMatch) & local player validity (data::localPlayerAddr).
//    Step 3: Call acquire_target() (in target.hpp) to scan enemies with target hysteresis.
//    Step 4: Publish locked screen position via publish_target() for FOV visualizer (in fov.hpp).
//    Step 5: Call evaluate_headshot_rate() (headshot.hpp) for distance-aware headshot %.
//    Step 6: Call apply_trajectory() (silent.hpp) to steer bullet raycast to head or chest.
//
// 3. THREADING & ADAPTIVE POLLING:
//    - Runs on a dedicated high-priority thread (get_aim_thread()).
//    - Polls at cfg::aim::read_interval_ms, clamped to 8..33 ms (default 16).
//    - Direction is written once per estimated bullet while isFiring is held,
//      so every round of a full-auto burst is steered (rising-edge-only steering
//      left all shots after the first one untouched) without multiplying WPM.
// ==============================================================================================

namespace trajectory {

    // Thread running state atomic flag
    inline std::atomic<bool>& aim_thread_running() {
        static std::atomic<bool> s_run{false};
        return s_run;
    }

    // Core trajectory evaluation routine executed on each tick of the trajectory thread
    inline void update() {
        static bool prev_firing = false;

        // Master switch guard
        if (!cfg::master_enabled) {
            prev_firing = false;
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Verify at least one aim combat feature is toggled on
        if (!cfg::aim::silent && !cfg::aim::auto_headshot && !cfg::aim::scope_aim) {
            prev_firing = false;
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            s_smoothDir = Vector3::Zero();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Verify active gameplay match state
        if (!data::inMatch.load(std::memory_order_relaxed)) {
            prev_firing = false;
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            restore_headshot_override();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Verify local player entity validity
        uint64_t localPlayer = data::localPlayerAddr.load(std::memory_order_relaxed);
        if (!is_valid_ptr(localPlayer)) {
            prev_firing = false;
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            restore_headshot_override();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Execute target acquisition and FOV hysteresis check
        TargetAcquisitionResult acq = acquire_target(localPlayer);
        if (!acq.valid) {
            prev_firing = false;
            restore_headshot_override();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Publish target screen telemetry for the FOV lock marker ring (drawn in fov.hpp)
        TargetInfo info;
        info.valid = true;
        info.screenPos = acq.bestScreenPos;
        publish_target(info);

        // Headshot rate-controller evaluation (distance-aware, slider-driven).
        // No more hard 100% force — Auto Headshot now enables this rate controller.
        float headRate = evaluate_headshot_rate(acq.target, acq.isScoping);

        // ── FULL-AUTO STEERING (rising-edge bug fix) ──────────────────────────────
        // IsFiring is a HELD flag, not a one-shot pulse. Steering only on its
        // rising edge meant only the FIRST bullet of a trigger pull was steered
        // and every follow-up round of a full-auto burst fired naturally.
        //
        // The weapon re-samples Direction for each round, so we write once per
        // estimated bullet (~85ms ≈ 11-12 rounds/s) instead of once per trigger
        // pull. Time-based (not tick-based) so the write cadence stays correct
        // whether the aim loop runs at 8ms or 33ms — and it does NOT multiply
        // the WPM footprint by writing on every tick.
        static thread_local unsigned int s_skip_seed = 0x534B4950; // "SKIP"
        static thread_local std::chrono::steady_clock::time_point s_lastShotWrite{};

        bool shouldSteer = false;
        if (acq.isFiring) {
            auto now = std::chrono::steady_clock::now();
            long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - s_lastShotWrite).count();
            // ~12% of bullets are deliberately left natural (human miss pattern).
            if (!prev_firing || elapsed >= 85) {
                s_lastShotWrite = now;
                shouldSteer = ((rand_r(&s_skip_seed) % 100) >= 12);
            }
        } else {
            s_lastShotWrite = {};
        }
        prev_firing = acq.isFiring;

        if (cfg::aim::silent || cfg::aim::auto_headshot || (cfg::aim::scope_aim && acq.isScoping)) {
            apply_trajectory(localPlayer, acq.target, headRate, shouldSteer);
        } else {
            clear_silent_data();
        }
    }

    // Steady natural trajectory coordinator loop
    inline void loop() {
        while (aim_thread_running().load(std::memory_order_relaxed)) {
            bool combatActive = cfg::master_enabled && (cfg::aim::silent || cfg::aim::auto_headshot || cfg::aim::scope_aim);
            proc::wpm_allowed.store(combatActive, std::memory_order_release);
            if (combatActive) {
                update();
            } else {
                publish_target_invalid();
                clear_silent_data();
            }

            int interval = cfg::aim::read_interval_ms.load(std::memory_order_relaxed);
            int baseMs = (interval >= 8 && interval <= 33) ? interval : 16;

            // ── Cadence variation ─────────────────────────────────────────────────
            // Add ±3ms of thread-local noise so the poll/write timing is never
            // perfectly periodic. Human input and frame pacing are irregular;
            // a fixed 16.000ms loop is a machine signature.
            static thread_local unsigned int s_cadence_seed = 0x43414445; // "CADE"
            int jitterMs = (int)(rand_r(&s_cadence_seed) % 7) - 3; // -3..+3
            int sleepMs = baseMs + jitterMs;
            if (sleepMs < 6) sleepMs = 6;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    }

    // Singleton thread handle for the trajectory coordinator thread
    inline std::thread& get_aim_thread() {
        static std::thread s_aim_thread;
        return s_aim_thread;
    }

    // Starts the trajectory coordinator thread and the trajectory writer thread
    inline void thread_start() {
        if (aim_thread_running().exchange(true)) return;
        start_silent_writer();
        auto& t = get_aim_thread();
        if (t.joinable()) {
            t.join();
        }
        t = std::thread(loop);
    }

    // Stops the trajectory coordinator thread, flushes trajectory writer, and clears targets
    inline void thread_stop() {
        proc::wpm_allowed.store(false, std::memory_order_release);
        if (!aim_thread_running().exchange(false)) return;
        auto& t = get_aim_thread();
        if (t.joinable()) {
            t.join();
        }
        stop_silent_writer();
        restore_headshot_override();
        publish_target_invalid();
    }

} // namespace trajectory
