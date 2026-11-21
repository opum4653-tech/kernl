#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <chrono>
#include "player.hpp"
#include "math.hpp"

// ==============================================================================================
// FILE: src/game/data.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Public interface and state declarations for the game memory scraping pipeline.
//
// VISIBILITY & RPM OPTIMIZATION:
// ----------------------------------------------------------------------------------------------
// VISIBILITY RULE: ESP range == CAMERA FRUSTUM (no distance limit).
//   Only players whose head/feet project inside the screen bounds are processed.
//
// RPM OPTIMIZATIONS:
// 1. BATCH READ: Sequential fields read via single mem_read() call, not many rpm<T>()
// 2. TRANSFORM CACHE: GetNodePosition() results cached for 100ms (3 ticks @ 33ms)
// 3. INTERVAL THROTTLE: Default data loop runs at 33ms (30Hz) not 16ms (60Hz)
// 4. FRUSTUM CULLING: Off-screen/behind-camera players skipped before all detailed reads
// 5. READ ORDER RANDOMIZATION: Fisher-Yates shuffle prevents pattern detection
// 6. ESP-ONLY GATE: Skip combat reads (PhyXdata/IsKnocked) when aim is disabled
// 7. DEAD/TEAMMATE REJECT: Cheap flags checked before expensive bone walks
// ==============================================================================================

namespace data {

    // Transform position cache entry — avoids re-walking Unity bone hierarchy every tick
    struct BoneCache {
        Vector3 headPos;
        Vector3 feetPos;
        std::chrono::steady_clock::time_point cachedAt;
        bool valid;
    };

    // Executes a single complete memory walk cycle: reads BaseGame, Match, Camera, and PlayerDict
    void update();

    // Spawns the dedicated background thread that loops update() at configurable intervals
    void thread_start();

    // Safely joins the background thread and flushes active player memory lists
    void thread_stop();

    // Drops published match state without joining the worker. Used when the game
    // process exits so the render thread does not block on thread_stop().
    void note_detached();

    // Thread-safe repository of active parsed enemy entities
    extern std::vector<PlayerData> players;

    // Mutex synchronizing writes from background thread with reads from render/aim threads
    extern std::mutex              players_mutex;

    // Fast-path atomic cache of the local player entity pointer in Il2Cpp memory
    extern std::atomic<uint64_t>   localPlayerAddr;

    // Atomic boolean flag: true when inside an active battle match, false in lobby/menus
    extern std::atomic<bool>       inMatch;

    // Live camera ViewMatrix published each data tick (row-major Unity 4x4).
    // Consumers: silent.hpp cone guard — read under viewMatrix_mutex.
    extern std::mutex              viewMatrix_mutex;
    extern matrix                  viewMatrix_pub;

    // Monotonic tick counter bumped whenever viewMatrix_pub is refreshed.
    // Silent aim uses this to reject stale camera data during fast flicks.
    extern std::atomic<uint64_t>   viewMatrix_stamp;

    // Bone position cache: addr → cached head/feet positions (reduces Transform walks ~70%)
    // Entries expire after 100ms. Cleared on note_detached(), thread_stop() and
    // whenever the attached PID/lib base changes.
    extern std::unordered_map<uint64_t, BoneCache> bone_cache;

    // Guards bone_cache. The data worker reads/writes it while the render thread
    // may call note_detached() on game exit — without this lock that was a data race.
    extern std::mutex              bone_cache_mutex;
}
