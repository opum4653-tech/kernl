#pragma once
#include "../other/vector3.h"
#include <cstdint>

// ==============================================================================================
// FILE: src/game/player.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Primary entity data contract representing parsed enemy players in game memory.
//
// CROSS-FILE ARCHITECTURAL GRAPH & DEPENDENCY MAPPINGS:
// ----------------------------------------------------------------------------------------------
// 1. PRODUCER (Data Extraction & Parsing):
//    - File: src/game/data.cpp -> data::update()
//    - Mechanism: Reads Il2Cpp entities via kernel driver RPM (process_vm_readv / IOCTL DMA).
//    - Populates: Head, Toe, headWorld, isBot, distance, curHP, maxHP, curArmor.
//    - Published to: data::players (std::vector<PlayerData>) under data::players_mutex.
//
// 2. CONSUMERS (Visuals & Combat Subsystems):
//    - Visuals (ESP Subsystem):
//      * src/func/esp/esp.hpp    -> func::esp::draw(): Iterates data::players snapshot every frame.
//      * src/func/esp/box.hpp    -> func::esp::computePlayerBox(): Derives 2D bounding boxes from Head & Toe.
//      * src/func/esp/line.hpp   -> func::esp::drawTracer(): Draws laser tracer lines to Head position.
//      * src/func/esp/health.hpp -> func::esp::drawSideArmorBar(): Renders armor gauge from curArmor.
//      * src/func/esp/info.hpp   -> func::esp::drawBotTag(): Renders server AI tag when isBot is true.
//    - Combat (Kernel Trajectory Subsystem):
//      * src/func/aim/target.hpp -> trajectory::acquire_target(): Evaluates FOV angle and target hysteresis.
//      * src/func/aim/silent.hpp -> trajectory::apply_trajectory(): Computes bullet vector to headWorld.
//      * src/func/aim/headshot.hpp -> evaluate_headshot_rate(): distance-aware headshot rate controller.
//
// MEMORY LAYOUT & CACHE-LINE ALIGNMENT:
// ----------------------------------------------------------------------------------------------
// Total Size: Exactly 64 Bytes (Matches ARM64 L1 Data Cache Line size).
// [Offset 0x00] uint64_t addr       : 8 bytes (Il2Cpp pointer to enemy entity in target memory)
// [Offset 0x08] Vector3  Head       : 12 bytes (Screen-space 2D projected coordinate {x, y, z=depth})
// [Offset 0x14] Vector3  Toe        : 12 bytes (Screen-space 2D projected coordinate {x, y, z=depth})
// [Offset 0x20] Vector3  headWorld  : 12 bytes (3D world coordinates {x, y, z} in Unity meters)
// [Offset 0x2C] bool     isBot      : 1 byte  (True if enemy is a server-spawned bot entity)
// [Offset 0x2D] uint8_t  _pad[3]    : 3 bytes (Implicit alignment padding for 4-byte boundaries)
// [Offset 0x30] float    distance   : 4 bytes (Physical distance in meters from local player)
// [Offset 0x34] int      curHP      : 4 bytes (Live current health parsed from ReplicationData pool)
// [Offset 0x38] int      maxHP      : 4 bytes (Maximum health pool limit, default 200)
// [Offset 0x3C] int      curArmor   : 4 bytes (Live armor/vest protection points, 0 - 100+)
// ==============================================================================================

struct PlayerData {
    // Il2Cpp entity base pointer in target process virtual address space
    uint64_t addr;

    // 2D Viewport Screen Projections (derived via WorldToScreenPoint in math.hpp)
    Vector3  Head;      // Screen position of skull top (Head node 0x6A0)
    Vector3  Toe;       // Screen position of feet/ground (LeftToe 0x6E8 / Root 0x6C8)

    // 3D Engine World Coordinates (extracted from Unity ITransformNode)
    Vector3  headWorld; // True 3D coordinate used for bullet trajectory calculations

    // Entity Metadata & Health Telemetry
    bool     isBot;     // Extracted from Player::IsBot (0x4A8)
    bool     isVisible; // Extracted from Avatar::Uma_Visible (0x101)
    bool     isKnocked; // Extracted from PhyXdata (0x1D60) -> maybeDead (0x20) -> isKnocked == 8
    float    distance;  // Euclidean distance between local player head and enemy head
    int      curHP;     // Current health extracted from m_PRIDataPool (0x70) Index 0
    int      maxHP;     // Maximum health extracted from m_PRIDataPool (0x70) Index 1
    int      curArmor;  // Current vest/armor points extracted from m_PRIDataPool Index 4

    // Default constructor zero-initializes all coordinates and sets safe defaults
    PlayerData()
        : addr(0),
          Head(Vector3::Zero()), Toe(Vector3::Zero()), headWorld(Vector3::Zero()),
          isBot(false), isVisible(false), isKnocked(false), distance(0.f),
          curHP(0), maxHP(200), curArmor(0)
    {}

    // Resolves 3D target coordinates: targetPos == 0 for Head, targetPos == 1 for Body/Chest
    Vector3 getTargetWorld(int targetPos = 0, float headBias = 0.0f) const {
        if (targetPos == 1) {
            // Chest / Body center: sternum level (0.40m below skull center)
            return Vector3(headWorld.x, headWorld.y - 0.40f, headWorld.z);
        }
        // Head / Skull center: headWorld + vertical elevation trim
        return (headBias != 0.0f) ? Vector3(headWorld.x, headWorld.y + headBias, headWorld.z) : headWorld;
    }
};
