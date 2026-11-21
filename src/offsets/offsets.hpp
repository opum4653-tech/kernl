#pragma once

#include "../../Offsetsmax.h"
#include <cstdint>

// ==============================================================================================
// FILE: src/offsets/offsets.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// SOURCE OF TRUTH: Offsetsmax.h (Free Fire Max ARM64)
// PURPOSE: Central verified offset registry. Directly powered by Offsetsmax.h.
// ==============================================================================================

namespace Offsets {

// ----------------------------------------------------------------------------------------------
// 1. BASE CHAIN RESOLUTION (Free Fire Max ARM64 - Powered by Offsetsmax.h)
// ----------------------------------------------------------------------------------------------
    namespace Base {
        constexpr uintptr_t GameFacade           = Offsets::GameFacade;
        constexpr uintptr_t GameFacade_TypeInfo  = Offsets::GameFacade_TypeInfo;
        constexpr uintptr_t GameFacade_Alt       = 0xAD6DC18;
        // GameFacade_Global (0xA72E4E0) removed — MAX-ONLY build, Global unsupported.
        constexpr uintptr_t GameFacade_P2        = Offsets::Staticfield;
        constexpr uintptr_t Staticfield          = Offsets::Staticfield;
        constexpr uintptr_t StaticGameFacade     = Offsets::StaticGameFacade;
        constexpr uintptr_t BaseGame_HasInited   = Offsets::BaseGame_HasInited;
        constexpr uintptr_t BaseGame_MatchEnd    = Offsets::BaseGame_MatchEnd;
        constexpr uintptr_t BaseGame_Match       = Offsets::CurrentMatch;
        constexpr uintptr_t CurrentMatch         = Offsets::CurrentMatch;
        constexpr uintptr_t BaseGame_Timer       = 0x20;
        constexpr uintptr_t Timer_Delta          = 0x2C;
        constexpr uintptr_t MatchIsRunning       = Offsets::MatchIsRunning;
        constexpr uintptr_t Match_PlayerDict     = Offsets::Dictionary;
        constexpr uintptr_t Dictionary           = Offsets::Dictionary;
        constexpr uintptr_t DictionaryEntities   = Offsets::DictionaryEntities;
        constexpr uintptr_t LDictionaryEntities  = Offsets::LDictionaryEntities;
        constexpr uintptr_t Match_LocalPlayer    = Offsets::localPlayer;
        constexpr uintptr_t LocalPlayer          = Offsets::LocalPlayer;
        constexpr uintptr_t CurrentObserve       = Offsets::CurrentObserve;
        constexpr uintptr_t ObserverPlayer       = Offsets::ObserverPlayer;
    }

// ----------------------------------------------------------------------------------------------
// 2. IL2CPP DICTIONARY INTERNAL DISPLACEMENTS (Offsetsmax.h entryAddr 0x18 / enemyList 0x10)
// ----------------------------------------------------------------------------------------------
    namespace Dict {
        constexpr uintptr_t Entries              = Offsets::entryAddr; // 0x18
        constexpr uintptr_t Count                = 0x20;
        constexpr uintptr_t ArrayHeader          = 0x20;
        constexpr uintptr_t EntryStride          = 0x18;
        constexpr uintptr_t EntryValue           = 0x10;
    }

// ----------------------------------------------------------------------------------------------
// 3. MEMORY SAFETY THRESHOLDS
// ----------------------------------------------------------------------------------------------
    namespace Memory {
        constexpr uintptr_t MinValidAddress      = 0x10000;
    }

// ----------------------------------------------------------------------------------------------
// 4. VISUAL & ESP GEOMETRIC PROPORTIONS
// ----------------------------------------------------------------------------------------------
    namespace ESP {
        constexpr float BoxWidthRatio            = 0.48f;
        constexpr float BoxPadTopPercent         = 0.12f;
        constexpr float BoxPadBotPercent         = 0.05f;
    }

// ----------------------------------------------------------------------------------------------
// 5. PLAYER REPLICATION ENTITY FIELDS (Powered by Offsetsmax.h)
// ----------------------------------------------------------------------------------------------
    namespace Player {
        constexpr uintptr_t Rotation             = 0x614;
        constexpr uintptr_t AimRotation          = Offsets::AimRotation; // 0x61C
        constexpr uintptr_t CallSetAimRotationCount = Offsets::CallSetAimRotationCount; // 0x79C
        constexpr uintptr_t HeadTF               = Offsets::Head;  // 0x6A8
        constexpr uintptr_t HeadTF_Alt           = Offsets::Head;  // 0x6A8
        constexpr uintptr_t HipTF                = Offsets::Hip;   // 0x6B0
        constexpr uintptr_t SpineTF              = Offsets::Spine; // 0x6B8
        constexpr uintptr_t RootTF               = Offsets::Root;  // 0x6D0
        constexpr uintptr_t RootTF_Alt           = Offsets::Root;  // 0x6D0
        constexpr uintptr_t FootTF               = Offsets::lToe;  // 0x6F0
        constexpr uintptr_t FootTF_Alt           = Offsets::rToe;  // 0x6F8
        constexpr uintptr_t HeadCollider         = Offsets::HedColider; // 0x740
        constexpr uintptr_t HeadCollider_Alt     = Offsets::HedColider;
        constexpr uintptr_t LockedAimingCollider = Offsets::Colider_Field; // 0x80
        constexpr uintptr_t LockedAimingCollider_Alt = Offsets::Colider_Field;
        constexpr uintptr_t AimingTarget         = 0xE68;
        constexpr uintptr_t AimingAdjusted       = 0xE60;
        constexpr uintptr_t Camera               = Offsets::FollowCamera; // 0x698
        constexpr uintptr_t Camera_Alt           = Offsets::FollowCamera;
        constexpr uintptr_t FollowCamera_Sub     = Offsets::FollowCamera;
        constexpr uintptr_t IsFiring             = Offsets::isFiring; // 0x850
        constexpr uintptr_t IsFiring_Alt         = 0x848; // Alternative weapon firing flag
        constexpr uintptr_t HP                   = Offsets::Player_Data; // 0x70
        constexpr uintptr_t Armor                = 0x74;
        constexpr uintptr_t HP_MaxEntry          = 0x28;
        constexpr uintptr_t HP_ArmorEntry        = 0x40;
        constexpr uintptr_t IsDead               = Offsets::Player_IsDead; // 0x7C
        constexpr uintptr_t Avatar               = Offsets::AvatarManager; // 0x778
        constexpr uintptr_t Avatar_Alt           = Offsets::AvatarManager;
        constexpr uintptr_t IsBot                = Offsets::IsBot; // 0x4A8
        constexpr uintptr_t IsBot_Alt            = Offsets::IsBot;
        constexpr uintptr_t Name                 = Offsets::Player_Name; // 0x498
        constexpr uintptr_t Speed                = 0x560;
        constexpr uintptr_t AimInfo              = Offsets::aimInfo; // 0xE58
        constexpr uintptr_t AimInfo_Alt          = Offsets::aimInfo;
        constexpr uintptr_t Velocity             = 0xF0;
        constexpr uintptr_t IsGrounded           = 0x11C;
        constexpr uintptr_t FOVOffset            = 0x84;
        constexpr uintptr_t Weapon               = Offsets::Weapon; // 0x608
        constexpr uintptr_t Weapon_Alt           = Offsets::Weapon;
        constexpr uintptr_t WeaponOnHand         = Offsets::Player_WeaponOnHand; // 0x17E8
        constexpr uintptr_t InventoryManager     = Offsets::InventoryManager; // 0x748
        constexpr uintptr_t Attributes           = Offsets::PlayerAttributes; // 0x770
        constexpr uintptr_t Attributes_Max       = Offsets::PlayerAttributes;
        constexpr uintptr_t PhyXdata             = Offsets::PhyXdata; // 0x1D60
        constexpr uintptr_t maybeDead            = Offsets::maybeDead; // 0x20
        constexpr uintptr_t isKnocked            = Offsets::isKnocked; // 0x10
        constexpr uintptr_t GhostHack            = Offsets::GhostHack; // 0x814
        constexpr uintptr_t ShootNoReload        = Offsets::ShootNoReload; // 0x111

