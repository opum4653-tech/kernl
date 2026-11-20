#pragma once

#include <cstdint>

// ==============================================================================================
// FILE: src/offsets/offsets.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// TARGET BUILD: Free Fire OB55 / Version 1.132.1 (ARM64 Il2Cpp)
// PURPOSE: Central verified offset registry. Encapsulates all object graph traversal displacements,
//          Unity transform internal layouts, and Il2Cpp structure field offsets.
//
// CROSS-FILE CONSUMPTION GRAPH:
// ----------------------------------------------------------------------------------------------
// 1. DATA EXTRACTION ENGINE (src/game/data.cpp):
//    - Uses Base::GameFacade, BaseGame_Match, Match_PlayerDict, Match_LocalPlayer.
//    - Uses Player::HP, Player::Avatar, Player::Bone_Head, Player::Bone_LeftToe.
//    - Uses Avatar::Uma, Avatar::Uma_Data, Avatar::UmaData_Teammate.
// 2. COMBAT & KERNEL TRAJECTORY (src/func/aim/):
//    - Uses Aim::Direction (0x40), Aim::StartPosition (0x4C) in silent.hpp.
//    - Uses Player::AimInfo (0xE58), Player::IsFiring (0x850), Player::Camera (0x690) in target.hpp.
// 3. MOTION & COMBAT MODS (src/func/speed.hpp):
//    - Uses Player::Attributes (0x768), Speed::RunSpeedUpScale (0x2C0).
//    - Uses Player::Weapon (0x600), Weapon::Data (0x18), Weapon::Recoil (0x80).
// 4. MATH & ENGINE TRANSFORMS (src/game/math.hpp):
//    - Uses Transform::Obj (0x10), Transform::MatrixPtr (0x38), Matrices (0x18), Indices (0x20).
// ==============================================================================================

