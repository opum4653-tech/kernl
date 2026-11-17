#pragma once
#include "common.hpp"
#include <sched.h>
#include <unistd.h>

namespace aimbot {

    // High-speed writer state — zero lock contention (atomic hand-off)
    inline std::atomic<bool>     s_writerRunning{false};
    inline std::atomic<bool>     s_hasTarget{false};
    inline std::atomic<bool>     s_hasData{false};
    inline std::atomic<uint64_t> s_targetAddr{0};
    inline std::atomic<uint64_t> s_aimInfo{0};
    inline std::atomic<uint64_t> s_aimInfoAlt{0};
    inline std::atomic<float>    s_dirX{0.f};
    inline std::atomic<float>    s_dirY{0.f};
    inline std::atomic<float>    s_dirZ{0.f};
    inline std::atomic<float>    s_targetHeadX{0.f};
    inline std::atomic<float>    s_targetHeadY{0.f};
    inline std::atomic<float>    s_targetHeadZ{0.f};
    inline std::thread           s_writerThread;

    inline void clear_silent_data() {
        s_hasData.store(false, std::memory_order_relaxed);
        s_hasTarget.store(false, std::memory_order_relaxed);
        s_targetAddr.store(0, std::memory_order_relaxed);
        s_aimInfo.store(0, std::memory_order_relaxed);
        s_aimInfoAlt.store(0, std::memory_order_relaxed);
        s_dirX.store(0.f, std::memory_order_relaxed);
        s_dirY.store(0.f, std::memory_order_relaxed);
        s_dirZ.store(0.f, std::memory_order_relaxed);
    }

    inline void set_silent_target(const PlayerData& target) {
        if (!is_valid_ptr(target.addr)) {
            clear_silent_data();
            return;
        }

        Vector3 headPos = target.headWorld;
        if (headPos == Vector3::Zero() || !std::isfinite(headPos.x) || !std::isfinite(headPos.y) || !std::isfinite(headPos.z)) {
            clear_silent_data();
            return;
        }

        // Headshot vertical trim (0.00m default = exact center of OB55 Head Bone 0x6A0)
        float bias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (bias < -0.30f || bias > 0.30f) bias = 0.00f;
        headPos.y += bias;

        s_targetHeadX.store(headPos.x, std::memory_order_relaxed);
        s_targetHeadY.store(headPos.y, std::memory_order_relaxed);
        s_targetHeadZ.store(headPos.z, std::memory_order_relaxed);
        s_targetAddr.store(target.addr, std::memory_order_relaxed);
        s_hasTarget.store(true, std::memory_order_release);
    }

    // High-Speed Writer Thread (Hybrid pewpew + STORM_MODS architecture):
    // 1. Silent Aim: Writes unnormalized displacement vector (targetPos - startPos) to AimInfo + 0x40.
    // 2. Auto Headshot: Swaps target's LockedAimingCollider (0x80) with HeadCollider (0x740)
    //    guaranteeing 100% Red headshot registration whenever Aim Silent or Auto Headshot is active.
    // 3. Zero-RPM Hot Loop: Precalculated direction vector is flooded with zero kernel read overhead.
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            if (!s_hasData.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint64_t hitObj = s_aimInfo.load(std::memory_order_relaxed);
            if (!is_valid_ptr(hitObj)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            Vector3 dir(
                s_dirX.load(std::memory_order_relaxed),
                s_dirY.load(std::memory_order_relaxed),
                s_dirZ.load(std::memory_order_relaxed)
            );

            // Zero-latency continuous memory flooding (pewpew standard)
            if (dir.sqrMagnitude() > 0.001f) {
                wpm<Vector3>(hitObj + Offsets::Aim::Direction, dir);
                uint64_t hitObjAlt = s_aimInfoAlt.load(std::memory_order_relaxed);
                if (is_valid_ptr(hitObjAlt)) {
                    wpm<Vector3>(hitObjAlt + Offsets::Aim::Direction, dir);
                }
            }

            sched_yield();
        }
    }

    inline void start_silent_writer() {
        if (s_writerRunning.exchange(true)) return;
        s_writerThread = std::thread(writer_loop);
    }

