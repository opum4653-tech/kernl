#pragma once
#include "common.hpp"
#include <sched.h>
#include <unistd.h>

namespace aimbot {

    // High-speed writer state — zero lock contention (atomic hand-off)
    inline std::atomic<bool>     s_writerRunning{false};
    inline std::atomic<bool>     s_hasTarget{false};
    inline std::atomic<uint64_t> s_targetAddr{0};
    inline std::atomic<float>    s_targetHeadX{0.f};
    inline std::atomic<float>    s_targetHeadY{0.f};
    inline std::atomic<float>    s_targetHeadZ{0.f};
    inline std::thread           s_writerThread;

    inline void clear_silent_data() {
        s_hasTarget.store(false, std::memory_order_relaxed);
        s_targetAddr.store(0, std::memory_order_relaxed);
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

        // Apply Headshot Elevation Bias (default 0.22m into skull center)
        float bias = cfg::aim::head_bias.load(std::memory_order_relaxed);
        if (bias < 0.05f || bias > 0.40f) bias = 0.22f;
        headPos.y += bias;

        s_targetHeadX.store(headPos.x, std::memory_order_relaxed);
        s_targetHeadY.store(headPos.y, std::memory_order_relaxed);
        s_targetHeadZ.store(headPos.z, std::memory_order_relaxed);
        s_targetAddr.store(target.addr, std::memory_order_relaxed);
        s_hasTarget.store(true, std::memory_order_release);
    }

    // High-Speed Writer Thread (Hybrid pewpew + STORM_MODS architecture):
    // 1. Silent Aim: Writes unnormalized displacement vector (targetPos - startPos) to AimInfo + 0x40.
    //    Fixes the 1-meter raycast bug caused by vector normalization.
    // 2. Auto Headshot: Swaps target's LockedAimingCollider (0x80) with HeadCollider (0x738)
    //    from STORM_MODS, guaranteeing 100% Red damage across all hits.
    // 3. Zero Lock Contention: Lock-free atomic synchronization eliminates thread starvation.
    inline void writer_loop() {
        while (s_writerRunning.load(std::memory_order_relaxed)) {
            if (!cfg::master_enabled.load(std::memory_order_relaxed) ||
                (!cfg::aim::silent.load(std::memory_order_relaxed) &&
                 !cfg::aim::auto_headshot.load(std::memory_order_relaxed) &&
                 !cfg::aim::scope_aim.load(std::memory_order_relaxed))) {
                if (s_hasTarget.load(std::memory_order_relaxed)) {
                    clear_silent_data();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                continue;
            }

            if (!data::inMatch.load(std::memory_order_relaxed)) {
                if (s_hasTarget.load(std::memory_order_relaxed)) {
                    clear_silent_data();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                continue;
            }

            uint64_t localPlayer = data::localPlayerAddr.load(std::memory_order_relaxed);
            if (!is_valid_ptr(localPlayer)) {
                if (s_hasTarget.load(std::memory_order_relaxed)) {
                    clear_silent_data();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // High-cadence firing check
            bool isFiring = rpm<bool>(localPlayer + Offsets::Player::IsFiring);
            if (!isFiring) {
                isFiring = rpm<bool>(localPlayer + Offsets::Player::IsFiring_Alt);
            }

            bool hasTarget = s_hasTarget.load(std::memory_order_acquire);
            uint64_t targetAddr = s_targetAddr.load(std::memory_order_relaxed);

            if (isFiring && hasTarget && is_valid_ptr(targetAddr)) {
                Vector3 targetHead(
                    s_targetHeadX.load(std::memory_order_relaxed),
                    s_targetHeadY.load(std::memory_order_relaxed),
                    s_targetHeadZ.load(std::memory_order_relaxed)
                );

                // 1. Silent Aim Steering (AimInfo + 0x40 RayDir)
                if (cfg::aim::silent.load(std::memory_order_relaxed) ||
                    cfg::aim::scope_aim.load(std::memory_order_relaxed) ||
                    cfg::aim::auto_headshot.load(std::memory_order_relaxed)) {

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

                        if (startPos != Vector3::Zero() && std::isfinite(startPos.x)) {
                            // Full unnormalized displacement vector (pewpew & STORM_MODS standard)
                            Vector3 dir = targetHead - startPos;
                            if (dir.sqrMagnitude() > 0.001f) {
                                wpm<Vector3>(aimInfo + Offsets::Aim::Direction, dir);
                                if (is_valid_ptr(aimInfoAlt)) {
                                    wpm<Vector3>(aimInfoAlt + Offsets::Aim::Direction, dir);
                                }
                            }
                        }
                    }
                }

                // 2. Auto Headshot (100% Red Hitbox Collider Swap from STORM_MODS)
                if (cfg::aim::auto_headshot.load(std::memory_order_relaxed)) {
                    uint64_t headCollider = rpm<uint64_t>(targetAddr + Offsets::Player::HeadCollider);
                    if (is_valid_ptr(headCollider)) {
                        wpm<uint64_t>(targetAddr + Offsets::Player::LockedAimingCollider, headCollider);
                    }
                }

                // High-performance yield while shooting
                sched_yield();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
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

        // Immediate first-frame raycast alignment if firing
        if (isFiring) {
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
                    Vector3 dir = targetHead - startPos;
                    if (dir.sqrMagnitude() > 0.001f) {
                        wpm<Vector3>(aimInfo + Offsets::Aim::Direction, dir);
                        if (is_valid_ptr(aimInfoAlt)) {
                            wpm<Vector3>(aimInfoAlt + Offsets::Aim::Direction, dir);
                        }
                    }
                }
            }

            if (cfg::aim::auto_headshot.load(std::memory_order_relaxed)) {
                uint64_t headCollider = rpm<uint64_t>(target.addr + Offsets::Player::HeadCollider);
                if (is_valid_ptr(headCollider)) {
                    wpm<uint64_t>(target.addr + Offsets::Player::LockedAimingCollider, headCollider);
                }
            }
        }
    }

} // namespace aimbot
