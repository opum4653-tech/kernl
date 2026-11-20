#include "data.hpp"
#include "../offsets/offsets.hpp"
#include "../other/utils.hpp"
#include "../other/memory.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

// ==============================================================================================
// FILE: src/game/data.cpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Core memory extraction pipeline. Traverses Il2Cpp object graphs, parses enemy entities,
//          extracts 3D bone nodes, executes WorldToScreen projection, and publishes to rendering.
//
// CALL GRAPH & CROSS-FILE DATA FLOW:
// ----------------------------------------------------------------------------------------------
// 1. INPUT SOURCES (Target Process Memory):
//    - Process base: proc::lib (libil2cpp.so base address resolved in main.cpp / memory.hpp)
//    - Kernel RPM: mem_read(), rpm<T>() routed through driver->read() in memory.hpp.
//    - Offsets: src/offsets/offsets.hpp (GameFacade, Match, Players, Bones, HP, UMA).
//
// 2. EXTRACTION SEQUENCE:
//    Step 1: GameFacade Dual-Path Resolution (libil2cpp.so + 0xA72E4E0 -> P2 0xB8 -> BaseGame).
//    Step 2: Match & LocalPlayer extraction (BaseGame + 0x90 -> Match -> LocalPlayer 0xD8).
//    Step 3: Camera & ViewMatrix extraction (LocalPlayer + 0x690 -> FollowCamera -> Native IntPtr -> ViewMatrix).
//    Step 4: Player Dictionary Traversal (Match + 0xC0 -> MonoDictionary 64-slot batch reader in utils.hpp).
//    Step 5: Single-Syscall PlayerSpan read (0x70 to 0x788 = HP, Bones, Avatar, Bot flag).
//    Step 6: Teammate & Bot filtration via UMAData flags (0x81 teammate, 0x80 local player).
//    Step 7: 3D Bone extraction (GetNodePosition) & 2D Projection (WorldToScreenPoint in math.hpp).
//
// 3. PUBLISHING & SYNCHRONIZATION:
//    - Atomically swaps parsed list into data::players under data::players_mutex.
//    - Consumed every frame by func::esp::draw() in src/func/esp/esp.hpp.
//    - Consumed by trajectory::acquire_target() in src/func/aim/target.hpp.
// ==============================================================================================

namespace data {

    // Thread-safe published vector of active enemy entities
    std::vector<PlayerData>  players;
    std::mutex               players_mutex;

    // Fast-path atomic pointer to local player entity
    std::atomic<uint64_t>    localPlayerAddr{0};

    // Match status flag (true = player is inside an active game round)
    std::atomic<bool>        inMatch{false};

    // Camera matrix lives entirely inside this translation unit: it is written
    // by update() and consumed by boneScreen, all on the data thread in program order.
    static matrix            viewMatrix{};

    // Lifecycle state and thread handle for the continuous background scraper loop
    static std::atomic<bool> g_run{false};
    static std::thread       g_thread;

    // Single-Syscall PlayerSpan Optimization:
    // Every field read directly off the player object sits between 0x70 and 0x870
    // (Player::HP .. Player::IsFiring). Pulling that span in a single process_vm_readv / IOCTL DMA
    // replaces roughly 10 individual syscalls per player with 1, eliminating CPU context switches.
    static constexpr size_t kSpanLo = Offsets::Player::HP;
    static constexpr size_t kSpanHi = Offsets::Player::IsFiring + 0x20; // 0x850 + 0x20 = 0x870 span coverage

    struct PlayerSpan {
        uint8_t buf[kSpanHi - kSpanLo];
        bool    ok;

        explicit PlayerSpan(uint64_t player)
            : ok(mem_read(player + kSpanLo, buf, sizeof(buf))) {}

        template <typename T>
        T at(size_t off) const {
            T v{};
            if (!ok || off < kSpanLo) return v;
            const size_t rel = off - kSpanLo;
            if (rel + sizeof(T) > sizeof(buf)) return v;
            memcpy(&v, buf + rel, sizeof(T));
            return v;
        }
    };

