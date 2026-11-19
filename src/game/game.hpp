#pragma once

#include "../other/vector3.h"

// ==============================================================================================
// FILE: src/game/game.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Unity 3D engine matrix representations (Camera ViewMatrix & Bone TransformMatrix).
//
// CROSS-FILE CALL GRAPH & CONSUMERS:
// ----------------------------------------------------------------------------------------------
// 1. matrix (Unity 4x4 ViewMatrix):
//    - Read from target memory in src/game/data.cpp -> data::update():
//      viewMatrix = rpm<matrix>(IntPtr + Offsets::Camera_Matrix).
//    - Consumed in src/game/math.hpp -> WorldToScreenPoint(viewMatrix, pos):
//      multiplies world vectors by ViewMatrix columns to derive screen coordinates.
//
// 2. TransformMatrix (Unity Native Transform Internal Layout):
//    - Read in src/game/math.hpp -> GetPosition():
//      auto t = rpm<TransformMatrix>(matAddr) where matAddr is matrices buffer.
//    - Unpacks position (Vector4), quaternion rotation (Quaternion), and scaling (Vector4).
// ==============================================================================================

// Standard Unity 4x4 Row-Major Camera Projection Matrix (64 bytes)
struct matrix {
    float m11, m12, m13, m14; // Column 1
    float m21, m22, m23, m24; // Column 2
    float m31, m32, m33, m34; // Column 3
    float m41, m42, m43, m44; // Column 4 (Translation & W Perspective divide)
};

// Unity Native Transform Hierarchy Memory Record (48 bytes = Transform::MatrixStride)
struct TransformMatrix {
    Vector4    position; // Local bone translation offset {x, y, z, w}
    Quaternion rotation; // Local bone orientation quaternion {x, y, z, w}
    Vector4    scale;    // Local bone scaling factor {x, y, z, w}
};