namespace Offsets {

// ----------------------------------------------------------------------------------------------
// 1. BASE CHAIN RESOLUTION: libil2cpp.so -> GameFacade -> BaseGame -> Match (Free Fire Max ARM64)
// ----------------------------------------------------------------------------------------------
    namespace Base {
        constexpr uintptr_t GameFacade           = 0xAD68E18; // Free Fire Max Il2Cpp GameFacade cell
        constexpr uintptr_t GameFacade_Alt       = 0xAD6DC18; // Free Fire Max alt candidate (STORM_MODS)
        constexpr uintptr_t GameFacade_Global    = 0xA72E4E0; // Free Fire Global 1.132.x primary
        constexpr uintptr_t GameFacade_P2        = 0xB8;      // Static fields pointer displacement (0xB8)
        constexpr uintptr_t BaseGame_Match       = 0x90;      // Current Match instance pointer displacement
        constexpr uintptr_t BaseGame_Timer       = 0x20;      // Game timer instance
        constexpr uintptr_t Timer_Delta          = 0x2C;      // Delta time float
        constexpr uintptr_t MatchIsRunning       = 0xCC;      // Boolean: active match flag
        constexpr uintptr_t Match_PlayerDict     = 0xC0;      // System.Collections.Generic.Dictionary<byte, Player>
        constexpr uintptr_t Match_LocalPlayer    = 0xD8;      // Local Player entity pointer
    }

// ----------------------------------------------------------------------------------------------
// 2. IL2CPP MONO DICTIONARY INTERNAL DISPLACEMENTS
// ----------------------------------------------------------------------------------------------
    namespace Dict {
        constexpr uintptr_t Entries              = 0x18;      // Pointer to managed entries array
        constexpr uintptr_t Count                = 0x20;      // Active element count
        constexpr uintptr_t ArrayHeader          = 0x20;      // Skip Il2Cpp managed array object header
        constexpr uintptr_t EntryStride          = 0x18;      // Size of each RawEntry struct (24 bytes)
        constexpr uintptr_t EntryValue           = 0x10;      // Offset to entity pointer within RawEntry
    }

// ----------------------------------------------------------------------------------------------
// 3. MEMORY SAFETY THRESHOLDS
// ----------------------------------------------------------------------------------------------
    namespace Memory {
        constexpr uintptr_t MinValidAddress      = 0x10000;   // 64KB userland unmapped null-page boundary
    }

// ----------------------------------------------------------------------------------------------
// 4. VISUAL & ESP GEOMETRIC PROPORTIONS
// ----------------------------------------------------------------------------------------------
    namespace ESP {
        constexpr float BoxWidthRatio            = 0.48f;     // Standard humanoid width-to-height ratio (48%)
        constexpr float BoxPadTopPercent         = 0.12f;     // 12% helmet/head padding above head node
        constexpr float BoxPadBotPercent         = 0.05f;     // 5% boots padding below toe node
    }

// ----------------------------------------------------------------------------------------------
// 5. PLAYER REPLICATION ENTITY FIELDS
// ----------------------------------------------------------------------------------------------
    namespace Player {
        constexpr uintptr_t Rotation             = 0x614;     // Entity rotation Vector3
        constexpr uintptr_t HeadTF               = 0x6A8;     // Free Fire Max Head ITransformNode
        constexpr uintptr_t HeadTF_Alt           = 0x6A0;     // Fallback
        constexpr uintptr_t HipTF                = 0x6B0;     // Free Fire Max Hip / Waist ITransformNode
        constexpr uintptr_t SpineTF              = 0x6B8;     // Spine ITransformNode
        constexpr uintptr_t RootTF               = 0x6C8;     // Root / Pelvis ITransformNode
        constexpr uintptr_t FootTF               = 0x6F0;     // Free Fire Max Left Toe ITransformNode
        constexpr uintptr_t FootTF_Alt           = 0x6F8;     // Right Toe TF
        constexpr uintptr_t HeadCollider         = 0x740;     // Head Collider pointer
        constexpr uintptr_t HeadCollider_Alt     = 0x738;     // Auxiliary Head Collider pointer
        constexpr uintptr_t LockedAimingCollider = 0x80;      // Primary collider (READ-ONLY)
        constexpr uintptr_t LockedAimingCollider_Alt = 0x78;  // Auxiliary collider (READ-ONLY)
        constexpr uintptr_t AimingTarget         = 0xE68;     // LastAimingTargetFromWeapon
        constexpr uintptr_t AimingAdjusted       = 0xE60;     // LastAimingInfoFromWeaponAdjusted
        constexpr uintptr_t Camera               = 0x698;     // Free Fire Max FollowCamera pointer
        constexpr uintptr_t Camera_Alt           = 0x690;     // FollowCamera auxiliary
        constexpr uintptr_t FollowCamera_Sub     = 0x698;     // FollowCamera auxiliary pointer
        constexpr uintptr_t IsFiring             = 0x850;     // Primary isFiring integer flag
        constexpr uintptr_t IsFiring_Alt         = 0x848;     // Auxiliary isFiring integer flag
        constexpr uintptr_t HP                   = 0x70;      // m_PRIDataPool pointer
        constexpr uintptr_t Armor                = 0x74;      // Armor points
        constexpr uintptr_t HP_MaxEntry          = 0x28;      // Index 1 (Max HP) in ReplicationData pool
        constexpr uintptr_t HP_ArmorEntry        = 0x40;      // Index 4 (Armor Points) in pool
        constexpr uintptr_t IsDead               = 0x7C;      // Boolean: isDead flag
        constexpr uintptr_t Avatar               = 0x778;     // AvatarManager pointer
        constexpr uintptr_t Avatar_Alt           = 0x770;     // AvatarManager auxiliary pointer
        constexpr uintptr_t IsBot                = 0x4A8;     // Boolean: isClientBot flag
        constexpr uintptr_t IsBot_Alt            = 0x4A0;     // Auxiliary bot flag
        constexpr uintptr_t Name                 = 0x498;     // MonoString* OriginalNickName
        constexpr uintptr_t Speed                = 0x560;     // Speed single
        constexpr uintptr_t AimInfo              = 0xE58;     // Primary LastAimingInfoFromWeapon
        constexpr uintptr_t AimInfo_Alt          = 0xE50;     // Auxiliary AimingInfo
        constexpr uintptr_t Velocity             = 0xF0;      // Velocity Vector3
        constexpr uintptr_t IsGrounded           = 0x11C;     // Boolean: isGrounded flag
        constexpr uintptr_t FOVOffset            = 0x84;      // Scope FOV offset float inside FollowCamera
        constexpr uintptr_t Weapon               = 0x608;     // Free Fire Max WeaponOnHand pointer
        constexpr uintptr_t Weapon_Alt           = 0x600;     // Free Fire Global WeaponOnHand pointer
        constexpr uintptr_t Attributes           = 0x770;     // PlayerAttributes Max
        constexpr uintptr_t Attributes_Max       = 0x778;

