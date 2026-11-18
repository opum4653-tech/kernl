#pragma once

#include <cstdint>

// CENTRALIZED OFFSETS MODULE — STRICT CORE ARCHITECTURE
// Target game build: OB55 / 1.132.1

namespace Offsets {

// Base Chain (GameFacade → BaseGame → Match)
    namespace Base {
        constexpr uintptr_t GameFacade           = 0xA72E4E0;
        constexpr uintptr_t GameFacade_P2        = 0xB8;
        constexpr uintptr_t BaseGame_Match       = 0x90;
        constexpr uintptr_t BaseGame_Timer       = 0x20;
        constexpr uintptr_t Timer_Delta          = 0x2C;
        constexpr uintptr_t MatchIsRunning       = 0xCC;
        constexpr uintptr_t Match_PlayerDict     = 0xC0;
        constexpr uintptr_t Match_LocalPlayer    = 0xD8;
    }

// Il2Cpp Mono Dictionary Traversal
    namespace Dict {
        constexpr uintptr_t Entries              = 0x18;
        constexpr uintptr_t Count                = 0x20;
        constexpr uintptr_t ArrayHeader          = 0x20;
        constexpr uintptr_t EntryStride          = 0x18;
        constexpr uintptr_t EntryValue           = 0x10;
    }

// Player Entity Fields
    namespace Player {
        constexpr uintptr_t Rotation             = 0x614;   // Vector3
        constexpr uintptr_t HeadTF               = 0x6A0;   // Head ITransformNode (0x6A0 GBKFHDFCPMD)
        constexpr uintptr_t HeadTF_Alt           = 0x6A8;   // Auxiliary Head / Hip TF
        constexpr uintptr_t SpineTF              = 0x6B8;   // Spine ITransformNode (0x6B8 CBHHCCNKOND)
        constexpr uintptr_t FootTF               = 0x6E8;   // Foot / Left Toe ITransformNode (0x6E8 GMJLIMFHMAE)
        constexpr uintptr_t FootTF_Alt           = 0x6C8;   // Auxiliary Root TF (0x6C8 IIPBIDIBJDK)
        constexpr uintptr_t HeadCollider         = 0x740;   // Head Collider (0x740 proven in STORM_MODS, pewpew & crack_master)
        constexpr uintptr_t HeadCollider_Alt     = 0x738;   // Auxiliary Head Collider (0x738)
        constexpr uintptr_t LockedAimingCollider = 0x80;    // Primary: Collider on AttackableEntity (0x80)
        constexpr uintptr_t LockedAimingCollider_Alt = 0x78;// Auxiliary: AttackableEntity Current Collider (0x78)
        constexpr uintptr_t AimingTarget         = 0xE68;   // LastAimingTargetFromWeapon (0xE68 in OB55)
        constexpr uintptr_t AimingAdjusted       = 0xE60;   // LastAimingInfoFromWeaponAdjusted (0xE60 in OB55)
        constexpr uintptr_t Camera               = 0x690;   // FollowCamera (0x690)
        constexpr uintptr_t Camera_Alt           = 0x698;   // FollowCamera auxiliary
        constexpr uintptr_t FollowCamera_Sub     = 0x690;   // FollowCamera auxiliary pointer
        constexpr uintptr_t IsFiring             = 0x850;   // Primary: isFiring (0x850)
        constexpr uintptr_t IsFiring_Alt         = 0x848;   // Auxiliary: isFiring (0x848)
        constexpr uintptr_t HP                   = 0x70;    // HP Pool Pointer
        constexpr uintptr_t Armor                = 0x74;    // Int32
        constexpr uintptr_t HP_MaxEntry          = 0x28;    // Index 1 in HP datas pool
        constexpr uintptr_t HP_ArmorEntry        = 0x40;    // Index 4 in HP datas pool
        constexpr uintptr_t IsDead               = 0x7C;    // Boolean (0x7C)
        constexpr uintptr_t Avatar               = 0x770;   // AvatarManager (0x770)
        constexpr uintptr_t IsBot                = 0x4A0;   // Boolean: IsClientBot (0x4A0)
        constexpr uintptr_t Name                 = 0x498;   // MonoString* OriginalNickName (0x498)
        constexpr uintptr_t Speed                = 0x560;   // Single (Speed 0x560)
        constexpr uintptr_t AimInfo              = 0xE58;   // Primary: LastAimingInfoFromWeapon (0xE58)
        constexpr uintptr_t AimInfo_Alt          = 0xE50;   // Auxiliary: AimingInfo (0xE50)
        constexpr uintptr_t Velocity             = 0xF0;    // Vector3
        constexpr uintptr_t IsGrounded           = 0x11C;   // Boolean
        constexpr uintptr_t FOVOffset            = 0x84;    // Single
        constexpr uintptr_t Weapon               = 0x600;   // WeaponOnHand (0x600)
        constexpr uintptr_t Attributes           = 0x768;   // PlayerAttributes (0x768)
        constexpr uintptr_t Attributes_Max       = 0x770;   // PlayerAttributes Max (0x770)
    }

// Mono Internals (String)
    namespace Mono {
        constexpr uintptr_t String_Length        = 0x10;
        constexpr uintptr_t String_Chars         = 0x14;
    }

// Unity Core Internals
    namespace Unity {
        constexpr uintptr_t StaticFields_Instance = 0x8;
        constexpr uintptr_t Component_Transform   = 0x18;
        constexpr uintptr_t Transform_Internal    = 0x20;
    }

// Transform Hierarchy Traversal (Unity Matrix Resolution)
    namespace Transform {
        constexpr uintptr_t Node                 = 0x10;
        constexpr uintptr_t Obj                  = 0x10;
        constexpr uintptr_t MatrixPtr            = 0x38;
        constexpr uintptr_t Position             = 0x90;
    }

// Weapon Data Fields
    namespace Weapon {
        constexpr uintptr_t Data                 = 0x80;
        constexpr uintptr_t Weapon_ID            = 0x64;
    }

// Avatar Visibility
    namespace Avatar {
        constexpr uintptr_t Uma                  = 0x138;
        constexpr uintptr_t Uma_Visible          = 0x101;
        constexpr uintptr_t Uma_Data             = 0x28;
        constexpr uintptr_t UmaData_LocalPlayer  = 0x80;
        constexpr uintptr_t UmaData_Teammate     = 0x81;
    }

// Camera Chain
    namespace Camera {
        constexpr uintptr_t Follow               = 0x28;
        constexpr uintptr_t IntPtr               = 0x10;
        constexpr uintptr_t Matrix               = 0x100;
        constexpr uintptr_t Follow_Alt           = 0x30;
        constexpr uintptr_t IntPtr_Alt           = 0x10;
    }

// Aim Information (HitInfo / Trajectory: CGKJLKPMGDJ in IL2CPP dump)
    namespace Aim {
        constexpr uintptr_t HitObject            = 0x18;    // GameObject JHAINOBIFMA (0x18)
        constexpr uintptr_t HitCollider          = 0x20;    // Collider JINDNOFABKB (0x20)
        constexpr uintptr_t HitPoint             = 0x28;    // Vector3 HPCOJHGFEEG (0x28)
        constexpr uintptr_t HitNormal            = 0x34;    // Vector3 DOJDNHCGFGK (0x34)
        constexpr uintptr_t Direction            = 0x40;    // Vector3 RayDir HLDECMJFKJK (0x40)
        constexpr uintptr_t StartPosition        = 0x4C;    // Vector3 EKPLMDDKOGB (0x4C)
        constexpr uintptr_t Damage               = 0x58;    // Int32 OLGEMBBDBID (0x58)
        constexpr uintptr_t Distance             = 0x5C;    // Single CFIPDNFBKHK (0x5C)
        constexpr uintptr_t ActorLayer           = 0x60;    // Int32 GABPHAKNADH (0x60)
        constexpr uintptr_t HitGroup             = 0x64;    // LKOOALMKJND DKPDBLLJEAA (0x64) (1 = Head, 2 = Body)
        constexpr uintptr_t HitPhysicMaterial    = 0x68;    // PhysicMaterial DIDOKHNBBAD (0x68)
        constexpr uintptr_t IgnoreHappens        = 0x70;    // Boolean EFMBNPNFGHM (0x70)
        constexpr uintptr_t ViewBlocked          = 0x71;    // Boolean CEOKOILBODN (0x71)
        constexpr uintptr_t OrigStartPosition    = 0x74;    // Vector3 ONMKMEGMHOL (0x74)
        constexpr uintptr_t SpecialHitType       = 0x80;    // Int16 CLOLKHNMNEK (0x80)
    }

// Skeleton Bones (OB55 / 1.132.1 verified: GBKFHDFCPMD Head, COBFGNOIPMF Hip, GMJLIMFHMAE Toe)
    namespace Bone {
        constexpr uintptr_t Head                 = 0x6A0;   // Head ITransformNode (0x6A0 GBKFHDFCPMD)
        constexpr uintptr_t Hip                  = 0x6A8;   // Hip / Waist ITransformNode (0x6A8 COBFGNOIPMF)
        constexpr uintptr_t Breast               = 0x6B0;   // Breast / Upper Chest (0x6B0 FFFPCADFFGA)
        constexpr uintptr_t Spine                = 0x6B8;   // Spine ITransformNode (0x6B8 CBHHCCNKOND)
        constexpr uintptr_t Root                 = 0x6C8;   // Root / Pelvis ITransformNode (0x6C8 IIPBIDIBJDK)
        constexpr uintptr_t LeftToe              = 0x6E8;   // Foot / Left Toe ITransformNode (0x6E8 GMJLIMFHMAE)
        constexpr uintptr_t RightToe             = 0x6F0;   // Foot / Right Toe ITransformNode (0x6F0 JMHBBGIKBNO)
    }

// Flat Aliases
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
    constexpr uintptr_t Weapon_Recoil            = 0x18;

