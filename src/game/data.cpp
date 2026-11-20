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
// PURPOSE: Core memory extraction pipeline synchronized 100% with Offsetsmax.h & pewpew engine.
// ==============================================================================================

namespace data {

    std::vector<PlayerData>  players;
    std::mutex               players_mutex;

    std::atomic<uint64_t>    localPlayerAddr{0};
    std::atomic<bool>        inMatch{false};

    static matrix            viewMatrix{};

    std::mutex               viewMatrix_mutex;
    matrix                   viewMatrix_pub{};

    static std::atomic<bool> g_run{false};
    static std::thread       g_thread;

    // Single chunk HP extraction matching Offsetsmax.h (Player_Data = 0x70)
    static bool readHP(uint64_t player, int& outHP) {
        outHP = 0;
        auto pool = rpm<uint64_t>(player + Offsets::Player_Data); // 0x70
        if (!is_valid_ptr(pool)) return false;
        auto datas = rpm<uint64_t>(pool + Offsets::HP_List);      // 0x10 (GetCur)
        if (!is_valid_ptr(datas)) return false;
        auto elem = rpm<uint64_t>(datas + Offsets::HP_Entry);     // 0x20 (HP)
        if (!is_valid_ptr(elem)) return false;
        outHP = rpm<int>(elem + Offsets::HP_Value);               // 0x18 (Health)
        return true;
    }

    static void readHealthAndArmor(uint64_t pool, int& curHP, int& maxHP, int& curArmor) {
        curHP = 0;
        maxHP = 200;
        curArmor = 0;
        if (!is_valid_ptr(pool)) return;
        auto datas = rpm<uint64_t>(pool + Offsets::HP_List); // 0x10
        if (!is_valid_ptr(datas)) return;

        auto elem0 = rpm<uint64_t>(datas + Offsets::HP_Entry); // 0x20
        if (is_valid_ptr(elem0)) {
            curHP = rpm<int>(elem0 + Offsets::HP_Value);
        }

        auto elem1 = rpm<uint64_t>(datas + 0x28); // MaxHP entry
        if (is_valid_ptr(elem1)) {
            int m = rpm<int>(elem1 + Offsets::HP_Value);
            if (m > 0 && m <= 1000) maxHP = m;
        }
        if (maxHP <= 0 && curHP > 0) maxHP = curHP;

        auto elem4 = rpm<uint64_t>(datas + 0x40); // Armor entry
        if (is_valid_ptr(elem4)) {
            curArmor = rpm<int>(elem4 + Offsets::HP_Value);
        }
    }