        // Physical Constants
        constexpr int       StandardBaseHP       = 200;       // Standard base HP
        constexpr float     ModelHeight          = 1.75f;     // Standard human model height (meters)
    }

// ----------------------------------------------------------------------------------------------
// 6. MONO & UNITY INTERNALS
// ----------------------------------------------------------------------------------------------
    namespace Mono {
        constexpr uintptr_t String_Length        = 0x10;
        constexpr uintptr_t String_Chars         = 0x14;
    }

    namespace Unity {
        constexpr uintptr_t StaticFields_Instance = 0x8;
        constexpr uintptr_t Component_Transform   = 0x18;
        constexpr uintptr_t Transform_Internal    = 0x20;
    }

// ----------------------------------------------------------------------------------------------
// 7. TRANSFORM HIERARCHY TRAVERSAL (Unity TransformAccessReadOnly Decomposition)
// ----------------------------------------------------------------------------------------------
    namespace Transform {
        constexpr uintptr_t Node                 = 0x10;      // Node to Transform component
        constexpr uintptr_t Obj                  = 0x10;      // Transform internal pointer
        constexpr uintptr_t MatrixPtr            = 0x38;      // Matrix pointer
        constexpr uintptr_t Matrices             = 0x18;      // Matrices array buffer pointer
        constexpr uintptr_t Indices              = 0x20;      // Indices array buffer pointer
        constexpr uintptr_t Index                = 0x40;      // Bone index slot
        constexpr uintptr_t MatrixStride         = 0x30;      // Matrix element stride (48 bytes)
        constexpr uintptr_t IndexStride          = 0x4;       // Index element stride (4 bytes)
        constexpr uintptr_t Position             = 0x90;      // Direct position offset
    }

// ----------------------------------------------------------------------------------------------
// 8. WEAPON & COMBAT DATA
// ----------------------------------------------------------------------------------------------
    namespace Weapon {
        constexpr uintptr_t Data                 = 0x18;      // Weapon_Data pointer displacement
        constexpr uintptr_t Weapon_ID            = 0x64;      // Weapon identifier int
        constexpr uintptr_t Recoil               = 0x80;      // Weapon Recoil float (0.0f = zero recoil)
        constexpr uintptr_t IsSight              = 0x8A0;     // Active Weapon Sight pointer (0x8A0 STORM_MODS)
    }