    // HEALTH & ARMOR EXTRACTION:
    // pool is m_PRIDataPool (0x70) on player.
    // pool + 0x10 is m_Datas (ReplicationData[] array). Elements start at +0x20.
    // Index 0 (+0x20): CurHP (ReplicationData*)
    // Index 1 (+0x28): MaxHP (ReplicationData*)
    // Index 4 (+0x40): CurAP (ReplicationData*, Armor Points)
    // Value is stored at +0x18 (Offsets::HP_Value) inside each ReplicationData object.
    static void readHealthAndArmor(uint64_t pool, int& curHP, int& maxHP, int& curArmor) {
        curHP = 0;
        maxHP = 0;
        curArmor = 0;
        if (!pool) return;
        auto datas = rpm<uint64_t>(pool + Offsets::HP_List);
        if (!datas) return;

        // 1. Current Health (Index 0 at Offsets::HP_Entry = 0x20)
        auto elem0 = rpm<uint64_t>(datas + Offsets::HP_Entry);
        if (elem0) {
            curHP = rpm<int>(elem0 + Offsets::HP_Value);
        }

        // 2. Maximum Health (Index 1 at Offsets::Player::HP_MaxEntry = 0x28)
        auto elem1 = rpm<uint64_t>(datas + Offsets::Player::HP_MaxEntry);
        if (elem1 && elem1 > Offsets::MinValidAddress) {
            int m = rpm<int>(elem1 + Offsets::HP_Value);
            if (m > 0 && m <= 1000) maxHP = m;
        }
        if (maxHP <= 0 && curHP > 0) maxHP = curHP;
        if (maxHP < curHP) maxHP = curHP;

        // 3. Live Armor Points (Index 4 at Offsets::Player::HP_ArmorEntry = 0x40)
        auto elem4 = rpm<uint64_t>(datas + Offsets::Player::HP_ArmorEntry);
        if (elem4 && elem4 > Offsets::MinValidAddress) {
            curArmor = rpm<int>(elem4 + Offsets::HP_Value);
        }
    }

    // Projects a 3D bone node to 2D screen coordinates using the current frame's ViewMatrix
    static Vector3 boneScreen(uint64_t node) {
        if (!node) return Vector3::Zero();
        Vector3 w = GetNodePosition(node);
        if (w == Vector3::Zero()) return Vector3::Zero();
        return WorldToScreenPoint(viewMatrix, w);
    }

    // Tests column norms and checks for NaN / Inf to guarantee that the camera
    // matrix is fully initialized and in an active 3D rendering projection state.
    static inline bool is_valid_matrix(const matrix& m) {
        if (std::isnan(m.m11) || std::isnan(m.m22) || std::isnan(m.m33) || std::isnan(m.m44)) return false;
        if (std::isinf(m.m11) || std::isinf(m.m22) || std::isinf(m.m33) || std::isinf(m.m44)) return false;
        float col1 = fabsf(m.m11) + fabsf(m.m21) + fabsf(m.m31);
        float col2 = fabsf(m.m12) + fabsf(m.m22) + fabsf(m.m32);
        float col4 = fabsf(m.m14) + fabsf(m.m24) + fabsf(m.m34) + fabsf(m.m44);
        return (col1 > 0.05f && col2 > 0.05f && col4 > 0.05f);
    }

