#pragma once
#include <cstdint>

namespace Offsets {
    constexpr const char* GameVersion   = "OB55 / 1.132.1";
    constexpr const char* TargetPackage = "com.dts.freefireth";

    namespace Base {
        constexpr uintptr_t GameFacade           = 0xA72E4E0;
        constexpr uintptr_t GameFacade_P2        = 0xB8;
        constexpr uintptr_t BaseGame_Match       = 0x90;
        constexpr uintptr_t BaseGame_Timer       = 0x20;
        constexpr uintptr_t Timer_Delta          = 0x2C;
        constexpr uintptr_t MatchIsRunning       = 0xCC;
        constexpr uintptr_t Match_PlayerDict     = 0xC0;
        constexpr uintptr_t Match_LocalPlayer    = 0xD8;
        constexpr uintptr_t Match_VehicleDict    = 0x188;
        constexpr uintptr_t Match_MetaManager    = 0xB8;
    }

    namespace Dict {
        constexpr uintptr_t Entries              = 0x18;
        constexpr uintptr_t Count                = 0x20;
        constexpr uintptr_t ArrayHeader          = 0x20;
        constexpr uintptr_t EntryStride          = 0x18;
        constexpr uintptr_t EntryValue           = 0x10;
    }

    namespace Player {
        constexpr uintptr_t Rotation             = 0x614;
        constexpr uintptr_t HeadTF               = 0x6A0;
        constexpr uintptr_t FootTF               = 0x6E8;
        constexpr uintptr_t Camera               = 0x690;
        constexpr uintptr_t IsFiring             = 0x848;
        constexpr uintptr_t HP                   = 0x70;
        constexpr uintptr_t Armor                = 0x74;
        constexpr uintptr_t IsDead               = 0x7C;
        constexpr uintptr_t Avatar               = 0x770;
        constexpr uintptr_t HeadCollider         = 0x738;
        constexpr uintptr_t IsBot                = 0x4A0;
        constexpr uintptr_t Name                 = 0x490;
        constexpr uintptr_t Speed                = 0x560;
        constexpr uintptr_t AimInfo              = 0xE50;
        constexpr uintptr_t Velocity             = 0xF0;
        constexpr uintptr_t IsGrounded           = 0x11C;
        constexpr uintptr_t FOVOffset            = 0x84;
        constexpr uintptr_t Weapon               = 0x600;
        constexpr uintptr_t ColWrite             = 0x80;
        constexpr uintptr_t DeathInfo            = 0x24A8;
    }

    namespace Weapon {
        constexpr uintptr_t Data                 = 0x80;
        constexpr uintptr_t Recoil               = 0x18;
    }

    namespace Avatar {
        constexpr uintptr_t Uma                  = 0x138;
        constexpr uintptr_t Uma_Visible          = 0x101;
        constexpr uintptr_t Uma_Data             = 0x28;
        constexpr uintptr_t UmaData_LocalPlayer  = 0x80;
        constexpr uintptr_t UmaData_Teammate     = 0x81;
        constexpr uintptr_t UmaData_Skip         = 0x81;
    }

    namespace Camera {
        constexpr uintptr_t Follow               = 0x30;
        constexpr uintptr_t IntPtr               = 0x10;
        constexpr uintptr_t Matrix               = 0x100;
    }

    namespace Aim {
        constexpr uintptr_t Direction            = 0x40;
        constexpr uintptr_t StartPosition        = 0x4C;
    }

    namespace Bone {
        constexpr uintptr_t Head                 = 0x6A0;
        constexpr uintptr_t Hip                  = 0x6A8;
        constexpr uintptr_t Breast               = 0x6B0;
        constexpr uintptr_t Root                 = 0x6C8;
        constexpr uintptr_t LeftShoulder         = 0x708;
        constexpr uintptr_t RightShoulder        = 0x710;
        constexpr uintptr_t LeftAnkle            = 0x6D8;
        constexpr uintptr_t RightAnkle           = 0x6E0;
        constexpr uintptr_t LeftToe              = 0x6E8;
        constexpr uintptr_t RightToe             = 0x6F0;
        constexpr uintptr_t RightForeArm         = 0x728;
        constexpr uintptr_t LeftForeArm          = 0x730;
        constexpr uintptr_t RightHand            = 0x718;
        constexpr uintptr_t LeftHand             = 0x720;
    }

    constexpr uintptr_t GameFacade = Base::GameFacade;
    constexpr uintptr_t GameFacade_P2 = Base::GameFacade_P2;
    constexpr uintptr_t BaseGame_Match = Base::BaseGame_Match;
    constexpr uintptr_t CurrentObserve = 0x100;
    constexpr uintptr_t ObserverPlayer = 0x30;
    constexpr uintptr_t BaseGame_Timer = Base::BaseGame_Timer;
    constexpr uintptr_t GhostHack = 0x814;
    constexpr uintptr_t Match_PlayerDict = Base::Match_PlayerDict;
    constexpr uintptr_t Match_LocalPlayer = Base::Match_LocalPlayer;

