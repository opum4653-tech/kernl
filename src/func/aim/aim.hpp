#pragma once
#include "common.hpp"
#include "target.hpp"
#include "headshot.hpp"
#include "silent.hpp"
#include "fov.hpp"

// ==============================================================================================
// FILE: src/func/aim/aim.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Master coordinator for the Combat, Silent Aim, and Trajectory Steering subsystem.
//
// CROSS-FILE CALL GRAPH & WORKFLOW:
// ----------------------------------------------------------------------------------------------
// 1. LIFECYCLE MANAGEMENT:
//    - src/main.cpp -> start_overlay():
//      * Calls aimbot::thread_start() upon game attachment.
//      * Calls aimbot::thread_stop() upon game exit or overlay termination.
//
// 2. INTERNAL DISPATCH PIPELINE (aimbot::update()):
//    Step 1: Check master enable & aim toggles (cfg::master_enabled, cfg::aim::silent, etc.).
//    Step 2: Check match state (data::inMatch) & local player validity (data::localPlayerAddr).
//    Step 3: Call acquire_target() (in target.hpp) to scan enemies with target hysteresis.
//    Step 4: Publish locked screen position via publish_target() for FOV visualizer (in fov.hpp).
//    Step 5: Call evaluate_headshot() (in headshot.hpp) to verify headshot authorization.
//    Step 6: Call apply_trajectory() (in silent.hpp) to steer bullet raycast directly to enemy head.
//
// 3. THREADING & ADAPTIVE POLLING:
//    - Runs on a dedicated high-priority thread (get_aim_thread()).
//    - While firing: polls at 2ms (500Hz) for zero-latency instant bullet redirection.
//    - While idle: throttles politely to 10ms (100Hz) to conserve CPU cycles and battery.
// ==============================================================================================

namespace aimbot {

    // Thread running state atomic flag
    inline std::atomic<bool>& aim_thread_running() {
        static std::atomic<bool> s_run{false};
        return s_run;
    }

    // Core aimbot evaluation routine executed on each tick of the aim thread
    inline void update() {
        // Master switch guard
        if (!cfg::master_enabled) {
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Verify at least one aim combat feature is toggled on
        if (!cfg::aim::silent && !cfg::aim::auto_headshot && !cfg::aim::scope_aim) {
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            s_smoothDir = Vector3::Zero();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Verify active gameplay match state
        if (!data::inMatch.load(std::memory_order_relaxed)) {
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

        // Headshot authorization evaluation
        bool lockToHead = evaluate_headshot(acq.target, acq.isScoping);

        // 3D Silent bullet trajectory steering execution
        if (cfg::aim::silent || cfg::aim::auto_headshot || (cfg::aim::scope_aim && acq.isScoping)) {
            apply_trajectory(localPlayer, acq.target, lockToHead, (acq.isFiring || acq.triggerActive));
        } else {
            clear_silent_data();
        }
    }

    // Continuous aimbot worker loop with adaptive 2ms/10ms rate throttling
    inline void loop() {
        while (aim_thread_running().load(std::memory_order_relaxed)) {
            if (cfg::master_enabled && (cfg::aim::silent || cfg::aim::auto_headshot || cfg::aim::scope_aim)) {
                update();
            } else {
                publish_target_invalid();
                clear_silent_data();
            }

            // Inspect local firing status to dynamically throttle loop sleep time
            uint64_t lp = data::localPlayerAddr.load(std::memory_order_relaxed);
            bool isFiring = false;
            if (lp) {
                int fireVal = rpm<int>(lp + Offsets::Player::IsFiring);
                if (fireVal <= 0) {
                    fireVal = rpm<int>(lp + Offsets::Player::IsFiring_Alt);
                }
                isFiring = (fireVal > 0);
            }

            // High-frequency polling (2ms) during firing bursts; relaxed polling (10ms) when idle
            if (isFiring) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    // Singleton thread handle for the aimbot coordinator thread
    inline std::thread& get_aim_thread() {
        static std::thread s_aim_thread;
        return s_aim_thread;
    }

    // Starts the aimbot coordinator thread and the trajectory writer thread
    inline void thread_start() {
        if (aim_thread_running().exchange(true)) return;
        start_silent_writer();
        auto& t = get_aim_thread();
        if (t.joinable()) {
            t.join();
        }
        t = std::thread(loop);
    }

    // Stops the aimbot coordinator thread, flushes trajectory writer, and clears targets
    inline void thread_stop() {
        if (!aim_thread_running().exchange(false)) return;
        auto& t = get_aim_thread();
        if (t.joinable()) {
            t.join();
        }
        stop_silent_writer();
        restore_headshot_override();
        publish_target_invalid();
    }

} // namespace aimbot
