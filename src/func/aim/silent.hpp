#pragma once
#include "common.hpp"
#include "../../other/logger.hpp"
#include <sched.h>
#include <unistd.h>

// ==============================================================================
// SILENT AIM - 100% IDENTICAL TO PEWPEW ARCHITECTURE (OB55 NATIVE)
// Matches refer/pewpew/src/func/silent.hpp 1:1
// Floods ONLY hitObj + 0x40 (raycast / Direction) with target direction.
// NEVER writes to enemy memory (zero targetAddr writes, 100% anti-ban & zero ESP drop).
// ==============================================================================

namespace aimbot {

    inline std::atomic<bool>     s_writerRunning{false};
    inline std::atomic<bool>     s_hasTarget{false};
    inline std::atomic<bool>     s_hasData{false};
    inline std::atomic<uint64_t> s_hitObj{0};
    inline std::atomic<float>    s_dirX{0.f};
    inline std::atomic<float>    s_dirY{0.f};
    inline std::atomic<float>    s_dirZ{0.f};
    inline std::thread           s_writerThread;

    inline void clear_silent_data() {
        s_hasData.store(false, std::memory_order_relaxed);
        s_hasTarget.store(false, std::memory_order_relaxed);
        s_hitObj.store(0, std::memory_order_relaxed);
        s_dirX.store(0.f, std::memory_order_relaxed);
        s_dirY.store(0.f, std::memory_order_relaxed);
        s_dirZ.store(0.f, std::memory_order_relaxed);
    }

    // High-speed writer thread — identical to pewpew writer_loop()
    // Continuously floods hitObj + 0x40 (raycast) while shooting
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            if (!s_hasData.load(std::memory_order_relaxed) || s_hitObj.load(std::memory_order_relaxed) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint64_t hitObj = s_hitObj.load(std::memory_order_relaxed);
            if (is_valid_ptr(hitObj)) {
                Vector3 dir(
                    s_dirX.load(std::memory_order_relaxed),
                    s_dirY.load(std::memory_order_relaxed),
                    s_dirZ.load(std::memory_order_relaxed)
                );
                wpm<Vector3>(hitObj + Offsets::Aim::Direction, dir);
            }
            sched_yield();
        }
    }

    inline void start_silent_writer() {
        if (s_writerRunning.exchange(true)) return;
        if (s_writerThread.joinable()) {
            s_writerThread.join();
        }
        s_writerThread = std::thread(writer_loop);
    }

    inline void stop_silent_writer() {
        if (!s_writerRunning.exchange(false)) return;
        clear_silent_data();
        if (s_writerThread.joinable()) {
            s_writerThread.join();
        }
    }

    // 3D Silent bullet trajectory calculation and steering entry (pewpew 1:1)
    inline void apply_trajectory(uint64_t localPlayer, const PlayerData& target, bool lockToHead, bool isFiring) {
        (void)lockToHead;
        if (!is_valid_ptr(localPlayer) || !is_valid_ptr(target.addr)) {
            clear_silent_data();
            return;
        }

        if (!cfg::aim::silent.load(std::memory_order_relaxed) &&
            !cfg::aim::auto_headshot.load(std::memory_order_relaxed) &&
            !cfg::aim::scope_aim.load(std::memory_order_relaxed)) {
            clear_silent_data();
            return;
        }

        // 1. Resolve Weapon AimInfo (0xE58 in pewpew)
        uint64_t hitObjAddress = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo);
        if (!is_valid_ptr(hitObjAddress)) {
            hitObjAddress = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo_Alt);
        }
        if (!is_valid_ptr(hitObjAddress)) {
            clear_silent_data();
            return;
        }

        // 2. Resolve Target Position
        Vector3 targetWorld = target.headWorld;
        if (targetWorld == Vector3::Zero() || !std::isfinite(targetWorld.x)) {
            clear_silent_data();
            return;
        }

        // Head bias elevation
        float bias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (bias < -0.30f || bias > 0.30f) bias = 0.00f;
        targetWorld.y += (Offsets::Bone::SkullElevation + bias);

        // 3. Resolve Start Position / ammoBase (0x4C in pewpew)
        Vector3 ammoBase = rpm<Vector3>(hitObjAddress + Offsets::Aim::StartPosition);
        if (ammoBase == Vector3::Zero() || !std::isfinite(ammoBase.x)) {
            uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone_Head);
            if (localHead) ammoBase = GetNodePosition(localHead);
        }

        // 4. Direction Calculation & Memory Flooding (pewpew lines 148-158)
        if (ammoBase != Vector3::Zero() && std::isfinite(ammoBase.x)) {
            Vector3 dir;
            dir.x = targetWorld.x - ammoBase.x;
            dir.y = targetWorld.y - ammoBase.y;
            dir.z = targetWorld.z - ammoBase.z;

            if (dir.sqrMagnitude() > 0.001f) {
                s_dirX.store(dir.x, std::memory_order_relaxed);
                s_dirY.store(dir.y, std::memory_order_relaxed);
                s_dirZ.store(dir.z, std::memory_order_relaxed);
                s_hitObj.store(hitObjAddress, std::memory_order_relaxed);
                s_hasData.store(isFiring, std::memory_order_release);
                s_hasTarget.store(true, std::memory_order_release);

                if (isFiring) {
                    // Direct immediate write for zero latency on trigger press (pewpew raycast 0x40)
                    wpm<Vector3>(hitObjAddress + Offsets::Aim::Direction, dir);
                }
            } else {
                clear_silent_data();
            }
        } else {
            clear_silent_data();
        }
    }

} // namespace aimbot