    constexpr uintptr_t Dict_Entries = Dict::Entries;
    constexpr uintptr_t Dict_Count = Dict::Count;
    constexpr uintptr_t Dict_ArrayHeader = Dict::ArrayHeader;
    constexpr uintptr_t Dict_EntryStride = Dict::EntryStride;
    constexpr uintptr_t Dict_EntryValue = Dict::EntryValue;

    constexpr uintptr_t Player_Rotation = Player::Rotation;
    constexpr uintptr_t Player_HeadTF = Player::HeadTF;
    constexpr uintptr_t Player_FootTF = Player::FootTF;
    constexpr uintptr_t Player_Camera = Player::Camera;
    constexpr uintptr_t Player_IsFiring = Player::IsFiring;
    constexpr uintptr_t Player_HP = Player::HP;
    constexpr uintptr_t Player_Armor = Player::Armor;
    constexpr uintptr_t Player_IsDead = Player::IsDead;
    constexpr uintptr_t Player_Avatar = Player::Avatar;
    constexpr uintptr_t Player_HedColider = Player::HeadCollider;
    constexpr uintptr_t Player_ColWrite = Player::ColWrite;
    constexpr uintptr_t Player_DeathInfo = Player::DeathInfo;
    constexpr uintptr_t Player_IsBot = Player::IsBot;
    constexpr uintptr_t Player_Name = Player::Name;
    constexpr uintptr_t Player_Speed = Player::Speed;
    constexpr uintptr_t Player_AimInfo = Player::AimInfo;
    constexpr uintptr_t Player_Velocity = Player::Velocity;
    constexpr uintptr_t Player_IsGrounded = Player::IsGrounded;
    constexpr uintptr_t Player_FOVOffset = Player::FOVOffset;
    constexpr uintptr_t Player_Weapon = Player::Weapon;

    constexpr uintptr_t Avatar_Uma = Avatar::Uma;
    constexpr uintptr_t Uma_Visible = Avatar::Uma_Visible;
    constexpr uintptr_t Uma_Data = Avatar::Uma_Data;
    constexpr uintptr_t UmaData_Skip = Avatar::UmaData_Skip;
    constexpr uintptr_t UmaData_Teammate = Avatar::UmaData_Teammate;
    constexpr uintptr_t UmaData_LocalPlayer = Avatar::UmaData_LocalPlayer;

    constexpr uintptr_t Camera_Follow = Camera::Follow;
    constexpr uintptr_t Camera_IntPtr = Camera::IntPtr;
    constexpr uintptr_t Camera_Matrix = Camera::Matrix;

    constexpr uintptr_t HP_List = 0x10;
    constexpr uintptr_t HP_Entry = 0x20;
    constexpr uintptr_t HP_Value = 0x18;

    constexpr uintptr_t Player_BulletInfo = Player::AimInfo;
    constexpr uintptr_t Bullet_SpawnPos = Aim::StartPosition;
    constexpr uintptr_t Bullet_DirWrite = Aim::Direction;

    constexpr uintptr_t MainCameraFallback = 0x3f0;
    constexpr uintptr_t WeaponInstance = Player::Weapon;
    constexpr uintptr_t ReloadInstance = 0x708;
    constexpr uintptr_t InventoryManager = 0x748;
    constexpr uintptr_t StatusStruct = 0x1B98;