    // Master memory walk cycle: executed continuously on the dedicated background data thread
    void update() {
        // Atomic snapshot of live PID and module base to prevent race conditions during game exit
        const auto snap = proc::snapshot();
        if (snap.pid <= 0 || snap.lib == 0) return;

        std::vector<PlayerData> tmp;
        tmp.reserve(64);

        // 1. Resolve GameFacade Cell from libil2cpp.so base (Multi-Candidate: Max + Global)
        const uintptr_t gfCandidates[] = {
            0xAD68E18, // Free Fire Max (maxff.h primary)
            0xAD6DC18, // Free Fire Max (STORM_MODS alt)
            0xA72E4E0  // Free Fire Global (1.132.x primary)
        };

        uint64_t BaseGame = 0;
        uint64_t StaticFields = 0;

        for (uintptr_t gfOff : gfCandidates) {
            auto GfCell = rpm<uint64_t>(snap.lib + gfOff);
            if (!GfCell || GfCell < 0x10000) continue;

            // Primary Path: GfCell is a pointer to GameFacade Il2CppClass
            uint64_t GfClass = rpm<uint64_t>(GfCell);
            if (is_valid_ptr(GfClass)) {
                uint64_t sf = rpm<uint64_t>(GfClass + Offsets::GameFacade_P2);
                if (is_valid_ptr(sf)) {
                    uint64_t bg = rpm<uint64_t>(sf + Offsets::StaticFields_Instance); // +0x8
                    if (is_valid_ptr(bg)) { BaseGame = bg; StaticFields = sf; break; }
                    uint64_t bg0 = rpm<uint64_t>(sf + 0x0);
                    if (is_valid_ptr(bg0)) { BaseGame = bg0; StaticFields = sf; break; }
                }
            }

            // Secondary Fallback: GfCell is directly GameFacade Il2CppClass
            uint64_t sf1 = rpm<uint64_t>(GfCell + Offsets::GameFacade_P2);
            if (is_valid_ptr(sf1)) {
                uint64_t curGame1 = rpm<uint64_t>(sf1 + Offsets::StaticFields_Instance); // +0x8
                uint64_t curMatch1 = rpm<uint64_t>(sf1 + 0x0);
                if (is_valid_ptr(curGame1)) { BaseGame = curGame1; StaticFields = sf1; break; }
                else if (is_valid_ptr(curMatch1)) { BaseGame = curMatch1; StaticFields = sf1; break; }
            }
        }

        if (!is_valid_ptr(BaseGame)) return;

        // 2. Resolve Match and Local Player entities
        auto m_Match = rpm<uint64_t>(BaseGame + Offsets::BaseGame_Match);
        auto localPlayer = m_Match ? rpm<uint64_t>(m_Match + Offsets::Match_LocalPlayer) : 0;

        // Store local player address immediately for trajectory and speed mod fast-path lookup
        localPlayerAddr.store(localPlayer ? localPlayer : 0);

        bool matchActive = (localPlayer != 0);
        if (!matchActive && is_valid_ptr(m_Match)) {
            matchActive = (rpm<int>(m_Match + Offsets::MatchIsRunning) == 1) || rpm<bool>(BaseGame + Offsets::MatchIsRunning);
        }
        inMatch.store(matchActive);

        // If match is not currently active, flush lists and sleep gracefully
        if (!matchActive) {
            std::lock_guard<std::mutex> lk(players_mutex);
            if (!players.empty()) players.clear();
            return;
        }

        // Feature-muted mode: if user disabled all features in menu, skip heavy camera/bone calculations
        if (!cfg::any_feature_enabled()) {
            std::lock_guard<std::mutex> lk(players_mutex);
            if (!players.empty()) players.clear();
            return;
        }

        if (!m_Match) {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        if (!localPlayer) {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        // 3. Resolve Camera ViewMatrix chain (Multi-Candidate with matrix validation)
        uint64_t Follow = 0, Cam = 0, IntPtr = 0;
        bool matrix_resolved = false;

        auto try_matrix = [&](uint64_t intPtrCandidate) -> bool {
            if (!intPtrCandidate || !is_valid_ptr(intPtrCandidate)) return false;
            matrix vm = rpm<matrix>(intPtrCandidate + Offsets::Camera_Matrix);
            if (is_valid_matrix(vm)) {
                viewMatrix = vm;
                matrix_resolved = true;
                return true;
            }
            return false;
        };

        // Try primary FollowCamera pointer (0x698 on Max, 0x690 on Global)
        const uintptr_t camPtrOffsets[] = { 0x698, 0x690 };
        for (uintptr_t camOff : camPtrOffsets) {
            Follow = rpm<uint64_t>(localPlayer + camOff);
            if (!Follow || !is_valid_ptr(Follow)) continue;

            // Probe Camera displacements: 0x20 (maxff), 0x30 (STORM_MODS), 0x28 (Global)
            const uintptr_t followOffsets[] = { 0x20, 0x30, 0x28 };
            for (uintptr_t fOff : followOffsets) {
                Cam = rpm<uint64_t>(Follow + fOff);
                if (Cam && is_valid_ptr(Cam)) {
                    IntPtr = rpm<uint64_t>(Cam + Offsets::Camera_IntPtr);
                    if (try_matrix(IntPtr)) break;
                }
            }
            if (matrix_resolved) break;
        }

        // Candidate 4: BaseGame + 0xD8 -> 0x20 / 0x28 -> 0x10 (CameraBase fallback)
        if (!matrix_resolved && BaseGame) {
            uint64_t camMgr = rpm<uint64_t>(BaseGame + 0xD8);
            if (camMgr && is_valid_ptr(camMgr)) {
                uint64_t c20 = rpm<uint64_t>(camMgr + 0x20);
                if (c20) try_matrix(rpm<uint64_t>(c20 + 0x10));
                if (!matrix_resolved) {
                    uint64_t c28 = rpm<uint64_t>(camMgr + 0x28);
                    if (c28) try_matrix(rpm<uint64_t>(c28 + 0x10));
                }
            }
        }

        if (!matrix_resolved && !is_valid_matrix(viewMatrix)) {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        // Read 4x4 ViewMatrix committed
        (void)viewMatrix;

        // 4. Resolve Local Player Head Bone Position (Primary 0x6A0)
        Vector3 localPos = Vector3::Zero();
        uint64_t headNode = rpm<uint64_t>(localPlayer + Offsets::Bone_Head);
        if (headNode) localPos = GetNodePosition(headNode);
        if (Offsets::Bone_Head_Alt && (localPos == Vector3::Zero() || localPos.sqrMagnitude() < 1.0f)) {
            uint64_t headNodeAlt = rpm<uint64_t>(localPlayer + Offsets::Bone_Head_Alt);
            if (headNodeAlt) {
                Vector3 altPos = GetNodePosition(headNodeAlt);
                if (altPos != Vector3::Zero() && altPos.sqrMagnitude() >= 1.0f) {
                    localPos = altPos;
                    headNode = headNodeAlt;
                }
            }
        }

        // 5. Resolve Enemy Player Dictionary (Match_PlayerDict = 0xC0)
        uint64_t dictAddr = rpm<uint64_t>(BaseGame + Offsets::Match_PlayerDict);
        if (!is_valid_ptr(dictAddr) && is_valid_ptr(m_Match)) {
            dictAddr = rpm<uint64_t>(m_Match + Offsets::Match_PlayerDict);
        }
        if (!dictAddr) {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        MonoDictionary dict(dictAddr);
        if (!dict.valid) {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        int nv = dict.getNumValues();
        MonoDictionary::EntryBlock blk{};

        // 6. Iterate dictionary entries in 64-slot batch blocks (zero per-element syscall overhead)
        for (int i = 0; i < nv; i++) {
            if (i % MonoDictionary::kBlockSize == 0 && !dict.read_block(i, blk)) break;
            uint64_t enemy = blk.value[i % MonoDictionary::kBlockSize];
            if (!is_valid_ptr(enemy) || enemy == localPlayer) continue;

            // Execute single-syscall span read across 0x70 - 0x788
            PlayerSpan span(enemy);
            if (!span.ok) continue;

            // Reject dead entities
            if (span.at<bool>(Offsets::Player_IsDead)) continue;
            bool isBot = span.at<bool>(Offsets::Player_IsBot);

            // Health & armor validation: strictly reject dead, knocked-out or corrupted entities
            int curHP = 0, maxHP = 0, curArmor = 0;
            readHealthAndArmor(span.at<uint64_t>(Offsets::Player_HP), curHP, maxHP, curArmor);
            if (curHP <= 0 || curHP > 1000) continue;

            // Teammate & Self filter via AvatarManager UMAData
            auto avatarMgr = span.at<uint64_t>(Offsets::Player_Avatar);
            if (!avatarMgr) {
                avatarMgr = span.at<uint64_t>(Offsets::Player::Avatar_Alt);
            }
            if (!avatarMgr) continue;
            auto uma = rpm<uint64_t>(avatarMgr + Offsets::Avatar_Uma);
            if (!uma) continue;

            bool isVisible = rpm<bool>(uma + Offsets::Avatar::Uma_Visible);

            auto umaData = rpm<uint64_t>(uma + Offsets::Uma_Data);
            if (!umaData) continue;
            if (rpm<bool>(umaData + Offsets::Avatar::UmaData_Teammate)) continue;
            if (rpm<bool>(umaData + Offsets::Avatar::UmaData_LocalPlayer)) continue;

            // 7. Resolve Head World Position (Primary 0x6A0)
            uint64_t hNode = span.at<uint64_t>(Offsets::Bone_Head);
            Vector3 headWorld = hNode ? GetNodePosition(hNode) : Vector3::Zero();
            if (Offsets::Bone_Head_Alt && (headWorld == Vector3::Zero() || headWorld.sqrMagnitude() < 1.0f)) {
                uint64_t hNodeAlt = span.at<uint64_t>(Offsets::Bone_Head_Alt);
                if (hNodeAlt) {
                    Vector3 altPos = GetNodePosition(hNodeAlt);
                    if (altPos != Vector3::Zero() && altPos.sqrMagnitude() >= 1.0f) {
                        headWorld = altPos;
                        hNode = hNodeAlt;
                    }
                }
            }
            if (headWorld == Vector3::Zero() || headWorld.sqrMagnitude() < 1.0f) continue;
            if (!std::isfinite(headWorld.x) || !std::isfinite(headWorld.y) || !std::isfinite(headWorld.z)) continue;
            if (headWorld.y < -150.0f || headWorld.y > 600.0f) continue;

            // Distance filter against user-configured Max Range slider (0.0f = Unlimited Range)
            float dist = calculate_distance(localPos, headWorld);
            float maxTrackingDist = cfg::esp::max_range.load(std::memory_order_relaxed);
            if (dist < 0.5f || (maxTrackingDist > 0.0f && dist > maxTrackingDist)) continue;

            // Project Head to screen coordinates
            Vector3 headScreen = WorldToScreenPoint(viewMatrix, headWorld);
            if (headScreen.z < 0.001f) continue;

            // 8. Resolve Foot / Toe Node strictly from real memory transforms
            uint64_t footNode = span.at<uint64_t>(Offsets::Bone_LeftToe);
            if (!footNode) footNode = span.at<uint64_t>(Offsets::Bone::LeftToe_Alt);
            if (!footNode) footNode = span.at<uint64_t>(Offsets::Bone_Root);
            if (!footNode) footNode = span.at<uint64_t>(Offsets::Bone::Root_Alt);

            Vector3 toeScreen = footNode ? boneScreen(footNode) : Vector3::Zero();
            if (toeScreen.z < 0.001f && footNode != span.at<uint64_t>(Offsets::Bone_RightToe)) {
                uint64_t rightToe = span.at<uint64_t>(Offsets::Bone_RightToe);
                if (rightToe) toeScreen = boneScreen(rightToe);
            }

            // 9. Construct clean 64-byte PlayerData record
            PlayerData pd;
            pd.addr       = enemy;
            pd.Head       = headScreen;
            pd.Toe        = toeScreen;
            pd.headWorld  = headWorld;
            pd.isBot      = isBot;
            pd.isVisible  = isVisible;
            pd.distance   = dist;
            pd.curHP      = curHP;
            pd.maxHP      = maxHP;
            pd.curArmor   = curArmor;

            tmp.push_back(pd);
        }

        // 10. Atomic thread-safe swap into published repository
        {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.swap(tmp);
        }
    }

    // Background worker loop running data::update() with high-refresh real-time cadence
    static void loop() {
        while (g_run.load(std::memory_order_relaxed)) {
            update();
            int iv = cfg::aim::read_interval_ms.load(std::memory_order_relaxed);
            if (iv < 2) iv = 2;
            if (iv > 50) iv = 50;

            // Throttle politely to 100ms when idle in lobby to conserve CPU & battery
            if (!inMatch.load(std::memory_order_relaxed)) {
                iv = 100;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(iv));
        }
    }

    // Spawns the data extraction worker thread
    void thread_start() {
        if (g_run.exchange(true)) return;
        if (g_thread.joinable()) g_thread.join();
        g_thread = std::thread(loop);
    }

    // Safely halts the data worker thread and clears player buffers
    void thread_stop() {
        if (!g_run.exchange(false)) return;
        if (g_thread.joinable()) g_thread.join();
        std::lock_guard<std::mutex> lk(players_mutex);
        players.clear();
    }
}