    constexpr uintptr_t AimInfo_HitCollider      = Aim::HitCollider;
    constexpr uintptr_t AimInfo_HitPoint         = Aim::HitPoint;
    constexpr uintptr_t AimInfo_HitNormal        = Aim::HitNormal;
    constexpr uintptr_t AimInfo_Direction        = Aim::Direction;
    constexpr uintptr_t AimInfo_StartPosition    = Aim::StartPosition;
    constexpr uintptr_t AimInfo_HitGroup         = Aim::HitGroup;
    constexpr uintptr_t AimInfo_ViewBlocked      = Aim::ViewBlocked;
    constexpr uintptr_t AimInfo_OrigStartPosition = Aim::OrigStartPosition;
    constexpr uintptr_t AimInfo_SpecialHitType   = Aim::SpecialHitType;

    constexpr uintptr_t Bone_Head                = Bone::Head;
    constexpr uintptr_t Bone_Hip                 = Bone::Hip;
    constexpr uintptr_t Bone_Chest               = Bone::Spine;
    constexpr uintptr_t Bone_Spine               = Bone::Spine;
    constexpr uintptr_t Bone_Root                = Bone::Root;
    constexpr uintptr_t Bone_LeftToe             = Bone::LeftToe;
    constexpr uintptr_t Player_HeadCollider      = Player::HeadCollider;
    constexpr uintptr_t Player_HeadCollider_Alt  = Player::HeadCollider_Alt;
    constexpr uintptr_t Player_LockedAimingCollider = Player::LockedAimingCollider;
    constexpr uintptr_t Player_LockedAimingCollider_Alt = Player::LockedAimingCollider_Alt;
    constexpr uintptr_t Player_AimingTarget      = Player::AimingTarget;
    constexpr uintptr_t Player_AimingAdjusted    = Player::AimingAdjusted;

    constexpr uintptr_t StaticFields_Instance    = Unity::StaticFields_Instance;

    // Movement & Speed Attributes
    constexpr uintptr_t PlayerAttributes         = Player::Attributes;     // 0x768 (Global Primary)
    constexpr uintptr_t PlayerAttributes_Max     = Player::Attributes_Max; // 0x770 (Max Fallback)
    constexpr uintptr_t RunSpeedUpScale          = 0x2C0;                  // Single (0x2C0)

} // namespace Offsets
