#pragma once
#include "common.hpp"
#include "target.hpp"
#include "headshot.hpp"
#include "silent.hpp"
#include "fov.hpp"

// TRAJECTORY ASSIST ENGINE (NATURAL GAMEPLAY — ZERO CAMERA JUMPING)
// Features:
// 1. Natural Gameplay: Camera rotation is 100% untouched. Finger aiming is free.
// 2. Strict Target Stickiness: ZERO screen shaking when multiple enemies enter FOV.
// 3. Grace-Period Burst Lock: Prevents lock resets between rapid bullet intervals.
// 4. Trajectory Write: only the client bullet direction is steered to head sphere.
// 5. Strict FOV Circle: Strictly respects FOV limit; never locks outside.

namespace aimbot {

    inline std::atomic<bool>& aim_thread_running() {
        static std::atomic<bool> s_run{false};
        return s_run;
    }

    inline void update() {
        if (!cfg::master_enabled) {
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        if (!cfg::aim::silent && !cfg::aim::auto_headshot && !cfg::aim::scope_aim) {
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            s_smoothDir = Vector3::Zero();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        if (!data::inMatch.load(std::memory_order_relaxed)) {
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            restore_headshot_override();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        uint64_t localPlayer = data::localPlayerAddr.load(std::memory_order_relaxed);
        if (!is_valid_ptr(localPlayer)) {
            s_lockedTargetAddr = 0;
            s_lockTimeoutTicks = 0;
            restore_headshot_override();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        TargetAcquisitionResult acq = acquire_target(localPlayer);
        if (!acq.valid) {
            restore_headshot_override();
            publish_target_invalid();
            clear_silent_data();
            return;
        }

        // Target telemetry for FOV indicator
        TargetInfo info;
        info.valid = true;
        snprintf(info.name, sizeof(info.name), "%s",
                 acq.target.name[0] ? acq.target.name : (acq.target.isBot ? "BOT" : "Player"));
        info.distance = acq.target.distance;
        info.hp = static_cast<int>(acq.target.curHP);
        info.screenPos = acq.bestScreenPos;
        info.headWorld = acq.target.headWorld;
        publish_target(info);

        // Headshot evaluation
        bool lockToHead = evaluate_headshot(acq.target, acq.isScoping);

        // 3D Silent bullet trajectory steering
        if (cfg::aim::silent || cfg::aim::auto_headshot || (cfg::aim::scope_aim && acq.isScoping)) {
            apply_trajectory(localPlayer, acq.target, lockToHead, (acq.isFiring || acq.triggerActive));
        } else {
            clear_silent_data();
        }
    }

    inline void loop() {
        while (aim_thread_running().load(std::memory_order_relaxed)) {
            if (cfg::master_enabled && (cfg::aim::silent || cfg::aim::auto_headshot || cfg::aim::scope_aim)) {
                update();
            } else {
                publish_target_invalid();
                clear_silent_data();
            }

            uint64_t lp = data::localPlayerAddr.load(std::memory_order_relaxed);
            bool isFiring = false;
            if (lp) {
                int fireVal = rpm<int>(lp + Offsets::Player::IsFiring);
                if (fireVal <= 0) {
                    fireVal = rpm<int>(lp + Offsets::Player::IsFiring_Alt);
                }
                isFiring = (fireVal > 0);
            }

            if (isFiring) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    inline std::thread& get_aim_thread() {
        static std::thread s_aim_thread;
        return s_aim_thread;
    }

    inline void thread_start() {
        if (aim_thread_running().exchange(true)) return;
        start_silent_writer();
        auto& t = get_aim_thread();
        if (t.joinable()) {
            t.join();
        }
        t = std::thread(loop);
    }

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
