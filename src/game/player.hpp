#pragma once
#include "../other/vector3.h"
#include <cstdint>

struct PlayerData {
    uint64_t addr;

// Box
    Vector3  Head;
    Vector3  Toe;
    Vector3  headWorld;

// Meta
    bool     isVisible;
    bool     isBot;
    float    distance;
    int      curHP;
    int      maxHP;
    int      curArmor;
    bool     isFiring;
    bool     isGrounded;
    float    speed;
    char     name[36];
    int      weaponType;

    PlayerData()
        : addr(0),
          Head(Vector3::Zero()), Toe(Vector3::Zero()), headWorld(Vector3::Zero()),
          isVisible(false), isBot(false), distance(0.f),
          curHP(0), maxHP(200), curArmor(0),
          isFiring(false), isGrounded(true), speed(0.f),
          weaponType(-1)
    { name[0] = 0; }
};
