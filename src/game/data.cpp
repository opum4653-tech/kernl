#include "data.hpp"
#include "../offsets/offsets.hpp"
#include "../other/utils.hpp"
#include "../other/memory.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

// ==============================================================================================
// FILE: src/game/data.cpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Core memory extraction pipeline synchronized 100% with Offsetsmax.h & pewpew engine.
//
// VISIBILITY & RPM OPTIMIZATION SUMMARY:
// ----------------------------------------------------------------------------------------------
// VISIBILITY RULE: ESP range == CAMERA FRUSTUM. No artificial distance limit (no 300m/150m).
//   A player is processed if and only if their projected head or feet fall inside the
//   screen bounds (+80px margin). Whatever the camera renders gets complete ESP data;
//   anything off-screen or behind the camera is dropped entirely (zero wasted RPM).
//
// RPM OPTIMIZATIONS (7 layers, ~90% reduction):
// 1. BATCH READ: Sequential fields read via single mem_read() blocks, not many rpm<T>().
// 2. TRANSFORM CACHE: GetNodePosition() results cached per-entity for 100ms (3 ticks).
// 3. INTERVAL THROTTLE: Default loop interval 33ms (30Hz) — ESP doesn't need 60Hz.
// 4. FRUSTUM CULLING: Off-screen players rejected before ANY detailed read.
// 5. READ ORDER RANDOMIZATION: Fisher-Yates shuffle prevents access pattern detection.
// 6. ESP-ONLY MODE GATE: Skip PhyXdata/IsKnocked when aimbot disabled.
// 7. DEAD/TEAMMATE REJECT: Cheap isDead + team checks before expensive Transform walks.
// ==============================================================================================

namespace data {

    std::vector<PlayerData>  players;
    std::mutex               players_mutex;

    std::atomic<uint64_t>    localPlayerAddr{0};
    std::atomic<bool>        inMatch{false};

    static matrix            viewMatrix{};

    std::mutex               viewMatrix_mutex;
    matrix                   viewMatrix_pub{};
    std::atomic<uint64_t>    viewMatrix_stamp{0}; // bumped on every matrix publish

    // Bone position cache — keyed by entity address, expires after 100ms
    std::unordered_map<uint64_t, BoneCache> bone_cache;
    std::mutex               bone_cache_mutex;

    // Cache validity window: 100ms = ~3 ticks at 33ms interval
    static constexpr int BONE_CACHE_TTL_MS = 100;

    // ── FRUSTUM-ONLY VISIBILITY ──────────────────────────────────────────────────
    // ESP range is EXACTLY what the camera renders: the screen frustum.
    // No artificial distance limit (no 300m, no 150m). A player is visible if and
    // only if their projected head/feet fall inside the screen bounds + small margin.
    // Frustum culling margin in pixels — catches players whose body is partially on-screen.
    static constexpr float FRUSTUM_MARGIN = 80.0f;

    // Read order randomization seed (thread_local for isolation)
    inline thread_local unsigned int s_read_order_seed = 0x4E494E4D;

    static std::atomic<bool> g_run{false};
    static std::thread       g_thread;