    constexpr uintptr_t gameFacade = GameFacade;
    constexpr uintptr_t StaticGame = GameFacade_P2;
    constexpr uintptr_t CurrentMatch = BaseGame_Match;
    constexpr uintptr_t MatchIsRunning = Base::MatchIsRunning;
    constexpr uintptr_t DictionaryEntities = Match_PlayerDict;
    constexpr uintptr_t localPlayer = Match_LocalPlayer;
    constexpr uintptr_t playerAttributes = 0x770;
    constexpr uintptr_t m_LocalObserver = CurrentObserve;
    constexpr uintptr_t m_TargetPlayer = ObserverPlayer;
    constexpr uintptr_t m_LocalSpectator = 0x108;
    constexpr uintptr_t TargeSpectPlayer = 0x60;
    constexpr uintptr_t isClientBot = Player_IsBot;
    constexpr uintptr_t AimRotation = Player_Rotation;
    constexpr uintptr_t Player_SpineTF = Bone::Breast;
    constexpr uintptr_t Player_RootTF = Bone::Root;
    constexpr uintptr_t IsPrepareAttack = Player_IsFiring;
    constexpr uintptr_t Player_Data = Player_HP;
    constexpr uintptr_t Player_isDead = Player_IsDead;
    constexpr uintptr_t AvatarManager = Player_Avatar;
    constexpr uintptr_t AvatarObj = Avatar_Uma;
    constexpr uintptr_t Avatar_IsVisible = Uma_Visible;
    constexpr uintptr_t Avatar_Data = Uma_Data;
    constexpr uintptr_t Avatar_IsTeam = UmaData_Skip;
    constexpr uintptr_t FollowCamera = Player_Camera;
    constexpr uintptr_t CameraObj = Camera_Follow;
    constexpr uintptr_t Player_ShadowBase = Player_DeathInfo;
    constexpr uintptr_t Player_IsKnocked = 0x80;
    constexpr uintptr_t Player_TransformType = 0x11DC;
    constexpr uintptr_t LastAimingInfo = Player_BulletInfo;
    constexpr uintptr_t hitPos = Bullet_SpawnPos;
    constexpr uintptr_t firePos = Bullet_DirWrite;
    constexpr uintptr_t HeadCollider = Player_HedColider;
    constexpr uintptr_t Aimlock = Player_ColWrite;
    constexpr uintptr_t WeaponObj = WeaponInstance;
    constexpr uintptr_t WeaponData = Weapon::Data;
    constexpr uintptr_t WeaponRecoil = Weapon::Recoil;

    constexpr uintptr_t sAim1 = Player_IsFiring;
    constexpr uintptr_t sAim2 = Player_AimInfo;
    constexpr uintptr_t sAim3 = Bullet_SpawnPos;
    constexpr uintptr_t sAim4 = Bullet_DirWrite;
    constexpr uintptr_t CallSetAimRotationCount = 0x79C;

    constexpr uintptr_t IsGrounded = Player_IsGrounded;
    constexpr uintptr_t CurrentGroundState = 0xD0;
    constexpr uintptr_t LastGroundState = 0x110;
    constexpr uintptr_t Velocity = Player_Velocity;
    constexpr uintptr_t BHNEFIPKOMG = 0x24;
    constexpr uintptr_t InSnowSlideWayDashing = 0x1EA0;
    constexpr uintptr_t m_RightOffset = 0x8C;
    constexpr uintptr_t m_UpOffset = 0x90;
    constexpr uintptr_t FOVOffset = Player_FOVOffset;
    constexpr uintptr_t RunSpeedUpScale = 0x270;

    constexpr uintptr_t Engine_BaseGamePtr = GameFacade_P2;
    constexpr uintptr_t Engine_Match = BaseGame_Match;
    constexpr uintptr_t Engine_Dictionary = Match_PlayerDict;
    constexpr uintptr_t Engine_EntityList = Dict_Entries;
    constexpr uintptr_t Engine_TimeService = BaseGame_Timer;
    constexpr uintptr_t Engine_TimeServiceSpeed = Base::Timer_Delta;

    constexpr uintptr_t Match_MatchRunning = MatchIsRunning;
    constexpr uintptr_t Match_LocalPlayer2 = Match_LocalPlayer;

    constexpr uintptr_t Cam_FollowCamera = Player_Camera;
    constexpr uintptr_t Cam_CameraObject = Camera_Follow;
    constexpr uintptr_t Cam_RawCameraData = Camera_IntPtr;
    constexpr uintptr_t Cam_ViewMatrix = Camera_Matrix;

    constexpr uintptr_t Transform_TransformToObj = 0x10;
    constexpr uintptr_t Transform_ObjToIndex = 0x40;
    constexpr uintptr_t Transform_ObjToMatrix = 0x38;
    constexpr uintptr_t Transform_MatrixList = 0x18;
    constexpr uintptr_t Transform_MatrixIndices = 0x20;
    constexpr uintptr_t Transform_MatrixSize = 0x30;
    constexpr uintptr_t Transform_IndexSize = 0x4;