    // Dynamic ADRP+LDR scanner from pewpew reference engine
    static uint64_t resolve_gamefacade_dynamic(uint64_t lib_base) {
        if (!lib_base) return 0;
        const uint64_t rvas[] = { 0x75ebd88, 0x75e9240 };
        for (uint64_t rva : rvas) {
            uint32_t insns[8] = {0};
            if (!mem_read(lib_base + rva, insns, sizeof(insns))) continue;

            for (int i = 0; i < 6; i++) {
                uint32_t insn1 = insns[i];
                if ((insn1 & 0x9F000000) == 0x90000000) {
                    int reg_adrp = insn1 & 0x1F;
                    int64_t immlo = (insn1 >> 29) & 0x3;
                    int64_t immhi = (insn1 >> 5) & 0x7FFFF;
                    int64_t imm = (immhi << 2) | immlo;
                    if (imm & (1 << 20)) imm |= ~((int64_t)0x1FFFFF);
                    uint64_t pc = lib_base + rva + (i * 4);
                    uint64_t page = (pc & ~0xFFFULL) + (imm << 12);

                    for (int j = i + 1; j < i + 4 && j < 8; j++) {
                        uint32_t insn2 = insns[j];
                        if ((insn2 & 0xFFC00000) == 0xF9400000) {
                            int rn = (insn2 >> 5) & 0x1F;
                            if (rn == reg_adrp) {
                                uint64_t imm12 = ((insn2 >> 10) & 0xFFF) << 3;
                                uint64_t target_addr = page + imm12;
                                if (target_addr >= lib_base) {
                                    uint64_t offset = target_addr - lib_base;
                                    if (offset > 0x100000 && offset < 0x20000000) {
                                        return offset;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return 0;
    }

    static inline bool is_valid_matrix(const matrix& m) {
        if (std::isnan(m.m11) || std::isnan(m.m22) || std::isnan(m.m33) || std::isnan(m.m44)) return false;
        if (std::isinf(m.m11) || std::isinf(m.m22) || std::isinf(m.m33) || std::isinf(m.m44)) return false;
        float col1 = fabsf(m.m11) + fabsf(m.m21) + fabsf(m.m31);
        float col2 = fabsf(m.m12) + fabsf(m.m22) + fabsf(m.m32);
        float col4 = fabsf(m.m14) + fabsf(m.m24) + fabsf(m.m34) + fabsf(m.m44);
        return (col1 > 0.05f && col2 > 0.05f && col4 > 0.05f);
    }

    void update() {
        const auto snap = proc::snapshot();
        if (snap.pid <= 0 || snap.lib == 0) return;

        std::vector<PlayerData> tmp;
        tmp.reserve(64);

        // 1. Resolve GameFacade (Static Offsetsmax.h 0xAD68E18 + Dynamic ARM64 scanner)
        uint64_t gf_offset = Offsets::GameFacade;
        static uint64_t s_dyn_gf = 0;
        static bool s_dyn_checked = false;
        if (!s_dyn_checked && snap.lib != 0) {
            s_dyn_checked = true;
            s_dyn_gf = resolve_gamefacade_dynamic(snap.lib);
        }

        uint64_t gf_ptr = snap.lib + gf_offset;
        uint64_t GfCell = rpm<uint64_t>(gf_ptr);
        if (!is_valid_ptr(GfCell) && s_dyn_gf != 0 && s_dyn_gf != gf_offset) {
            gf_offset = s_dyn_gf;
            gf_ptr = snap.lib + gf_offset;
            GfCell = rpm<uint64_t>(gf_ptr);
        }

        if (!is_valid_ptr(GfCell)) {
            const uintptr_t gfCandidates[] = { Offsets::GameFacade, 0xAD6DC18, 0xA72E4E0 };
            for (uintptr_t cand : gfCandidates) {
                uint64_t cval = rpm<uint64_t>(snap.lib + cand);
                if (is_valid_ptr(cval)) {
                    GfCell = cval;
                    break;
                }
            }
        }

        if (!is_valid_ptr(GfCell)) return;

        // 2. Resolve BaseGame via Staticfield (Offsetsmax.h Staticfield = 0xB8)
        uint64_t BaseGame = 0;
        uint64_t sf1 = rpm<uint64_t>(GfCell + Offsets::Staticfield);
        if (is_valid_ptr(sf1)) {
            uint64_t curGame1 = rpm<uint64_t>(sf1 + 0x0);
            uint64_t curMatch1 = rpm<uint64_t>(sf1 + 0x8);
            if (is_valid_ptr(curGame1)) BaseGame = curGame1;
            else if (is_valid_ptr(curMatch1)) BaseGame = curMatch1;
        }

        if (!is_valid_ptr(BaseGame)) {
            uint64_t classPtr = rpm<uint64_t>(GfCell);
            if (is_valid_ptr(classPtr)) {
                uint64_t sf2 = rpm<uint64_t>(classPtr + Offsets::Staticfield);
                if (is_valid_ptr(sf2)) {
                    uint64_t cg2 = rpm<uint64_t>(sf2 + 0x0);
                    uint64_t cm2 = rpm<uint64_t>(sf2 + 0x8);
                    if (is_valid_ptr(cg2)) BaseGame = cg2;
                    else if (is_valid_ptr(cm2)) BaseGame = cm2;
                }
            }
        }

        if (!is_valid_ptr(BaseGame)) return;

        // 3. CurrentMatch (0x90) & localPlayer (0xD8)
        auto m_Match = rpm<uint64_t>(BaseGame + Offsets::CurrentMatch);
        uint64_t localPlayer = 0;
        if (is_valid_ptr(m_Match)) {
            localPlayer = rpm<uint64_t>(m_Match + Offsets::localPlayer);
        }
        if (!is_valid_ptr(localPlayer) && is_valid_ptr(BaseGame)) {
            localPlayer = rpm<uint64_t>(BaseGame + Offsets::localPlayer);
        }

        int matchState = is_valid_ptr(m_Match) ? rpm<int>(m_Match + Offsets::MatchIsRunning) : -1;
        bool matchActive = (matchState == 1) || is_valid_ptr(localPlayer) || is_valid_ptr(BaseGame);
        inMatch.store(matchActive);

        // 4. Resolve Camera ViewMatrix chain (FollowCamera 0x698, CameraBase 0xD8)
        uint64_t Camera = 0, FollowCamera = 0, IntPtrCam = 0;
        bool matrix_valid = false;

        auto try_camera = [&](uint64_t candidate) -> bool {
            if (!is_valid_ptr(candidate)) return false;
            // Case A: candidate -> IntPtrCam (0x10) -> Matrix (0x100)
            uint64_t intPtr = rpm<uint64_t>(candidate + Offsets::IntPtrCam); // 0x10
            if (is_valid_ptr(intPtr)) {
                matrix vm = rpm<matrix>(intPtr + Offsets::Matrix); // 0x100
                if (is_valid_matrix(vm)) {
                    Camera = candidate;
                    IntPtrCam = intPtr;
                    viewMatrix = vm;
                    matrix_valid = true;
                    return true;
                }
            }
            // Case B: candidate -> Matrix (0x100) directly
            matrix direct_vm = rpm<matrix>(candidate + Offsets::Matrix); // 0x100
            if (is_valid_matrix(direct_vm)) {
                Camera = candidate;
                IntPtrCam = candidate;
                viewMatrix = direct_vm;
                matrix_valid = true;
                return true;
            }
            return false;
        };

        if (is_valid_ptr(localPlayer)) {
            FollowCamera = rpm<uint64_t>(localPlayer + Offsets::FollowCamera); // 0x698
        }

        // Candidate 1: FollowCamera -> Camera (0x20) (matching STORM_MODS & Offsetsmax.h)
        if (is_valid_ptr(FollowCamera)) {
            uint64_t c20 = rpm<uint64_t>(FollowCamera + Offsets::Camera); // 0x20
            if (try_camera(c20)) goto cam_resolved;
            uint64_t c30 = rpm<uint64_t>(FollowCamera + 0x30);
            if (try_camera(c30)) goto cam_resolved;
            uint64_t c28 = rpm<uint64_t>(FollowCamera + 0x28);
            if (try_camera(c28)) goto cam_resolved;
            if (try_camera(FollowCamera)) goto cam_resolved;
        }

        // Candidate 2: BaseGame + CameraBase (0xD8) -> Camera (0x20)
        if (is_valid_ptr(BaseGame)) {
            uint64_t camMgr = rpm<uint64_t>(BaseGame + Offsets::CameraBase); // 0xD8
            if (is_valid_ptr(camMgr)) {
                uint64_t c20 = rpm<uint64_t>(camMgr + Offsets::Camera); // 0x20
                if (try_camera(c20)) goto cam_resolved;
                uint64_t c28 = rpm<uint64_t>(camMgr + 0x28);
                if (try_camera(c28)) goto cam_resolved;
                if (try_camera(camMgr)) goto cam_resolved;
            }
        }

        // Candidate 3: m_Match + CameraBase (0xD8) -> Camera (0x20)
        if (is_valid_ptr(m_Match)) {
            uint64_t camMgr = rpm<uint64_t>(m_Match + Offsets::CameraBase); // 0xD8
            if (is_valid_ptr(camMgr)) {
                uint64_t c20 = rpm<uint64_t>(camMgr + Offsets::Camera); // 0x20
                if (try_camera(c20)) goto cam_resolved;
                if (try_camera(camMgr)) goto cam_resolved;
            }
        }

    cam_resolved:
        localPlayerAddr.store(localPlayer ? localPlayer : 0);

        if (!matrix_valid && !is_valid_matrix(viewMatrix)) {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
            return;
        }

        {
            std::lock_guard<std::mutex> lk(viewMatrix_mutex);
            viewMatrix_pub = viewMatrix;
        }

        // 5. Resolve Dictionary & entitylist (Offsetsmax.h: Dictionary 0xC0, entryAddr 0x18, enemyList 0x10)
        uint64_t dictionary = rpm<uint64_t>(BaseGame + Offsets::Dictionary); // 0xC0
        if (!is_valid_ptr(dictionary) && is_valid_ptr(m_Match)) {
            dictionary = rpm<uint64_t>(m_Match + Offsets::Dictionary);
        }

        Vector3 localPos = Vector3::Zero();
        if (is_valid_ptr(localPlayer)) {
            uint64_t headTF = rpm<uint64_t>(localPlayer + Offsets::Head); // 0x6A8
            if (headTF) localPos = GetNodePosition(headTF);
            if (localPos == Vector3::Zero()) {
                uint64_t rootTF = rpm<uint64_t>(localPlayer + Offsets::Root); // 0x6D0
                if (rootTF) localPos = GetNodePosition(rootTF);
            }
        }

        float maxTrackingDist = cfg::esp::max_range.load(std::memory_order_relaxed);

        // 6. Entity Processing Lambda
        auto process_enemy = [&](uint64_t enemy) {
            if (!is_valid_ptr(enemy) || enemy == localPlayer) return;

            // Reject duplicates already in tmp
            for (const auto& existing : tmp) {
                if (existing.addr == enemy) return;
            }

            // 1. Dead check (0 = alive)
            if (rpm<uint8_t>(enemy + Offsets::Player_IsDead) != 0) return; // 0x7C

            // 2. AvatarManager & Uma validation
            auto avatarMgr = rpm<uint64_t>(enemy + Offsets::AvatarManager); // 0x778
            if (!is_valid_ptr(avatarMgr)) return;

            auto umaSimple = rpm<uint64_t>(avatarMgr + Offsets::Avatar); // 0x138
            if (!is_valid_ptr(umaSimple)) return;

            // 3. UmaData & Team check
            auto umaData = rpm<uint64_t>(umaSimple + Offsets::Avatar_Data); // 0x28
            if (is_valid_ptr(umaData)) {
                if (rpm<uint8_t>(umaData + Offsets::Avatar_Data_IsTeam) != 0) return; // 0x81 (teammate)
            }

            // 4. Bone positions (Head = 0x6A8, Root = 0x6D0)
            auto headTF = rpm<uint64_t>(enemy + Offsets::Head); // 0x6A8
            if (!is_valid_ptr(headTF)) return;
            Vector3 headWorld = GetNodePosition(headTF);
            if (headWorld == Vector3::Zero()) return;

            Vector3 feetWorld = Vector3::Zero();
            auto feetTF = rpm<uint64_t>(enemy + Offsets::Root); // 0x6D0
            if (is_valid_ptr(feetTF)) {
                feetWorld = GetNodePosition(feetTF);
            }
            if (feetWorld == Vector3::Zero()) {
                feetWorld = Vector3(headWorld.x, headWorld.y - 1.75f, headWorld.z);
            }

            float dist = (localPos != Vector3::Zero()) ? calculate_distance(localPos, headWorld) : 10.0f;
            if (maxTrackingDist > 0.0f && dist > maxTrackingDist) return;

            Vector3 headScreen = WorldToScreenPoint(viewMatrix, headWorld);
            if (headScreen.z < 0.05f) return;

            Vector3 toeScreen = WorldToScreenPoint(viewMatrix, feetWorld);

            // 5. PhyXdata & Knocked check (Offsetsmax.h: PhyXdata 0x1D60 -> maybeDead 0x20 -> isKnocked 0x10)
            bool isKnocked = false;
            auto phyxData = rpm<uint64_t>(enemy + Offsets::PhyXdata); // 0x1D60
            if (is_valid_ptr(phyxData)) {
                auto maybeDead = rpm<uint64_t>(phyxData + Offsets::maybeDead); // 0x20
                if (is_valid_ptr(maybeDead) && rpm<int>(maybeDead + Offsets::isKnocked) == 8) { // 0x10
                    isKnocked = true;
                }
            }

            // 6. Health & Armor extraction
            int curHP = 0, maxHP = 200, curArmor = 0;
            readHealthAndArmor(rpm<uint64_t>(enemy + Offsets::Player_Data), curHP, maxHP, curArmor);
            if (curHP <= 0) {
                readHP(enemy, curHP);
            }
            if (curHP <= 0) curHP = 100;

            bool isBot = rpm<bool>(enemy + Offsets::IsBot); // 0x4A8
            uint8_t isVis = rpm<uint8_t>(umaSimple + Offsets::Avatar_IsVisible); // 0x101

            PlayerData pd;
            pd.addr      = enemy;
            pd.Head      = headScreen;
            pd.Toe       = toeScreen;
            pd.headWorld = headWorld;
            pd.isBot     = isBot;
            pd.isVisible = (isVis == 1);
            pd.isKnocked = isKnocked;
            pd.distance  = dist;
            pd.curHP     = curHP;
            pd.maxHP     = maxHP;
            pd.curArmor  = curArmor;

            tmp.push_back(pd);
        };

        // 7. Multi-Tier Dictionary & Entity Extraction
        auto read_dict_blocks = [&](uint64_t dictAddr) -> bool {
            if (!is_valid_ptr(dictAddr)) return false;
            MonoDictionary dict(dictAddr);
            if (dict.valid && dict.getNumValues() > 0) {
                int nv = std::min(dict.getNumValues(), 200);
                MonoDictionary::EntryBlock blk{};
                for (int i = 0; i < nv; i++) {
                    if (i % MonoDictionary::kBlockSize == 0) {
                        if (!dict.read_block(i, blk)) {
                            i += MonoDictionary::kBlockSize - 1;
                            continue;
                        }
                    }
                    uint64_t enemy = blk.value[i % MonoDictionary::kBlockSize];
                    if (is_valid_ptr(enemy)) {
                        process_enemy(enemy);
                    }
                }
                return !tmp.empty();
            }
            return false;
        };

        auto read_entries_stride = [&](uint64_t dictAddr) -> bool {
            if (!is_valid_ptr(dictAddr)) return false;
            uint64_t entries = rpm<uint64_t>(dictAddr + Offsets::entryAddr); // 0x18
            if (!is_valid_ptr(entries)) return false;

            int count = rpm<int>(dictAddr + 0x20); // Dict._count
            if (count <= 0 || count > 500) {
                count = rpm<int>(entries + 0x18); // Array length
            }
            if (count <= 0 || count > 500) count = 100;

            uint64_t entries_data = entries + 0x20; // Skip Il2CppArray header
            for (int i = 0; i < count; i++) {
                uint64_t entryAddr = entries_data + (i * 0x18);
                int32_t hashCode = rpm<int32_t>(entryAddr);
                if (hashCode < 0) continue;
                uint64_t enemy = rpm<uint64_t>(entryAddr + Offsets::enemyList); // 0x10
                if (is_valid_ptr(enemy)) {
                    process_enemy(enemy);
                }
            }
            return !tmp.empty();
        };

        auto read_flat_array = [&](uint64_t arrAddr) -> bool {
            if (!is_valid_ptr(arrAddr)) return false;
            for (int i = 0; i < 150; i++) {
                uint64_t enemy = rpm<uint64_t>(arrAddr + (i * sizeof(uint64_t)));
                if (is_valid_ptr(enemy)) {
                    process_enemy(enemy);
                }
            }
            return !tmp.empty();
        };

        const uintptr_t dictCandidates[] = {
            dictionary,
            is_valid_ptr(m_Match) ? rpm<uint64_t>(m_Match + Offsets::Dictionary) : 0,
            is_valid_ptr(BaseGame) ? rpm<uint64_t>(BaseGame + Offsets::LDictionaryEntities) : 0, // 0xC8
            is_valid_ptr(BaseGame) ? rpm<uint64_t>(BaseGame + Offsets::CurrentMatch) : 0,
        };

        for (uintptr_t cand : dictCandidates) {
            if (!is_valid_ptr(cand)) continue;
            if (read_dict_blocks(cand)) break;
            if (read_entries_stride(cand)) break;
            uint64_t innerList = rpm<uint64_t>(cand + Offsets::entryAddr);
            if (is_valid_ptr(innerList)) {
                if (read_flat_array(innerList + 0x20)) break;
                if (read_flat_array(innerList)) break;
            }
            if (read_flat_array(cand)) break;
        }

        // 8. Commit to published players repository
        {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.swap(tmp);
        }
    }

    // Background worker loop running data::update()
    static void loop() {
        while (g_run.load(std::memory_order_relaxed)) {
            update();
            int iv = cfg::aim::read_interval_ms.load(std::memory_order_relaxed);
            if (iv < 2) iv = 2;
            if (iv > 50) iv = 50;

            if (!inMatch.load(std::memory_order_relaxed)) {
                iv = 100;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(iv));
        }
    }

    void thread_start() {
        if (g_run.exchange(true)) return;
        if (g_thread.joinable()) g_thread.join();
        g_thread = std::thread(loop);
    }

    void thread_stop() {
        if (!g_run.exchange(false)) return;
        if (g_thread.joinable()) g_thread.join();
        std::lock_guard<std::mutex> lk(players_mutex);
        players.clear();
    }
}