    // ── Bone cache helpers (mutex-guarded) ──────────────────────────────────────
    static bool bone_cache_get(uint64_t addr, Vector3& outHead, Vector3& outFeet) {
        std::lock_guard<std::mutex> lk(bone_cache_mutex);
        auto it = bone_cache.find(addr);
        if (it == bone_cache.end() || !it->second.valid) return false;
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - it->second.cachedAt).count();
        if (age > BONE_CACHE_TTL_MS) return false;
        outHead = it->second.headPos;
        outFeet = it->second.feetPos;
        return true;
    }

    static void bone_cache_put(uint64_t addr, const Vector3& head, const Vector3& feet) {
        BoneCache bc;
        bc.headPos = head;
        bc.feetPos = feet;
        bc.cachedAt = std::chrono::steady_clock::now();
        bc.valid = true;
        std::lock_guard<std::mutex> lk(bone_cache_mutex);
        bone_cache[addr] = bc;
    }

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

    // Batch HP + Armor extraction: reads 3 pointer slots in one pass
    // Reduces 6 individual rpm calls to 3 (pointers) + 3 (values) = still less than before
    // because we skip nullptr entries early
    static void readHealthAndArmor(uint64_t pool, int& curHP, int& maxHP, int& curArmor) {
        curHP = 0;
        maxHP = 200;
        curArmor = 0;
        if (!is_valid_ptr(pool)) return;

        // Batch read: pool+0x10 (HP_List), pool+0x18, pool+0x20 — read 0x30 bytes in one go
        // Layout: [0x10]=datas_ptr, [0x18]=?, [0x20]=?, we only need datas
        auto datas = rpm<uint64_t>(pool + Offsets::HP_List); // 0x10
        if (!is_valid_ptr(datas)) return;

        // Batch read 3 pointer slots from datas: 0x20, 0x28, 0x40
        // These are elem0 (curHP), elem1 (maxHP), elem4 (armor)
        // Read a 0x30-byte block covering 0x20..0x50 to get all 3 pointers in one RPM
        uint8_t ptrBuf[0x30] = {0};
        if (mem_read(datas + 0x20, ptrBuf, sizeof(ptrBuf))) {
            uint64_t elem0 = *reinterpret_cast<uint64_t*>(ptrBuf + 0x00); // datas+0x20
            uint64_t elem1 = *reinterpret_cast<uint64_t*>(ptrBuf + 0x08); // datas+0x28
            uint64_t elem4 = *reinterpret_cast<uint64_t*>(ptrBuf + 0x20); // datas+0x40

            if (is_valid_ptr(elem0)) {
                curHP = rpm<int>(elem0 + Offsets::HP_Value); // 0x18
            }
            if (is_valid_ptr(elem1)) {
                int m = rpm<int>(elem1 + Offsets::HP_Value);
                if (m > 0 && m <= 1000) maxHP = m;
            }
            if (maxHP <= 0 && curHP > 0) maxHP = curHP;

            if (is_valid_ptr(elem4)) {
                curArmor = rpm<int>(elem4 + Offsets::HP_Value);
            }
        } else {
            // Fallback: individual reads if batch fails
            auto elem0 = rpm<uint64_t>(datas + Offsets::HP_Entry);
            if (is_valid_ptr(elem0)) {
                curHP = rpm<int>(elem0 + Offsets::HP_Value);
            }
            auto elem1 = rpm<uint64_t>(datas + 0x28);
            if (is_valid_ptr(elem1)) {
                int m = rpm<int>(elem1 + Offsets::HP_Value);
                if (m > 0 && m <= 1000) maxHP = m;
            }
            if (maxHP <= 0 && curHP > 0) maxHP = curHP;

            auto elem4 = rpm<uint64_t>(datas + 0x40);
            if (is_valid_ptr(elem4)) {
                curArmor = rpm<int>(elem4 + Offsets::HP_Value);
            }
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

    void note_detached() {
        inMatch.store(false, std::memory_order_release);
        localPlayerAddr.store(0, std::memory_order_release);
        // Lock the two repositories separately — never nested — so this can be
        // called from the render thread without deadlocking the data worker.
        {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
        }
        {
            std::lock_guard<std::mutex> lk(bone_cache_mutex);
            bone_cache.clear();
        }
    }

    void update() {
        const auto snap = proc::snapshot();
        if (snap.pid <= 0 || snap.lib == 0) {
            note_detached();
            return;
        }

        // Dynamic GameFacade scanner state — one scan per attached lib base.
        static uint64_t s_dyn_gf = 0;
        static bool     s_dyn_checked = false;

        // ── ATTACHMENT CHANGE GUARD ─────────────────────────────────────────────
        // A new PID or a relocated libil2cpp.so means a new game instance.
        // Reusing cached bone positions from the old instance draws ghost
        // players at stale coordinates, and the dynamic GameFacade offset scan
        // belongs to the old module image. Invalidate both here.
        static int      s_bound_pid = 0;
        static uint64_t s_bound_lib = 0;
        if (s_bound_pid != snap.pid || s_bound_lib != snap.lib) {
            s_bound_pid = snap.pid;
            s_bound_lib = snap.lib;
            s_dyn_checked = false;
            s_dyn_gf = 0;
            std::lock_guard<std::mutex> lk(bone_cache_mutex);
            bone_cache.clear();
        }

        std::vector<PlayerData> tmp;
        tmp.reserve(64);

        // 1. Resolve GameFacade (Static Offsetsmax.h 0xAD68E18 + Dynamic ARM64 scanner)
        uint64_t gf_offset = Offsets::GameFacade;
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
            // MAX-ONLY candidates — no Global (0xA72E4E0) fallback.
            const uintptr_t gfCandidates[] = { Offsets::GameFacade, 0xAD6DC18 };
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
        // BaseGame is ALWAYS valid here (there is an early return when it is not),
        // so OR-ing it into matchActive made inMatch permanently true — including
        // in the lobby. Trust the authoritative match flag instead. Only when the
        // flag cannot be read at all do we fall back to local-player presence.
        bool matchActive = (matchState > 0);
        if (matchState < 0) matchActive = is_valid_ptr(localPlayer);
        inMatch.store(matchActive, std::memory_order_release);

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

        if (!matrix_valid) {
            // Do NOT republish the previous good matrix with a FRESH stamp.
            // That made the silent-aim staleness guard useless: a stalled camera
            // looked brand new on every tick. Keep the last published matrix
            // (so ESP does not vanish on a single hitch) but leave its stamp
            // untouched — the aim thread can now detect the stall properly.
            bool haveMatrix = false;
            {
                std::lock_guard<std::mutex> lk(viewMatrix_mutex);
                haveMatrix = is_valid_matrix(viewMatrix_pub);
            }
            if (!haveMatrix) {
                std::lock_guard<std::mutex> lk(players_mutex);
                players.clear();
                return;
            }
        } else {
            std::lock_guard<std::mutex> lk(viewMatrix_mutex);
            viewMatrix_pub = viewMatrix;
            viewMatrix_stamp.fetch_add(1, std::memory_order_release);
        }

        // 5. Resolve Dictionary & entitylist (Offsetsmax.h: Dictionary 0xC0, entryAddr 0x18, enemyList 0x10)
        uint64_t dictionary = rpm<uint64_t>(BaseGame + Offsets::Dictionary); // 0xC0
        if (!is_valid_ptr(dictionary) && is_valid_ptr(m_Match)) {
            dictionary = rpm<uint64_t>(m_Match + Offsets::Dictionary);
        }

        // ── Local player position with bone cache ────────────────────────────────
        Vector3 localPos = Vector3::Zero();
        if (is_valid_ptr(localPlayer)) {
            // Cache local player position too
            uint64_t headTF = rpm<uint64_t>(localPlayer + Offsets::Head); // 0x6A8
            if (headTF) {
                Vector3 ch, cf;
                if (bone_cache_get(localPlayer, ch, cf)) {
                    localPos = ch;
                } else {
                    localPos = GetNodePosition(headTF);
                    if (localPos != Vector3::Zero()) {
                        // Store the freshly walked head position — without the
                        // matching put this cache could never hit for the local
                        // player, so every tick paid for a full Transform walk.
                        bone_cache_put(localPlayer, localPos,
                                       Vector3(localPos.x, localPos.y - 1.75f, localPos.z));
                    }
                }
            }
            if (localPos == Vector3::Zero()) {
                uint64_t rootTF = rpm<uint64_t>(localPlayer + Offsets::Root); // 0x6D0
                if (rootTF) localPos = GetNodePosition(rootTF);
            }
        }

        // ── ESP RANGE = CAMERA FRUSTUM ONLY ──────────────────────────────────────
        // No cfg::esp::max_range distance gate. The camera frustum (screen bounds)
        // is the single source of truth for what ESP shows — nothing beyond it.

        // ── ESP-ONLY MODE DETECTION: If no aim features active, skip combat reads ──
        bool aimActive = cfg::aim::silent.load(std::memory_order_relaxed) ||
                         cfg::aim::auto_headshot.load(std::memory_order_relaxed) ||
                         cfg::aim::scope_aim.load(std::memory_order_relaxed);

        // ── Screen bounds for frustum culling ─────────────────────────────────────
        float screenW = g_sw.load(std::memory_order_relaxed);
        float screenH = g_sh.load(std::memory_order_relaxed);

        // ── OPTIMIZED Entity Processing with batch read, cache, distance cull, LOD ──
        auto process_enemy = [&](uint64_t enemy) {
            if (!is_valid_ptr(enemy) || enemy == localPlayer) return;

            // Reject duplicates already in tmp
            for (const auto& existing : tmp) {
                if (existing.addr == enemy) return;
            }

            // ── BATCH READ: Player header fields in one mem_read ────────────────
            // Read a block covering IsDead (0x7C), IsBot (0x4A8), Head (0x6A8), Root (0x6D0)
            // These are spread too far for a single contiguous read, so we batch the
            // close ones: IsDead at 0x7C and Player_Data at 0x70 — read 0x10 bytes
            uint8_t deadAndData[0x10] = {0};
            if (mem_read(enemy + Offsets::Player_Data, deadAndData, sizeof(deadAndData))) {
                // Player_Data = 0x70, so deadAndData[0x0C] = enemy+0x7C = IsDead
                uint8_t isDead = deadAndData[0x0C];
                if (isDead != 0) return; // Dead player — skip
            } else {
                // Fallback: individual read
                if (rpm<uint8_t>(enemy + Offsets::Player_IsDead) != 0) return;
            }

            // 2. AvatarManager & Uma validation (pointer chain — can't batch non-contiguous)
            auto avatarMgr = rpm<uint64_t>(enemy + Offsets::AvatarManager); // 0x778
            if (!is_valid_ptr(avatarMgr)) return;

            auto umaSimple = rpm<uint64_t>(avatarMgr + Offsets::Avatar); // 0x138
            if (!is_valid_ptr(umaSimple)) return;

            // 3. UmaData & Team check
            auto umaData = rpm<uint64_t>(umaSimple + Offsets::Avatar_Data); // 0x28
            if (is_valid_ptr(umaData)) {
                if (rpm<uint8_t>(umaData + Offsets::Avatar_Data_IsTeam) != 0) return; // 0x81 (teammate)
            }

            // ── TRANSFORM CACHE: Try cached bone positions first ─────────────────
            Vector3 headWorld = Vector3::Zero();
            Vector3 feetWorld = Vector3::Zero();

            Vector3 cachedHead, cachedFeet;
            if (bone_cache_get(enemy, cachedHead, cachedFeet)) {
                headWorld = cachedHead;
                feetWorld = cachedFeet;
            } else {
                // Cache miss — walk Transform hierarchy (expensive).
                // Read both head and root BEFORE deciding to drop the entity.
                // The old code returned immediately when the head node failed,
                // which silently removed every distant / LOD enemy from ESP —
                // the game renders those with a simplified skeleton where the
                // head ITransformNode is often absent or unresolvable.
                auto headTF = rpm<uint64_t>(enemy + Offsets::Head); // 0x6A8
                if (is_valid_ptr(headTF)) headWorld = GetNodePosition(headTF);

                auto feetTF = rpm<uint64_t>(enemy + Offsets::Root); // 0x6D0
                if (is_valid_ptr(feetTF)) feetWorld = GetNodePosition(feetTF);

                if (headWorld == Vector3::Zero()) {
                    // No usable head bone — synthesize it from the root position.
                    // A standing player is ~1.75m tall; this keeps far enemies
                    // on screen without any artificial distance cutoff.
                    if (feetWorld == Vector3::Zero()) return; // no transform data at all
                    headWorld = Vector3(feetWorld.x, feetWorld.y + 1.75f, feetWorld.z);
                } else if (feetWorld == Vector3::Zero()) {
                    feetWorld = Vector3(headWorld.x, headWorld.y - 1.75f, headWorld.z);
                }

                // Store in cache for next 100ms
                bone_cache_put(enemy, headWorld, feetWorld);
            }

            // Distance still computed for ESP box scaling / HUD info only — NOT for culling.
            float dist = (localPos != Vector3::Zero()) ? calculate_distance(localPos, headWorld) : 10.0f;

            Vector3 headScreen = WorldToScreenPoint(viewMatrix, headWorld);
            Vector3 toeScreen = WorldToScreenPoint(viewMatrix, feetWorld);

            // ── FRUSTUM-ONLY GATE: ESP range == camera frustum ─────────────────
            // The camera is the single source of truth. A player is processed if
            // and only if head OR feet project inside the screen bounds (+ margin).
            // Behind-camera (z <= 0.05) AND off-screen players are dropped entirely —
            // ESP draws nothing for them, so reading their HP/Armor would be wasted RPM.
            auto in_bounds = [&](const Vector3& s) -> bool {
                return s.z > 0.05f &&
                       s.x >= -FRUSTUM_MARGIN && s.x <= screenW + FRUSTUM_MARGIN &&
                       s.y >= -FRUSTUM_MARGIN && s.y <= screenH + FRUSTUM_MARGIN;
            };
            bool onScreen = in_bounds(headScreen) || in_bounds(toeScreen);
            if (!onScreen) return;

            // ── FULL DETAIL for every frustum-visible player ───────────────────
            // No distance tiers. Whatever the camera sees gets complete data.
            bool isKnocked = false;
            int curHP = 0, maxHP = 200, curArmor = 0;
            bool isBot = false, isVisible = false;

            // PhyXdata & Knocked (only when aim active — ESP alone does not need it)
            if (aimActive) {
                auto phyxData = rpm<uint64_t>(enemy + Offsets::PhyXdata); // 0x1D60
                if (is_valid_ptr(phyxData)) {
                    auto maybeDead = rpm<uint64_t>(phyxData + Offsets::maybeDead); // 0x20
                    if (is_valid_ptr(maybeDead) && rpm<int>(maybeDead + Offsets::isKnocked) == 8) {
                        isKnocked = true;
                    }
                }
            }

            // Health & Armor (reuse batched pool pointer when available)
            uint64_t pool = 0;
            if (deadAndData[0] != 0 || deadAndData[1] != 0) {
                pool = *reinterpret_cast<uint64_t*>(deadAndData); // enemy+0x70
            } else {
                pool = rpm<uint64_t>(enemy + Offsets::Player_Data);
            }
            readHealthAndArmor(pool, curHP, maxHP, curArmor);
            if (curHP <= 0) readHP(enemy, curHP);
            if (curHP <= 0) curHP = 100;

            isBot = rpm<bool>(enemy + Offsets::IsBot); // 0x4A8
            uint8_t isVis = rpm<uint8_t>(umaSimple + Offsets::Avatar_IsVisible); // 0x101
            isVisible = (isVis == 1);

            PlayerData pd;
            pd.addr      = enemy;
            pd.Head      = headScreen;
            pd.Toe       = toeScreen;
            pd.headWorld = headWorld;
            pd.isBot     = isBot;
            pd.isVisible = isVisible;
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
                // Scan every published entity up to a hard safety ceiling.
                // The old 200-entry cap could cut off distant players that are
                // inserted later in the dictionary — they were never parsed,
                // so no ESP line/box could ever be drawn for them.
                int nv = std::min(dict.getNumValues(), 512);
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

            // ── READ ORDER RANDOMIZATION: Fisher-Yates shuffle of indices ──────
            // Prevents deterministic memory access pattern detection.
            // Each tick processes players in a different order.
            int indices[500];
            for (int i = 0; i < count && i < 500; i++) indices[i] = i;
            for (int i = std::min(count, 500) - 1; i > 0; i--) {
                int j = rand_r(&s_read_order_seed) % (i + 1);
                int tmpIdx = indices[i]; indices[i] = indices[j]; indices[j] = tmpIdx;
            }

            for (int idx = 0; idx < count; idx++) {
                int i = indices[idx];
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

            // ── READ ORDER RANDOMIZATION: shuffle processing order ─────────────
            int indices[150];
            for (int i = 0; i < 150; i++) indices[i] = i;
            for (int i = 149; i > 0; i--) {
                int j = rand_r(&s_read_order_seed) % (i + 1);
                int t = indices[i]; indices[i] = indices[j]; indices[j] = t;
            }

            for (int idx = 0; idx < 150; idx++) {
                int i = indices[idx];
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

        // 9. Periodic bone cache cleanup — remove stale entries every 10 seconds
        static auto lastCacheClean = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCacheClean).count() >= 10) {
            lastCacheClean = now;
            std::lock_guard<std::mutex> cacheLk(bone_cache_mutex);
            for (auto it = bone_cache.begin(); it != bone_cache.end(); ) {
                auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - it->second.cachedAt).count();
                if (age > BONE_CACHE_TTL_MS * 5 || !it->second.valid) {
                    it = bone_cache.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    // Background worker loop running data::update()
    // OPTIMIZATION: Default interval 33ms (30Hz) — ESP doesn't need 60Hz.
    // This alone cuts total RPM per second by 50%.
    static void loop() {
        while (g_run.load(std::memory_order_relaxed)) {
            update();
            int iv = cfg::aim::read_interval_ms.load(std::memory_order_relaxed);
            // Enforce minimum 33ms (30Hz max) for data thread — even if aim thread runs faster
            if (iv < 33) iv = 33;
            if (iv > 50) iv = 50;

            if (!inMatch.load(std::memory_order_relaxed)) {
                iv = 100; // Lobby: slow poll
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
        {
            std::lock_guard<std::mutex> lk(players_mutex);
            players.clear();
        }
        {
            std::lock_guard<std::mutex> lk(bone_cache_mutex);
            bone_cache.clear();
        }
    }
}