    namespace Speed {
        constexpr uintptr_t RunSpeedUpScale      = 0x2C0;     // RunSpeedUpScale float on PlayerAttributes (0x2C0 verified in ninmods_real_offsets.h)
    }

// ----------------------------------------------------------------------------------------------
// 9. AVATAR & TEAMMATE FILTERING (UMAData Flags)
// ----------------------------------------------------------------------------------------------
    namespace Avatar {
        constexpr uintptr_t Uma                  = 0x138;     // AvatarManager -> UMA pointer
        constexpr uintptr_t Uma_Visible          = 0x101;     // Visibility boolean
        constexpr uintptr_t Uma_Data             = 0x28;      // UMA -> UMAData pointer
        constexpr uintptr_t UmaData_LocalPlayer  = 0x80;      // Flag: isLocalPlayer
        constexpr uintptr_t UmaData_Teammate     = 0x81;      // Flag: isTeammate (filter out allies)
    }

// ----------------------------------------------------------------------------------------------
// 10. CAMERA & VIEW MATRIX
// ----------------------------------------------------------------------------------------------
    namespace Camera {
        constexpr uintptr_t Follow               = 0x28;      // Camera follow component
        constexpr uintptr_t IntPtr               = 0x10;      // Camera native IntPtr
        constexpr uintptr_t Matrix               = 0x100;     // 4x4 ViewMatrix float buffer
        constexpr uintptr_t Follow_Alt           = 0x30;
        constexpr uintptr_t IntPtr_Alt           = 0x10;
    }

// ----------------------------------------------------------------------------------------------
// 11. WEAPON AIM / RAYCAST BUFFER (Silent Aim Trajectory Injection)
// ----------------------------------------------------------------------------------------------
    namespace Aim {
        constexpr uintptr_t HitObject            = 0x18;      // Target GameObject
        constexpr uintptr_t HitCollider          = 0x20;      // Target Collider
        constexpr uintptr_t HitPoint             = 0x28;      // Raycast hit point Vector3
        constexpr uintptr_t HitNormal            = 0x34;      // Surface normal Vector3
        constexpr uintptr_t Direction            = 0x40;      // Bullet trajectory raycast direction (0x40)
        constexpr uintptr_t StartPosition        = 0x4C;      // Bullet muzzle start position (0x4C)
        constexpr uintptr_t Damage               = 0x58;      // Damage integer
        constexpr uintptr_t Distance             = 0x5C;      // Distance float
        constexpr uintptr_t ActorLayer           = 0x60;      // Layer mask
        constexpr uintptr_t HitGroup             = 0x64;      // Hit hitbox group (1 = Head, 2 = Body)
        constexpr uintptr_t HitPhysicMaterial    = 0x68;
        constexpr uintptr_t IgnoreHappens        = 0x70;
        constexpr uintptr_t ViewBlocked          = 0x71;
        constexpr uintptr_t OrigStartPosition    = 0x74;
        constexpr uintptr_t SpecialHitType       = 0x80;

        constexpr int32_t HitGroup_Default       = 0;
        constexpr int32_t HitGroup_Head          = 1;
        constexpr int32_t HitGroup_Body          = 2;
    }

// ----------------------------------------------------------------------------------------------
// 12. SKELETON BONES (ITransformNode Pointers - Free Fire Max ARM64)
// ----------------------------------------------------------------------------------------------
    namespace Bone {
        constexpr uintptr_t Head                 = 0x6A8;     // Free Fire Max Head node
        constexpr uintptr_t Head_Alt             = 0x6A0;     // Fallback
        constexpr uintptr_t Hip                  = 0x6B0;     // Free Fire Max Hip / Waist node
        constexpr uintptr_t Breast               = 0x6B8;     // Upper chest / Spine
        constexpr uintptr_t Spine                = 0x6B8;     // Spine node
        constexpr uintptr_t Root                 = 0x6C8;     // Root / Pelvis node
        constexpr uintptr_t Root_Alt             = 0x6D0;     // Root fallback
        constexpr uintptr_t LeftAnkle            = 0x6D8;     // Left ankle
        constexpr uintptr_t RightAnkle           = 0x6E0;     // Right ankle
        constexpr uintptr_t LeftToe              = 0x6F0;     // Free Fire Max Left toe sole node
        constexpr uintptr_t LeftToe_Alt          = 0x6F8;     // RightToe fallback
        constexpr uintptr_t RightToe             = 0x6F8;     // Right toe sole node
        constexpr uintptr_t LeftShoulder         = 0x710;     // Left shoulder
        constexpr uintptr_t RightShoulder        = 0x718;     // Right shoulder
        constexpr uintptr_t RightHand            = 0x720;     // Right hand
        constexpr uintptr_t LeftHand             = 0x728;     // Left hand
        constexpr uintptr_t RightForeArm         = 0x730;     // Right forearm
        constexpr uintptr_t LeftForeArm          = 0x738;     // Left forearm
        constexpr float     SkullElevation       = 0.12f;     // Head center elevation (meters)
    }

// ----------------------------------------------------------------------------------------------
// 13. FLAT GLOBAL ALIASES (Direct Namespace Access)
// ----------------------------------------------------------------------------------------------
    constexpr uintptr_t GameFacade               = Base::GameFacade;
    constexpr uintptr_t GameFacade_P2            = Base::GameFacade_P2;
    constexpr uintptr_t BaseGame_Match           = Base::BaseGame_Match;
    constexpr uintptr_t BaseGame_Timer           = Base::BaseGame_Timer;
    constexpr uintptr_t Timer_Delta              = Base::Timer_Delta;
    constexpr uintptr_t MatchIsRunning           = Base::MatchIsRunning;
    constexpr uintptr_t Match_PlayerDict         = Base::Match_PlayerDict;
    constexpr uintptr_t Match_LocalPlayer        = Base::Match_LocalPlayer;

