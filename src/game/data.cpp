#include "data.hpp"
#include "../offsets/offsets.hpp"
#include "../other/utils.hpp"
#include "../other/memory.hpp"
#include "../other/logger.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

// ==============================================================================================
// FILE: src/game/data.cpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
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
//    - Consumed by aimbot::acquire_target() in src/func/aim/target.hpp.
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
    // Every field read directly off the player object sits between 0x70 and 0x778
    // (Player::HP .. Player::Avatar). Pulling that span in a single process_vm_readv / IOCTL DMA
    // replaces roughly 10 individual syscalls per player with 1, eliminating CPU context switches.
    static constexpr size_t kSpanLo = Offsets::Player::HP;
    static constexpr size_t kSpanHi = Offsets::Player::Avatar + 0x10; // 0x778 + 0x10 = 0x788 span coverage

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
        if (maxHP <= 0) maxHP = Offsets::Player::StandardBaseHP; // Fallback baseline (200 HP)
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

    // Master memory walk cycle: executed continuously on the dedicated background data thread
    void update() {
        // Atomic snapshot of live PID and module base to prevent race conditions during game exit
        const auto snap = proc::snapshot();
        if (snap.pid <= 0 || snap.lib == 0) return;

        std::vector<PlayerData> tmp;
        tmp.reserve(64);

        // 1. Resolve GameFacade Cell from libil2cpp.so base
        auto GfCell = rpm<uint64_t>(snap.lib + oxorany(Offsets::GameFacade));
        if (!GfCell) {
            if (logger::should_log("err_gfcell", 2500)) {
                logger::log_not_found("GameFacade Cell (Offset +0x%lX from libil2cpp.so 0x%lX)", Offsets::GameFacade, (unsigned long)snap.lib);
            }
            return;
        }

        uint64_t BaseGame = 0;
        uint64_t StaticFields = 0;

        // Path A: GfCell is GameFacade Il2CppClass directly (P2 = Read(GameFacade_c + 0xB8); BaseGame = Read(P2))
        uint64_t sf1 = rpm<uint64_t>(GfCell + Offsets::GameFacade_P2);
        if (is_valid_ptr(sf1)) {
            uint64_t curGame1 = rpm<uint64_t>(sf1 + 0x0);
            uint64_t curMatch1 = rpm<uint64_t>(sf1 + 0x8);
            if (is_valid_ptr(curGame1)) { BaseGame = curGame1; StaticFields = sf1; }
            else if (is_valid_ptr(curMatch1)) { BaseGame = curMatch1; StaticFields = sf1; }
        }

        // Path B: GfCell is a pointer to GameFacade Il2CppClass (indirect pointer fallback)
        if (!is_valid_ptr(BaseGame)) {
            uint64_t GfClass = rpm<uint64_t>(GfCell);
            if (is_valid_ptr(GfClass)) {
                uint64_t sf2 = rpm<uint64_t>(GfClass + Offsets::GameFacade_P2);
                if (is_valid_ptr(sf2)) {
                    uint64_t curGame2 = rpm<uint64_t>(sf2 + 0x0);
                    uint64_t curMatch2 = rpm<uint64_t>(sf2 + 0x8);
                    if (is_valid_ptr(curGame2)) { BaseGame = curGame2; StaticFields = sf2; }
                    else if (is_valid_ptr(curMatch2)) { BaseGame = curMatch2; StaticFields = sf2; }
                }
            }
        }

        if (!is_valid_ptr(BaseGame)) {
            if (logger::should_log("err_bg", 2500)) {
                logger::log_not_found("BaseGame Instance (Dual-Path Failed from GfCell 0x%lX)", (unsigned long)GfCell);
            }
            return;
        }

        // 2. Resolve Match and Local Player entities
        auto m_Match = rpm<uint64_t>(BaseGame + Offsets::BaseGame_Match);
        auto localPlayer = m_Match ? rpm<uint64_t>(m_Match + Offsets::Match_LocalPlayer) : 0;
        bool matchActive = rpm<bool>(BaseGame + Offsets::MatchIsRunning) || (localPlayer != 0);
        inMatch.store(matchActive);

        // If match is not currently active, flush lists and sleep gracefully
        if (!matchActive) {
            if (logger::should_log("diag_match_idle", 3000)) {
                logger::log_info("Match Status: Waiting for match (BaseGame=0x%lX, Match=0x%lX)", (unsigned long)BaseGame, (unsigned long)m_Match);
            }
            localPlayerAddr.store(0);
            {
                std::lock_guard<std::mutex> lk(players_mutex);
                if (!players.empty()) players.clear();
            }
            return;
        }

        if (logger::should_log("diag_match_active", 3000)) {
            logger::log_found("Match Active: BaseGame=0x%lX | Match=0x%lX | LocalPlayer=0x%lX", (unsigned long)BaseGame, (unsigned long)m_Match, (unsigned long)localPlayer);
        }

        // Feature-muted mode: if user disabled all features in menu, skip heavy camera/bone calculations
        if (!cfg::any_feature_enabled()) {
            {
                std::lock_guard<std::mutex> lk(players_mutex);
                if (!players.empty()) players.clear();
            }
            return;
        }

        if (!m_Match) {
            if (logger::should_log("err_match", 2500)) {
                logger::log_not_found("Match Instance (Offset +0x%lX from BaseGame 0x%lX)", Offsets::BaseGame_Match, (unsigned long)BaseGame);
            }
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        // Store local player address for aimbot and speed mod fast-path lookup
        localPlayerAddr.store(localPlayer ? localPlayer : 0);

        if (!localPlayer) {
            if (logger::should_log("err_lp", 2500)) {
                logger::log_not_found("LocalPlayer Entity (Offset +0x%lX from Match 0x%lX)", Offsets::Match_LocalPlayer, (unsigned long)m_Match);
            }
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        // 3. Resolve Camera ViewMatrix chain
        uint64_t Follow = 0, Cam = 0, IntPtr = 0;

        Follow = rpm<uint64_t>(localPlayer + Offsets::Player_Camera);
        if (Follow) Cam = rpm<uint64_t>(Follow + Offsets::Camera_Follow);
        if (Cam)    IntPtr = rpm<uint64_t>(Cam + Offsets::Camera_IntPtr);

        if (!IntPtr) {
            // Fallback auxiliary camera offset chain
            Follow = rpm<uint64_t>(localPlayer + Offsets::Player::FollowCamera_Sub);
            if (Follow) Cam = rpm<uint64_t>(Follow + Offsets::Camera::Follow_Alt);
            if (Cam)    IntPtr = rpm<uint64_t>(Cam + Offsets::Camera::IntPtr_Alt);
        }

        if (!IntPtr) {
            if (logger::should_log("err_cam", 2500)) {
                logger::log_not_found("Camera Native IntPtr (Follow=0x%lX, Cam=0x%lX from LocalPlayer 0x%lX)", (unsigned long)Follow, (unsigned long)Cam, (unsigned long)localPlayer);
            }
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        // Read 4x4 ViewMatrix from Camera native instance
        viewMatrix = rpm<matrix>(IntPtr + Offsets::Camera_Matrix);

        // 4. Resolve Local Player Head Bone Position (Primary 0x6A8, Fallback 0x6A0)
        Vector3 localPos = Vector3::Zero();
        uint64_t headNode = rpm<uint64_t>(localPlayer + Offsets::Bone_Head);
        if (headNode) localPos = GetNodePosition(headNode);
        if (localPos == Vector3::Zero() || localPos.sqrMagnitude() < 1.0f) {
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
            if (logger::should_log("err_dict_addr", 2500)) {
                logger::log_not_found("Player Dictionary Address (Offset +0x%lX from BaseGame 0x%lX / Match 0x%lX)", Offsets::Match_PlayerDict, (unsigned long)BaseGame, (unsigned long)m_Match);
            }
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        MonoDictionary dict(dictAddr);
        if (!dict.valid) {
            if (logger::should_log("err_dict_valid", 2500)) {
                logger::log_not_found("MonoDictionary Validation Failed on 0x%lX", (unsigned long)dictAddr);
            }
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

            // Health & armor validation
            int curHP = 0, maxHP = 0, curArmor = 0;
            readHealthAndArmor(span.at<uint64_t>(Offsets::Player_HP), curHP, maxHP, curArmor);
            if (curHP <= 0) continue;

            // Teammate & Self filter via AvatarManager UMAData
            auto avatarMgr = span.at<uint64_t>(Offsets::Player_Avatar);
            if (!avatarMgr) {
                avatarMgr = span.at<uint64_t>(Offsets::Player::Avatar_Alt);
            }
            if (!avatarMgr) continue;
            auto uma = rpm<uint64_t>(avatarMgr + Offsets::Avatar_Uma);
            if (!uma) continue;

            auto umaData = rpm<uint64_t>(uma + Offsets::Uma_Data);
            if (umaData) {
                if (rpm<bool>(umaData + Offsets::Avatar::UmaData_Teammate)) continue;
                if (rpm<bool>(umaData + Offsets::Avatar::UmaData_LocalPlayer)) continue;
            }

            // 7. Resolve Head World Position (Primary 0x6A8, Fallback 0x6A0)
            uint64_t hNode = span.at<uint64_t>(Offsets::Bone_Head);
            Vector3 headWorld = hNode ? GetNodePosition(hNode) : Vector3::Zero();
            if (headWorld == Vector3::Zero() || headWorld.sqrMagnitude() < 1.0f) {
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

            // Distance filter against user-configured Max Range slider
            float dist = calculate_distance(localPos, headWorld);
            float maxTrackingDist = cfg::esp::max_range.load(std::memory_order_relaxed);
            if (maxTrackingDist < 50.0f) maxTrackingDist = 200.0f;
            if (dist < 0.8f || dist > maxTrackingDist) continue;

            // Project Head to screen coordinates
            Vector3 headScreen = WorldToScreenPoint(viewMatrix, headWorld);
            if (headScreen.z < 0.001f) continue;

            // 8. Resolve Foot / Toe Node (Primary LeftToe 0x6F0, Fallbacks 0x6E8, 0x6D0)
            uint64_t footNode = span.at<uint64_t>(Offsets::Bone_LeftToe);
            if (!footNode) footNode = span.at<uint64_t>(Offsets::Bone::LeftToe_Alt);
            if (!footNode) footNode = span.at<uint64_t>(Offsets::Bone_Root);
            if (!footNode) footNode = span.at<uint64_t>(Offsets::Bone::Root_Alt);
            if (!footNode) footNode = span.at<uint64_t>(Offsets::Player_FootTF);

            Vector3 toeScreen = footNode ? boneScreen(footNode) : Vector3::Zero();
            if (toeScreen.z < 0.001f) {
                uint64_t footAlt = span.at<uint64_t>(Offsets::Player::FootTF_Alt);
                if (footAlt) toeScreen = boneScreen(footAlt);
            }
            if (toeScreen.z < 0.001f) {
                // Synthetic foot projection based on human standard height (1.75m)
                Vector3 toeWorld = headWorld;
                toeWorld.y -= Offsets::Player::ModelHeight;
                toeScreen = WorldToScreenPoint(viewMatrix, toeWorld);
            }
            if (toeScreen.z < 0.001f) continue;

            // 9. Construct clean 64-byte PlayerData record
            PlayerData pd;
            pd.addr       = enemy;
            pd.Head       = headScreen;
            pd.Toe        = toeScreen;
            pd.headWorld  = headWorld;
            pd.isBot      = isBot;
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

        if (logger::should_log("diag_players_summary", 2500)) {
            logger::log_info("Players Parsed: %zu active enemies (Total Dict Entries: %d)", players.size(), nv);
        }
    }

    // Background worker loop running data::update() with adaptive throttling
    static void loop() {
        while (g_run.load(std::memory_order_relaxed)) {
            update();
            int iv = cfg::aim::read_interval_ms;
            if (iv < 8) iv = 8;
            if (iv > 100) iv = 100;

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