        constexpr int       StandardBaseHP       = 200;
        constexpr float     ModelHeight          = 1.75f;
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
// 7. TRANSFORM HIERARCHY TRAVERSAL (Offsetsmax.h Transform)
// ----------------------------------------------------------------------------------------------
    namespace Transform {
        constexpr uintptr_t Node                 = Offsets::transformValue;    // 0x10
        constexpr uintptr_t Obj                  = Offsets::transformObjValue; // 0x10
        constexpr uintptr_t MatrixPtr            = Offsets::matrixValue;       // 0x38
        constexpr uintptr_t Matrices             = Offsets::matrixListValue;   // 0x18
        constexpr uintptr_t Indices              = Offsets::matrixIndicesValue;// 0x20
        constexpr uintptr_t Index                = Offsets::indexValue;        // 0x40
        constexpr uintptr_t MatrixStride         = Offsets::resultValue;       // 0x30
        constexpr uintptr_t IndexStride          = 0x4;
        constexpr uintptr_t Position             = 0x90;
    }


// ----------------------------------------------------------------------------------------------
// 11. WEAPON AIM / RAYCAST BUFFER (Offsetsmax.h)
// ----------------------------------------------------------------------------------------------
    namespace Aim {
        constexpr uintptr_t HitObject            = 0x18;
        constexpr uintptr_t HitCollider          = 0x20;
        constexpr uintptr_t HitPoint             = 0x28;
        constexpr uintptr_t HitNormal            = 0x34;
        constexpr uintptr_t Direction            = Offsets::raycast; // 0x40
        constexpr uintptr_t StartPosition        = Offsets::startPos; // 0x4C
        constexpr uintptr_t Damage               = 0x58;
        constexpr uintptr_t Distance             = 0x5C;
        constexpr uintptr_t ActorLayer           = 0x60;
        constexpr uintptr_t HitGroup             = 0x64;
        constexpr uintptr_t HitPhysicMaterial    = 0x68;
        constexpr uintptr_t IgnoreHappens        = 0x70;
        constexpr uintptr_t ViewBlocked          = 0x71;
        constexpr uintptr_t OrigStartPosition    = 0x74;
        constexpr uintptr_t SpecialHitType       = 0x80;
        constexpr uintptr_t AimRotation          = Offsets::AimRotation; // 0x61C
        constexpr uintptr_t Colider_Field        = Offsets::Colider_Field; // 0x80
        constexpr uintptr_t HedColider           = Offsets::HedColider; // 0x740
        constexpr uintptr_t aimInfo              = Offsets::aimInfo; // 0xE58
        constexpr uintptr_t isFiring             = Offsets::isFiring; // 0x850