    constexpr uintptr_t Dict_Entries             = Dict::Entries;
    constexpr uintptr_t Dict_Count               = Dict::Count;
    constexpr uintptr_t Dict_ArrayHeader         = Dict::ArrayHeader;
    constexpr uintptr_t Dict_EntryStride         = Dict::EntryStride;
    constexpr uintptr_t Dict_EntryValue          = Dict::EntryValue;

    constexpr uintptr_t Player_Rotation          = Player::Rotation;
    constexpr uintptr_t Player_HeadTF            = Player::HeadTF;
    constexpr uintptr_t Player_FootTF            = Player::FootTF;
    constexpr uintptr_t Player_Camera            = Player::Camera;
    constexpr uintptr_t Player_IsFiring          = Player::IsFiring;
    constexpr uintptr_t Player_HP                = Player::HP;
    constexpr uintptr_t Player_Armor             = Player::Armor;
    constexpr uintptr_t Player_IsDead            = Player::IsDead;
    constexpr uintptr_t Player_Avatar            = Player::Avatar;
    constexpr uintptr_t Player_IsBot             = Player::IsBot;
    constexpr uintptr_t Player_Name              = Player::Name;
    constexpr uintptr_t Player_Speed             = Player::Speed;
    constexpr uintptr_t Player_AimInfo           = Player::AimInfo;
    constexpr uintptr_t Player_AimInfo_Alt       = Player::AimInfo_Alt;
    constexpr uintptr_t Player_Velocity          = Player::Velocity;
    constexpr uintptr_t Player_IsGrounded        = Player::IsGrounded;
    constexpr uintptr_t Player_FOVOffset         = Player::FOVOffset;
    constexpr uintptr_t Player_Weapon            = Player::Weapon;
    constexpr uintptr_t Player_Attributes        = Player::Attributes;
    constexpr uintptr_t Player_Attributes_Max    = Player::Attributes_Max;

    constexpr uintptr_t Transform_Node           = Transform::Node;
    constexpr uintptr_t Transform_Obj            = Transform::Obj;
    constexpr uintptr_t Transform_MatrixPtr      = Transform::MatrixPtr;
    constexpr uintptr_t Transform_Matrices       = Transform::Matrices;
    constexpr uintptr_t Transform_Indices        = Transform::Indices;
    constexpr uintptr_t Transform_Index          = Transform::Index;
    constexpr uintptr_t Transform_MatrixStride   = Transform::MatrixStride;
    constexpr uintptr_t Transform_IndexStride    = Transform::IndexStride;
    constexpr uintptr_t Transform_Position       = Transform::Position;

    constexpr uintptr_t Avatar_Uma               = Avatar::Uma;
    constexpr uintptr_t Uma_Visible              = Avatar::Uma_Visible;
    constexpr uintptr_t Uma_Data                 = Avatar::Uma_Data;

    constexpr uintptr_t Camera_Follow            = Camera::Follow;
    constexpr uintptr_t Camera_IntPtr            = Camera::IntPtr;
    constexpr uintptr_t Camera_Matrix            = Camera::Matrix;

    constexpr uintptr_t HP_List                  = 0x10;
    constexpr uintptr_t HP_Entry                 = 0x20;
    constexpr uintptr_t HP_Value                 = 0x18;

    constexpr uintptr_t Weapon_Data              = Weapon::Data;
    constexpr uintptr_t Weapon_Recoil            = Weapon::Recoil;