    constexpr uintptr_t Player_MainCamera = 0x3f0;
    constexpr uintptr_t Player_WeaponInstance = WeaponInstance;
    constexpr uintptr_t Player_ReloadInstance = ReloadInstance;
    constexpr uintptr_t Player_AvatarManager = Player_Avatar;
    constexpr uintptr_t Player_StatusStruct = StatusStruct;
    constexpr uintptr_t Player_IsSpectating = Player_IsDead;
    constexpr uintptr_t Player_HeadCollider = Player_HedColider;
    constexpr uintptr_t Player_AimTargetWrite = Player_ColWrite;
    constexpr uintptr_t Player_FiringState = Player_IsFiring;
    constexpr uintptr_t Player_ViewRotation = Player_Rotation;
    constexpr uintptr_t Player_ClientBotCheck = Player_IsBot;
    constexpr uintptr_t Player_NamePtr = Player_Name;
    constexpr uintptr_t Player_InventoryManager = InventoryManager;
    constexpr uintptr_t Player_AimingInfo = Player_AimInfo;
    constexpr uintptr_t Player_AimingInfoStartPos = Bullet_SpawnPos;
    constexpr uintptr_t Player_AimingInfoDirection = Bullet_DirWrite;
    constexpr uintptr_t Player_HealthChain1 = Player_HP;
    constexpr uintptr_t Player_HealthChain2 = HP_List;
    constexpr uintptr_t Player_HealthChain3 = HP_Entry;
    constexpr uintptr_t Player_HealthValue = HP_Value;
    constexpr uintptr_t Player_IsDead2 = Player_IsDead;
    constexpr uintptr_t Player_HeadTF2 = Player_HeadTF;
    constexpr uintptr_t Player_FootTF2 = Player_FootTF;

    constexpr uintptr_t Weapon_WeaponData = Weapon::Data;
    constexpr uintptr_t Weapon_RecoilValue = Weapon::Recoil;
    constexpr uintptr_t Weapon_FastReloadTrigger = 0x111;

    constexpr uintptr_t Status_StatusData = 0x20;
    constexpr uintptr_t Status_StateID = 0x10;

    constexpr uintptr_t Avatar_UmaSimple = Avatar_Uma;
    constexpr uintptr_t Avatar_IsVisible2 = Uma_Visible;
    constexpr uintptr_t Avatar_UmaData = Uma_Data;
    constexpr uintptr_t Avatar_IsDead = UmaData_Skip;

    constexpr uintptr_t Inventory_ActiveItem = 0xA0;
    constexpr uintptr_t Inventory_WeaponComponent = 0x80;
    constexpr uintptr_t Inventory_NoRecoil = 0x18;
    constexpr uintptr_t Inventory_ItemData = 0x28;
    constexpr uintptr_t Inventory_WeaponId = 0x40;

    constexpr uintptr_t Bones_Head = Bone::Head;
    constexpr uintptr_t Bones_Chest = Bone::Breast;
    constexpr uintptr_t Bones_Hip = Bone::Hip;
    constexpr uintptr_t Bones_Feet = Bone::LeftToe;
    constexpr uintptr_t Bones_Neck = Bone::Head;
    constexpr uintptr_t Bones_Spine = Bone::Breast;
    constexpr uintptr_t Bones_LeftShoulder = Bone::LeftShoulder;
    constexpr uintptr_t Bones_RightShoulder = Bone::RightShoulder;
    constexpr uintptr_t Bones_LeftHand = Bone::LeftHand;
    constexpr uintptr_t Bones_RightHand = Bone::RightHand;
    constexpr uintptr_t Bones_LeftAnkle = Bone::LeftAnkle;
    constexpr uintptr_t Bones_RightAnkle = Bone::RightAnkle;
    constexpr uintptr_t Bones_Breast = Bone::Breast;
    constexpr uintptr_t Bones_Root = Bone::Root;
    constexpr uintptr_t Bones_LeftToe = Bone::LeftToe;
    constexpr uintptr_t Bones_RightToe = Bone::RightToe;
    constexpr uintptr_t Bones_RightForeArm = Bone::RightForeArm;
    constexpr uintptr_t Bones_LeftForeArm = Bone::LeftForeArm;

    constexpr uintptr_t Bone_Head = Bone::Head;
    constexpr uintptr_t Bone_Hip = Bone::Hip;
    constexpr uintptr_t Bone_Breast = Bone::Breast;
    constexpr uintptr_t Bone_Root = Bone::Root;
    constexpr uintptr_t Bone_LeftShoulder = Bone::LeftShoulder;
    constexpr uintptr_t Bone_RightShoulder = Bone::RightShoulder;
    constexpr uintptr_t Bone_LeftAnkle = Bone::LeftAnkle;
    constexpr uintptr_t Bone_RightAnkle = Bone::RightAnkle;
    constexpr uintptr_t Bone_LeftToe = Bone::LeftToe;
    constexpr uintptr_t Bone_RightToe = Bone::RightToe;
    constexpr uintptr_t Bone_RightForeArm = Bone::RightForeArm;
    constexpr uintptr_t Bone_LeftForeArm = Bone::LeftForeArm;
    constexpr uintptr_t Bone_RightHand = Bone::RightHand;
    constexpr uintptr_t Bone_LeftHand = Bone::LeftHand;

    constexpr uintptr_t PlayerAttributes = 0x770;
    constexpr uintptr_t ShootNoReload   = 0x111;
}
