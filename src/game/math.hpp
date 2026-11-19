#pragma once

#include "game.hpp"
#include "../offsets/offsets.hpp"
#include "../other/vector3.h"
#include "../other/memory.hpp"
#include "../globals.hpp"
#include <cmath>

// ==============================================================================================
// FILE: src/game/math.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Low-level mathematical engine for Unity 3D Transform traversal and W2S projection.
//
// CROSS-FILE CALL GRAPH & USAGE SITES:
// ----------------------------------------------------------------------------------------------
// 1. CALLERS:
//    - src/game/data.cpp -> boneScreen(), readBone(), update():
//      * Calls GetNodePosition() to extract world coordinates from Unity bone nodes (Head, Toe).
//      * Calls WorldToScreenPoint() using viewMatrix to project 3D bones to 2D screen coordinates.
//      * Calls calculate_distance() to compute player-to-enemy physical range in meters.
//    - src/func/aim/target.hpp:
//      * Computes 2D screen Euclidean distances to determine target nearest to screen center / crosshair.
//
// 2. MATHEMATICAL ARCHITECTURE:
//    - Unity Transform Hierarchy Traversal:
//      * Unity stores hierarchical bone trees in flattened array buffers (TransformAccessReadOnly).
//      * Traverses bone indices recursively (up to 50 iterations) applying quaternion rotations
//        and scaling matrices until world-space position is reached.
//    - WorldToScreen Projection:
//      * Multiplies 3D world vector by Unity Camera 4x4 ViewMatrix.
//      * Performs perspective divide (1.0f / W) and maps Normalized Device Coordinates (NDC)
//        directly into physical display screen pixels [0 .. width, 0 .. height].
// ==============================================================================================

// Extracts the true 3D world position from a native Unity Transform component pointer
inline Vector3 GetPosition(uint64_t Transform) {
    if (Transform == 0) return Vector3::Zero();

    // 1. Dereference Transform internal pointer (TransformAccess)
    auto transformObjValue = rpm<uint64_t>(Transform + Offsets::Transform::Obj);
    if (transformObjValue == 0) return Vector3::Zero();

    // 2. Resolve Matrix list and Index buffer pointers
    auto matrixValue = rpm<uint64_t>(transformObjValue + Offsets::Transform::MatrixPtr);
    if (!matrixValue) return Vector3::Zero();

    uint64_t matrices = rpm<uint64_t>(matrixValue + Offsets::Transform::Matrices);
    uint64_t indices  = rpm<uint64_t>(matrixValue + Offsets::Transform::Indices);
    if (!matrices || !indices) return Vector3::Zero();

    // 3. Extract bone transform slot index
    int indexValue = rpm<int>(transformObjValue + Offsets::Transform::Index);
    if (indexValue < 0 || indexValue > 50000) return Vector3::Zero();

    // 4. Read initial position from matrices buffer
    auto resultValue = rpm<Vector3>(matrices + ((uint64_t)indexValue * Offsets::Transform::MatrixStride));
    int transformIndexValue = rpm<int>(indices + ((uint64_t)indexValue * Offsets::Transform::IndexStride));

    // 5. Walk parent hierarchy multiplying quaternion rotations & scale
    int tries = 0;
    while (transformIndexValue >= 0 && transformIndexValue <= 50000 && tries < 50) {
        tries++;
        uint64_t matAddr = matrices + ((uint64_t)transformIndexValue * Offsets::Transform::MatrixStride);
        auto t = rpm<TransformMatrix>(matAddr);

        auto rx = t.rotation.x, ry = t.rotation.y, rz = t.rotation.z, rw = t.rotation.w;
        auto sx = resultValue.x * t.scale.x;
        auto sy = resultValue.y * t.scale.y;
        auto sz = resultValue.z * t.scale.z;

        // Standard Unity quaternion-vector multiplication decomposition
        resultValue.x = t.position.x + sx +
                        (sx * ((ry * ry * -2.0f) - (rz * rz * 2.0f))) +
                        (sy * ((rw * rz * -2.0f) - (ry * rx * -2.0f))) +
                        (sz * ((rz * rx *  2.0f) - (rw * ry * -2.0f)));
        resultValue.y = t.position.y + sy +
                        (sx * ((rx * ry *  2.0f) - (rw * rz * -2.0f))) +
                        (sy * ((rz * rz * -2.0f) - (rx * rx *  2.0f))) +
                        (sz * ((rw * rx * -2.0f) - (rz * ry * -2.0f)));
        resultValue.z = t.position.z + sz +
                        (sx * ((rw * ry * -2.0f) - (rx * rz * -2.0f))) +
                        (sy * ((ry * rz *  2.0f) - (rw * rx * -2.0f))) +
                        (sz * ((rx * rx * -2.0f) - (ry * ry *  2.0f)));

        // Advance to next parent index in hierarchy
        transformIndexValue = rpm<int>(indices + ((uint64_t)transformIndexValue * Offsets::Transform::IndexStride));
    }

    if (tries >= 50) return Vector3::Zero();
    if (std::isnan(resultValue.x) || std::isnan(resultValue.y) || std::isnan(resultValue.z)) return Vector3::Zero();

    return resultValue;
}

// Resolves a game-level ITransformNode pointer (e.g., HeadTF 0x6A0) to its world 3D position
inline Vector3 GetNodePosition(uint64_t nodeTransform) {
    if (!is_valid_ptr(nodeTransform)) return Vector3::Zero();

    // Primary: nodeTransform is an ITransformNode containing a Transform pointer at +0x10
    auto transformValue = rpm<uint64_t>(nodeTransform + Offsets::Transform::Node);
    if (is_valid_ptr(transformValue)) {
        Vector3 pos = GetPosition(transformValue);
        if (pos != Vector3::Zero()) return pos;
    }

    // Fallback: nodeTransform is already a direct Transform component
    return GetPosition(nodeTransform);
}

// Projects 3D world coordinates into 2D screen viewport coordinates using the game's ViewMatrix
inline Vector3 WorldToScreenPoint(const matrix& vm, const Vector3& pos) {
    // Read atomic screen resolution snapshots to prevent race conditions during device rotation
    const float sw = g_sw;
    const float sh = g_sh;

    // Perspective depth component (W)
    float v12 = (pos.x * vm.m14) + (pos.y * vm.m24) + (pos.z * vm.m34) + vm.m44;

    // Check if point is in front of the camera near clipping plane
    if (v12 >= 0.001f) {
        float inv = 1.0f / v12;
        float cx = sw * 0.5f;
        float cy = sh * 0.5f;

        // Normalized Device Coordinates (NDC) transformed to screen pixel offsets
        float v9  = (pos.x * vm.m11) + (pos.y * vm.m21) + (pos.z * vm.m31) + vm.m41;
        float v10 = (pos.x * vm.m12) + (pos.y * vm.m22) + (pos.z * vm.m32) + vm.m42;

        // Return Vector3 where x,y are screen coordinates, and z is the depth distance
        return Vector3(cx + cx * v9 * inv, cy - cy * v10 * inv, v12);
    }

    // Point is behind camera
    return Vector3(-1, -1, -1);
}

// Computes 3D Euclidean distance between two points in meters
inline float calculate_distance(const Vector3& a, const Vector3& b) noexcept {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