        constexpr int32_t HitGroup_Default       = 0;
        constexpr int32_t HitGroup_Head          = 1;
        constexpr int32_t HitGroup_Body          = 2;
    }

// ----------------------------------------------------------------------------------------------
// 12. SKELETON BONES (Offsetsmax.h)
// ----------------------------------------------------------------------------------------------
    namespace Bone {
        constexpr uintptr_t Head                 = Offsets::Head; // 0x6A8
        constexpr uintptr_t Head_Alt             = Offsets::Head;
        constexpr uintptr_t Hip                  = Offsets::Hip; // 0x6B0
        constexpr uintptr_t Breast               = Offsets::Spine; // 0x6B8
        constexpr uintptr_t Spine                = Offsets::Spine; // 0x6B8
        constexpr uintptr_t Root                 = Offsets::Root; // 0x6D0
        constexpr uintptr_t Root_Alt             = Offsets::Root;
        constexpr uintptr_t LeftAnkle            = Offsets::lAnkle; // 0x6E0
        constexpr uintptr_t RightAnkle           = Offsets::rAnkle; // 0x6E8
        constexpr uintptr_t LeftToe              = Offsets::lToe; // 0x6F0
        constexpr uintptr_t LeftToe_Alt          = Offsets::rToe;
        constexpr uintptr_t RightToe             = Offsets::rToe; // 0x6F8
        constexpr uintptr_t LeftShoulder         = Offsets::lShoulder; // 0x710
        constexpr uintptr_t RightShoulder        = Offsets::rShoulder; // 0x718
        constexpr uintptr_t RightHand            = Offsets::rHand; // 0x720
        constexpr uintptr_t LeftHand             = Offsets::lHand; // 0x728
        constexpr uintptr_t RightForeArm         = Offsets::rElbow; // 0x730
        constexpr uintptr_t LeftForeArm          = Offsets::lElbow; // 0x738
        constexpr float     SkullElevation       = 0.12f;
    }

// ----------------------------------------------------------------------------------------------
// 13. FLAT GLOBAL ALIASES (Direct Namespace Access)
// ----------------------------------------------------------------------------------------------
    constexpr uintptr_t BaseGame_Match           = Base::BaseGame_Match;
    constexpr uintptr_t BaseGame_Timer           = Base::BaseGame_Timer;
    constexpr uintptr_t Timer_Delta              = Base::Timer_Delta;
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
    constexpr uintptr_t Player_HP                = Player::HP;
    constexpr uintptr_t Player_Armor             = Player::Armor;
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

    constexpr uintptr_t Avatar_Uma               = Offsets::Avatar;
    constexpr uintptr_t Uma_Visible              = Offsets::Avatar_IsVisible;
    constexpr uintptr_t Uma_Data                 = Offsets::Avatar_Data;

    constexpr uintptr_t Camera_Follow            = Offsets::Camera;
    constexpr uintptr_t Camera_IntPtr            = Offsets::IntPtrCam;
    constexpr uintptr_t Camera_Matrix            = Offsets::Matrix;

    constexpr uintptr_t HP_List                  = Offsets::GetCur; // 0x10
    constexpr uintptr_t HP_Entry                 = Offsets::HP;     // 0x20
    constexpr uintptr_t HP_Value                 = Offsets::Health; // 0x18

    constexpr uintptr_t Weapon_Recoil            = Offsets::NoRecoil;

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

} // namespace Offsets