    constexpr uintptr_t AimInfo_HitCollider      = Aim::HitCollider;
    constexpr uintptr_t AimInfo_HitPoint         = Aim::HitPoint;
    constexpr uintptr_t AimInfo_HitNormal        = Aim::HitNormal;
    constexpr uintptr_t AimInfo_Direction        = Aim::Direction;
    constexpr uintptr_t AimInfo_StartPosition    = Aim::StartPosition;
    constexpr uintptr_t AimInfo_Distance         = Aim::Distance;
    constexpr uintptr_t AimInfo_HitGroup         = Aim::HitGroup;
    constexpr int32_t   HitGroup_Head            = Aim::HitGroup_Head;
    constexpr uintptr_t AimInfo_ViewBlocked      = Aim::ViewBlocked;
    constexpr uintptr_t AimInfo_OrigStartPosition = Aim::OrigStartPosition;
    constexpr uintptr_t AimInfo_SpecialHitType   = Aim::SpecialHitType;

    constexpr uintptr_t Bone_Head                = Bone::Head;
    constexpr uintptr_t Bone_Head_Alt            = Bone::Head_Alt;
    constexpr uintptr_t Bone_Hip                 = Bone::Hip;
    constexpr uintptr_t Bone_Breast              = Bone::Breast;
    constexpr uintptr_t Bone_Chest               = Bone::Spine;
    constexpr uintptr_t Bone_Spine               = Bone::Spine;
    constexpr uintptr_t Bone_Root                = Bone::Root;
    constexpr uintptr_t Bone_Root_Alt            = Bone::Root_Alt;
    constexpr uintptr_t Bone_LeftAnkle           = Bone::LeftAnkle;
    constexpr uintptr_t Bone_RightAnkle          = Bone::RightAnkle;
    constexpr uintptr_t Bone_LeftToe             = Bone::LeftToe;
    constexpr uintptr_t Bone_LeftToe_Alt         = Bone::LeftToe_Alt;
    constexpr uintptr_t Bone_RightToe            = Bone::RightToe;
    constexpr uintptr_t Bone_LeftShoulder        = Bone::LeftShoulder;
    constexpr uintptr_t Bone_RightShoulder       = Bone::RightShoulder;
    constexpr uintptr_t Bone_RightHand           = Bone::RightHand;
    constexpr uintptr_t Bone_LeftHand            = Bone::LeftHand;
    constexpr uintptr_t Bone_RightForeArm        = Bone::RightForeArm;
    constexpr uintptr_t Bone_LeftForeArm         = Bone::LeftForeArm;
    constexpr float     Bone_SkullElevation      = Bone::SkullElevation;
    constexpr int       Player_StandardBaseHP    = Player::StandardBaseHP;
    constexpr float     Player_ModelHeight       = Player::ModelHeight;
    constexpr uintptr_t Player_HeadCollider      = Player::HeadCollider;
    constexpr uintptr_t Player_HeadCollider_Alt  = Player::HeadCollider_Alt;
    constexpr uintptr_t Player_LockedAimingCollider = Player::LockedAimingCollider;
    constexpr uintptr_t Player_LockedAimingCollider_Alt = Player::LockedAimingCollider_Alt;
    constexpr uintptr_t Player_AimingTarget      = Player::AimingTarget;
    constexpr uintptr_t Player_AimingAdjusted    = Player::AimingAdjusted;

    constexpr uintptr_t StaticFields_Instance    = Unity::StaticFields_Instance;
    constexpr uintptr_t MinValidAddress          = Memory::MinValidAddress;

    constexpr float     ESP_BoxWidthRatio        = ESP::BoxWidthRatio;
    constexpr float     ESP_BoxPadTopPercent     = ESP::BoxPadTopPercent;
    constexpr float     ESP_BoxPadBotPercent     = ESP::BoxPadBotPercent;

    // Movement & Speed Attributes
    constexpr uintptr_t PlayerAttributes         = Player::Attributes;     // 0x768 (Global Primary)
    constexpr uintptr_t PlayerAttributes_Max     = Player::Attributes_Max; // 0x770 (Max Fallback)
    constexpr uintptr_t RunSpeedUpScale          = Speed::RunSpeedUpScale; // 0x2C0 on PlayerAttributes (ninmods_real_offsets.h)

} // namespace Offsets
