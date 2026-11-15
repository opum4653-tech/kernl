# NINMODS KERNEL — Deep Technical Architecture

This document details the internal mathematical models, kernel driver interfaces, memory traversal pipelines, and rendering architectures powering `ninmods-kernel`.

---

## 1. World-to-Screen Projection Pipeline (`src/game/math.hpp`)

The projection transforms a 3D in-game world position `Vector3(x, y, z)` into a 2D screen coordinate `Vector3(screen_x, screen_y, w)` using Unity's Camera View Matrix:

$$\begin{pmatrix} v_9 \\ v_{10} \\ v_{11} \\ v_{12} \end{pmatrix} = \begin{pmatrix} m_{11} & m_{21} & m_{31} & m_{41} \\ m_{12} & m_{22} & m_{32} & m_{42} \\ m_{13} & m_{23} & m_{33} & m_{43} \\ m_{14} & m_{24} & m_{34} & m_{44} \end{pmatrix}^T \begin{pmatrix} x \\ y \\ z \\ 1 \end{pmatrix}$$

Where:
- $v_{12} = x \cdot m_{14} + y \cdot m_{24} + z \cdot m_{34} + m_{44}$ (Clip-space W component)
- When $v_{12} \ge 0.001$, the target is in front of the camera.
- The 2D viewport center is dynamically computed:
  $$c_x = \frac{g_{sw}}{2}, \quad c_y = \frac{g_{sh}}{2}$$
- Screen coordinates:
  $$\text{screen}_x = c_x + c_x \cdot \frac{v_9}{v_{12}}, \quad \text{screen}_y = c_y - c_y \cdot \frac{v_{10}}{v_{12}}$$

**Invariant:** Both $c_x$ and $c_y$ depend directly on $g_{sw}$ and $g_{sh}$. If $g_{sw}$ is static (e.g. 1920) on a 2400-wide screen, the projection is horizontally distorted by $\frac{2400 - 1920}{2} = 240\text{px}$ at center, causing all ESP lines and boxes to miss the targets.

---

## 2. Unity Bone Transform Traversal (`GetNodePosition`)

Rather than relying on brittle hardcoded bone coordinates, bone positions are resolved dynamically from Unity's Transform hierarchy:

1. Follows `nodeTransform + 0x10` to reach `TransformObj`.
2. Extracts `matrixValue` at `+0x38`.
3. Reads matrix pointer table (`matrices` at `+0x18`) and index table (`indices` at `+0x20`).
4. Iteratively rotates and scales parent bone matrices using quaternion rotation mathematics:
   $$\mathbf{r}' = \mathbf{r} + \mathbf{s} + (\mathbf{s} \times \dots)$$
5. Returns the true world-space position `Vector3` of the head, spine, shoulders, and feet.

---

## 3. High-Throughput Memory Batching (`PlayerSpan`)

To minimize kernel context switches, the entity reader uses `PlayerSpan`:
- In standard userland tools, reading 12 fields per player across 30 players results in 360 syscalls per tick.
- `PlayerSpan` pulls bytes `0x70` to `0x860` in a single read:
  - 1 call per player instead of 12.
  - Safe memory bounds checking: object is committed up to `0x24B0`.
  - Zero memory scraping footprint.

---

## 4. Crash-Proof Native Surface Pipeline (`ANativeWindowCreator`)

- Bypasses unstable userspace Binder IPC `getDisplayState` calls that crash on Android 12 through 15.
- Uses direct hardware probing of `/sys/class/graphics/fb0/virtual_size` to establish framebuffer dimensions.
- Creates an unmanaged hardware overlay layer directly via `ANativeWindowCreator::Create("AImGui", width, height, false, false)` without registering window tokens in Android `WindowManagerService`.
