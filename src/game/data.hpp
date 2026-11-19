#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include "player.hpp"
#include "math.hpp"

// ==============================================================================================
// FILE: src/game/data.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Public interface and state declarations for the game memory scraping pipeline.
//
// CROSS-FILE CALL GRAPH & CONSUMERS:
// ----------------------------------------------------------------------------------------------
// 1. LIFECYCLE MANAGEMENT:
//    - src/main.cpp:
//      * Calls data::thread_start() upon detecting valid game PID and libil2cpp.so base address.
//      * Calls data::thread_stop() when the target game terminates or overlay shuts down.
//
// 2. DATA SYNCHRONIZATION & THREAD ACCESS:
//    - src/game/data.cpp:
//      * Implements data::update() on a dedicated background thread (data::g_thread).
//      * Continuously extracts entities, runs WorldToScreen, and publishes into data::players.
//    - src/func/esp/esp.hpp:
//      * Acquires data::players_mutex to copy data::players snapshot for rendering.
//    - src/func/aim/target.hpp:
//      * Acquires data::players_mutex to scan enemies for optimal trajectory target locking.
//    - src/func/speed.hpp:
//      * Reads data::localPlayerAddr to directly apply velocity modifier & recoil compensation without extra RPM.
//    - src/func/aim/aim.hpp:
//      * Reads data::inMatch and data::localPlayerAddr to govern trajectory calculations.
// ==============================================================================================

namespace data {
    // Executes a single complete memory walk cycle: reads BaseGame, Match, Camera, and PlayerDict
    void update();

    // Spawns the dedicated background thread that loops update() at configurable intervals
    void thread_start();

    // Safely joins the background thread and flushes active player memory lists
    void thread_stop();

    // Thread-safe repository of active parsed enemy entities
    extern std::vector<PlayerData> players;

    // Mutex synchronizing writes from background thread with reads from render/aim threads
    extern std::mutex              players_mutex;

    // Fast-path atomic cache of the local player entity pointer in Il2Cpp memory
    extern std::atomic<uint64_t>   localPlayerAddr;

    // Atomic boolean flag: true when inside an active battle match, false in lobby/menus
    extern std::atomic<bool>       inMatch;
}
