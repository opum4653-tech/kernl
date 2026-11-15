# NINMODS KERNEL — Project Memory & Incident Log

This file is the permanent historical memory and technical log for the `ninmods-kernel` native overlay project.

---

## 1. Engine Specifications & Target Environment

- **Binary Name:** `ninmods-kernel`
- **Packaging:** `ninmods-kernel.zip` (deployed to `https://ninmods.smmtor.com/downloads/kernel/ninmods-kernel.zip`)
- **Compilation Toolchain:** Android NDK r25c (`ndk-build`, aarch64-linux-android)
- **Target Game:** Free Fire Global (`com.dts.freefireth`) & Free Fire MAX (`com.dts.freefiremax`)
- **Game Version:** OB55 (1.132.1)
- **Driver Architecture:** Ring 0 Linux Kernel Driver (RT Hook / RT DEV / DEV / QX / KPM Neo)
- **Rendering Pipeline:** Android Native Surface (`ANativeWindow`) + ImGui

---

## 2. Incident Reports & Technical Root Cause Analyses

### Incident 1: ESP Trace Line Offset / Drawing in Empty Space
- **Symptoms:** User reported that the ESP tracer line was falling on empty space instead of connecting to the enemy's body.
- **Root Cause Analysis:**
  1. In `src/globals.cpp`, global resolution variables were initialized as:
     `float g_sw = 1920.0f; float g_sh = 1080.0f;`
  2. While `start_overlay()` in `main.cpp` initialized `native_window_screen_x` and `native_window_screen_y` to the phone's actual resolution (e.g. 2400x1080 or 2460x1080), it **never updated `g_sw` and `g_sh`**.
  3. `WorldToScreenPoint` in `src/game/math.hpp` calculates 2D screen positions using:
     `float cx = g_sw * 0.5f; float cy = g_sh * 0.5f;`
     On a 2400x1080 screen, `cx` evaluated to `960` instead of `1200`, causing an immediate **240 pixel horizontal error** at the screen center, scaling even worse toward the screen edges.
  4. ImGui rendered onto the native 2400-wide surface, resulting in the projected enemy positions and tracer lines landing in empty space far to the left of the actual 3D character models.
  5. Furthermore, `drawTracer` in `src/func/esp/line.hpp` drew exclusively to raw `p.Head.x, p.Head.y` regardless of whether the line originated from the top, center, or bottom of the screen.
- **Fix Implemented:**
  1. **Dynamic Resolution Synchronization:**
     - In `src/main.cpp`: Updated `g_sw` and `g_sh` immediately upon surface creation.
     - In `includes/draw/Android_draw/draw.cpp`: Dynamically synchronized `g_sw` and `g_sh` in `drawBegin()` whenever screen orientation changes.
     - In `src/ui/menu.cpp`: Synchronized `g_sw = realW; g_sh = realH;` on every frame render.
  2. **Anatomical Tracer Line Snapping:**
     - Updated `drawTracer(dl, p, pb, colBase)` to anchor dynamically to the bounding box:
       - Top Origin: Snaps to `(midX, pb.tl.y)` (Head / top of box).
       - Center Origin: Snaps to `(midX, (pb.tl.y + pb.br.y) * 0.5f)` (Chest / center of box).
       - Bottom Origin: Snaps to `(midX, pb.br.y)` (Feet / bottom of box).

### Incident 2: Floating Logo Sizing Defect (Undersized Touch Target)
- **Symptoms:** The floating 'N' logo appeared excessively small on high-resolution Android screens, making it difficult to spot and tap.
- **Root Cause:**
  - `logoRadius` in `src/ui/menu.cpp` was set to `28.0f * ui_scale` (diameter 56px). On 1080p/1440p displays with 450+ PPI, 56px is smaller than standard touch accessibility guidelines (minimum 48dp / 72-80px).
- **Fix Implemented:**
  - Increased `logoRadius` to **`38.0f * ui_scale`** (diameter 76px, +36% larger).
  - The circular dark disc, outer orchid ring, center "N" letter font size (`radius * 1.15f`), daemon status dot, and touch hitbox (`Touch_SetUiRect`) all scale up proportionately.

---

## 3. Core Modules & Responsibilities

| File Path | Role |
| :--- | :--- |
| `src/main.cpp` | Entry point, license gate, native window creation, render loop |
| `src/ui/menu.cpp` | Floating 'N' logo, electric orchid menu, touch zone management |
| `src/func/esp/line.hpp` | ESP Tracer snapline renderer with anatomical box snapping |
| `src/func/esp/esp.hpp` | Main ESP dispatch pipeline (Box, Line, Info, Health, Weapon, Loot) |
| `src/game/data.cpp` | High-speed entity parsing, `PlayerSpan` DMA caching, local player tracking |
| `src/game/math.hpp` | Unity WorldToScreen matrix projection, node position resolution |
| `src/offsets/offsets.hpp` | Centralized memory offsets for OB55 (1.132.1) |