    inline void stop_silent_writer() {
        if (!s_writerRunning.exchange(false)) return;
        clear_silent_data();
        if (s_writerThread.joinable()) {
            s_writerThread.join();
        }
    }

    // 3D Silent bullet trajectory calculation and steering entry
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

        set_silent_target(target);

        // Resolve Weapon AimingInfo (0xE58 primary, 0xE50 auxiliary)
        uint64_t aimInfo = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo);
        uint64_t aimInfoAlt = rpm<uint64_t>(localPlayer + Offsets::Player::AimInfo_Alt);
        if (!is_valid_ptr(aimInfo)) {
            aimInfo = aimInfoAlt;
            aimInfoAlt = 0;
        }

        if (is_valid_ptr(aimInfo)) {
            Vector3 startPos = rpm<Vector3>(aimInfo + Offsets::Aim::StartPosition);
            if (startPos == Vector3::Zero() || !std::isfinite(startPos.x)) {
                uint64_t localHead = rpm<uint64_t>(localPlayer + Offsets::Bone_Head);
                if (localHead) startPos = GetNodePosition(localHead);
            }

            Vector3 targetHead(
                s_targetHeadX.load(std::memory_order_relaxed),
                s_targetHeadY.load(std::memory_order_relaxed),
                s_targetHeadZ.load(std::memory_order_relaxed)
            );

            if (startPos != Vector3::Zero() && std::isfinite(startPos.x)) {
                // Raw unnormalized displacement vector (pewpew line 149 & STORM_MODS line 460)
                Vector3 dir = targetHead - startPos;
                if (dir.sqrMagnitude() > 0.001f) {
                    s_aimInfo.store(aimInfo, std::memory_order_relaxed);
                    s_aimInfoAlt.store(aimInfoAlt, std::memory_order_relaxed);
                    s_dirX.store(dir.x, std::memory_order_relaxed);
                    s_dirY.store(dir.y, std::memory_order_relaxed);
                    s_dirZ.store(dir.z, std::memory_order_relaxed);
                    s_hasData.store(isFiring, std::memory_order_release);

                    // Immediate write on active firing/trigger for instant alignment
                    if (isFiring) {
                        wpm<Vector3>(aimInfo + Offsets::Aim::Direction, dir);
                        if (is_valid_ptr(aimInfoAlt)) {
                            wpm<Vector3>(aimInfoAlt + Offsets::Aim::Direction, dir);
                        }

                        // Automatic Head Collider Swap & Hit Confirmation while firing
                        uint64_t headCollider = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider);
                        if (!is_valid_ptr(headCollider)) {
                            headCollider = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider_Alt);
                        }
                        if (is_valid_ptr(headCollider)) {
                            wpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider, headCollider);
                            Vector3 targetHeadPos(
                                s_targetHeadX.load(std::memory_order_relaxed),
                                s_targetHeadY.load(std::memory_order_relaxed),
                                s_targetHeadZ.load(std::memory_order_relaxed)
                            );
                            wpm<uint64_t>(aimInfo + Offsets::Aim::HitCollider, headCollider);
                            wpm<Vector3>(aimInfo + Offsets::Aim::HitPoint, targetHeadPos);
                            if (is_valid_ptr(aimInfoAlt)) {
                                wpm<uint64_t>(aimInfoAlt + Offsets::Aim::HitCollider, headCollider);
                                wpm<Vector3>(aimInfoAlt + Offsets::Aim::HitPoint, targetHeadPos);
                            }
                        }

                        // Weapon Engine Aiming State Binding (Dual Client/Server Verification Bypass)
                        wpm<uint64_t>(localPlayer + Offsets::Player_AimingTarget, target.addr);
                        wpm<bool>(localPlayer + Offsets::Player_AimingAdjusted, true);
                    }
                } else {
                    s_hasData.store(false, std::memory_order_relaxed);
                }
            } else {
                s_hasData.store(false, std::memory_order_relaxed);
            }
        } else {
            s_hasData.store(false, std::memory_order_relaxed);
        }
    }

} // namespace aimbot
