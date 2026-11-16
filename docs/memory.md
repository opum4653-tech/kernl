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

---

## 4. PewPew Integration & Feature Additions

Key features from reference `pewpew` codebase ported and integrated into `ninmods-kernel`:

1. **Multi-Level Pointer Chain Resolver (`src/other/addr.hpp`):**
   - Implemented `find_addr(base, offsets, count)` utility resolving dynamic Unity runtime pointers.

2. **Wide FOV / iPad View (`src/func/speed.hpp`):**
   - Memory offset `0x36C9BD0` from `libil2cpp.so`.
   - Dynamic slider range `1.0x` to `2.5x`. Clean reverse-path restoration to `1.0f` on disable.

3. **Auto-Pickup Range Mod (`src/func/speed.hpp`):**
   - Pointer chain: `libil2cpp + 0xF1D5F70 -> 0x30 -> 0x4B8 -> 0x1EB4`.
   - Expands loot collection radius up to 35 meters. Restores standard 2.0m radius on disable.

4. **Hitbox / Player Size Scaler (`src/func/speed.hpp`):**
   - Pointer chain: `libil2cpp + 0xF1D5F70 -> 0x30 -> 0x4B8 -> 0x510 -> [0x1FC, 0x200, 0x204]`.
   - Scales enemy X, Y, Z hitboxes from `1.0x` to `3.0x` for effortless bullet registration.

5. **High Jump Elevation Mod (`src/func/speed.hpp`):**
   - Pointer chain: `libil2cpp + 0xF1D5F70 -> 0x30 -> 0x4B8 -> 0x518 -> 0x21C`.
   - Modifies vertical jump impulse from `1.0x` to `5.0x`. Fully restored on toggle off.

---

## 5. UI Touch Scrolling & Hidden Scrollbar Restoration

1. **Hidden Scrollbar with Full Page Touch Scrolling (`menu.cpp`):**
   - **Requirement:** The visual scrollbar bar is 100% transparent/hidden (`ScrollbarBg`, `ScrollbarGrab*` with alpha 0.0), matching the original UI clean aesthetic.
   - **Fix:** Restored `handleTouchScroll()` to the clean swipe detection gesture (`dx/dy` threshold `5.0f * 1.1f`) without aggressive `ClearActiveID()` interrupts, allowing seamless vertical finger scrolling across the entire page while keeping the scrollbar visual elements completely invisible.

---

## 6. Headshot & Silent Aim Engine Resolution (`ff-global-dump (1).cs` Alignment)

Comprehensive offset audit and aim engine fix:

1. **Bone Offset Realignment from `ff-global-dump (1).cs`:**
   - **Head Transform Node:** `0x6A8` (`protected ITransformNode COBFGNOIPMF; // 0x6a8` at line 1036419). Was previously `0x6A0` (`GBKFHDFCPMD`, neck node), causing headWorld to calculate off-target.
   - **Foot / Toe Transform Node:** `0x6F0` (`protected ITransformNode JMHBBGIKBNO; // 0x6f0` at line 1036428).
   - **Player NickName:** `0x498` (`public String OriginalNickName; // 0x498` at line 1036315).
   - **Head Collider:** `0x740` (`protected GMOCOOEIFMK BKEPAKCAGNJ; // 0x740` at line 1036438), Alt `0x738` (`protected Collider BANPEGJEAKE; // 0x738`).
   - **Locked Aiming Collider:** `0x80` (`private Collider <AONPKGBGHHI>k__BackingField; // 0x80` on `AttackableEntity` at line 1051013).

2. **Headshot Aim Collider Registration (`src/func/aim/headshot.hpp`):**
   - Free Fire 64-bit hit registration verifies bullet raycasts against `target.LockedAimingCollider` (0x80) on `AttackableEntity`.
   - On headshot rolls, writes `HeadCollider` (0x740 / 0x738) into `target.addr + 0x80`.
   - `0x80` is 8-byte aligned (`0x80..0x87`) and strictly preserves `0x7C` (`Boolean IsDead`).

3. **High-Speed Writer Flooding Architecture (`src/func/aim/silent.hpp`):**
   - Unity's weapon script continuously overwrites `AimingInfo + 0x40` (`HLDECMJFKJK`) during `Update()`. A slow 10ms loop gets overwritten before `BulletFire()` executes.
   - Implemented dedicated writer loop (`writer_loop`) that continuously floods `wpm<Vector3>(aimInfo + 0x40, targetDir)` and sets `wpm<bool>(aimInfo + 0x71, false)` with `sched_yield()` during active firing, matching `pewpew` and reference design. Zero bullets miss.

---

## 7. Advanced Dual-Engine KPM & Driver Ecosystem

Comprehensive modernization of Kernel Process Memory (`kpm_driver.h`) integrated and enhanced from `pewpew`:

1. **Dual-Engine KPM (`includes/driver/kpm_driver.h`):**
   - **TB KPM (TongBao / TianBao):** Uses `prctl(PRCTL_MEM_READ 0x63687501)` / `prctl(PRCTL_MEM_WRITE 0x63687502)`. Direct kernel ring-0 pointer transfer with zero device node footprint.
   - **Neo KPM:** Uses `prctl(NA_PRCTL_MAGIC 0x4E41)` with command struct `na_cmd`.
   - **Auto-Detection:** Automatically probes TB KPM first; if absent, probes Neo KPM.

2. **Fixing PewPew Implementation Flaws:**
   - **4096-Byte Limit Bug:** `pewpew` rejected any read/write greater than `NA_MAX_RW_SIZE` (`if (size > 4096) return false;`). Our implementation chunk-loops the request (`while (bytes_left > 0)`), supporting arbitrary buffer sizes without truncation.
   - **Abrupt Termination (`exit(0)`):** `pewpew` called `exit(0)` when KPM was unavailable. Our driver features an intelligent fallback ladder to `process_vm_readv` (PVM Syscall mode), preventing crashes.
   - **Expanded Node Scanner:** Added `"facking"` and `"YBBBYN"` to `rt_dev.h` device table.
   - **Dynamic UI Telemetry:** `menu.cpp` Tab 2 dynamically reflects the active engine (`TB KPM`, `NEO KPM`, or `PVM SYSCALL`).


---

## 8. HitInfo Engine, Normalized Trajectory Steering & Responsive Sidebar

Comprehensive resolution of Headshot / Silent Aim mechanics using internal `NXL` project reverse engineering:

1. **Unity Free Fire Damage Calculation Architecture (`CGKJLKPMGDJ`):**
   - In Free Fire 64-bit, `localPlayer + 0xE58` (`PKIOAMDCOPB`) and `0xE50` (`FDMIEDDNCEC`) point to instances of `CGKJLKPMGDJ` (HitInfo / AimingInfo).
   - Analysis of `NXL` hook (`hook_OnCalcDamage`) revealed that headshot registration strictly relies on 8 synchronized fields:
     - `0x18`: `JHAINOBIFMA` (GameObject)
     - `0x20`: `JINDNOFABKB` (Collider - Enemy Head Collider)
     - `0x28`: `HPCOJHGFEEG` (HitLocation - Exact Enemy Head 3D World Position)
     - `0x34`: `DOJDNHCGFGK` (HitNormal - Normalized Bullet Direction Vector)
     - `0x40`: `HLDECMJFKJK` (RayDir - Normalized Direction Vector)
     - `0x64`: `DKPDBLLJEAA` (HitGroup enum `LKOOALMKJND`: **1 = Head**, 2 = Body)
     - `0x70`: `EFMBNPNFGHM` (IgnoreHappens: `false`)
     - `0x71`: `CEOKOILBODN` (ViewBlocked: `false`)
     - `0x80`: `CLOLKHNMNEK` (SpecialHitType: `(int16)0`)
     - `target.addr + 0x80`: `LockedAimingCollider` locked to `headCol`
     - `localPlayer + 0xE60`: `LastAimingInfoFromWeaponAdjusted` set to `false`.

2. **Root Cause of Headshot & Silent Aim Failure:**
   - Previous code only wrote an unnormalized difference vector (`diff = target3D - bulletOrigin`, magnitude 20~100) to `0x40`. Unity Raycast requires a normalized unit vector; non-unit vectors caused raycast rejection.
   - `0x64` (`HitGroup`) was NEVER set to `1`. Free Fire therefore treated hits as non-headshot body hits.
   - `0x28` (`HitLocation`) and `0x20` (`HitCollider`) were never written to `AimInfo`.
   - `isFiring` trigger condition was dropping locks between tap-fire intervals.

3. **High-Speed Writer & HitInfo Flooding Engine:**
   - High-speed writer thread floods the complete 8-field `CGKJLKPMGDJ` hit block continuously while a target is acquired in FOV or firing.
   - Computes `diff.normalized()` to deliver exact unit direction vectors.
   - Sets `HitGroup = 1` for guaranteed red-number headshot registration.

4. **UI Sidebar & Icon Button Modernization (`menu.cpp`):**
   - Sidebar width enlarged from fixed `76.0f` to responsive `std::max(88.0f, 96.0f * ui_scale)`.
   - Icon button size increased from `56x52` to `(sidebarW - 16.0f) x std::max(56.0f, 62.0f * ui_scale)`.
   - Vector icon size dynamically scaled from static `23px` to `size.y * 0.54f` (~`34px`+), with proportional stroke thickness (`3.0f` active pill indicator, `12.0f` corner rounding).
   - Generates spacious, touch-friendly, balanced ergonomics matching modern tablets and high-DPI displays.


---

## 9. Modular UI Refactoring, Strict Mutual Exclusivity & 17-Point Audit Resolution

### 1. Architectural File Split
In commit `bf27ca9` / `2530d91`, `src/ui/menu.cpp` was modularized into 3 dedicated files:
1. `src/ui/floating_logo.hpp & .cpp`:
   - Floating circular 'N' logo window (`##NinmodsKernelPill`).
   - Dragging with instant viewport boundary clamping (`std::clamp`).
   - Tap vs Drag discrimination on release (`s_was_activated && deactivated && !s_moved_drag`).
   - Daemon connection status indicator dot (`proc::pid > 0 && proc::lib > 0`).
   - Zero-trace persistent anchor via `/dev/.nin_logo_pos.txt` (tmpfs node).
2. `src/ui/theme.hpp & .cpp`:
   - Centralized `theme_orchid` dynamic palette with `sync_from_arr()`.
   - 6 theme color swatches (`g_ThemeSwatches`).
   - Vector icon renderer (`DrawVectorIcon`).
3. `src/ui/menu.hpp & .cpp`:
   - Dual-column glass main menu window.
   - When `!g_ShowMenu`, delegates cleanly to `ui::floating_logo::render()`.
   - Backward compatible API (`ui::menu::render()`, `ui::menu::save_logo_pos()`).

### 2. Strict Mutual Exclusivity Lifecycle (`g_ShowMenu`)
- **Menu Closed (`g_ShowMenu == false`):** Main window is not rendered; floating logo is drawn.
- **Logo Tapped:** User releases finger without drag -> `g_ShowMenu = true`.
- **Menu Open (`g_ShowMenu == true`):** Floating logo is completely hidden from render; main menu is drawn.
- **Close Button Tapped ('X'):** `g_ShowMenu = false` -> Menu disappears, floating logo re-appears.

### 3. Comprehensive 17-Point Audit Resolutions
- **Style Stack Balance:** Exactly 20 `PushStyleColor` / `PopStyleColor(20)`. Zero stack leaks.
- **Sidebar Overlap Prevention:** `btnH = std::clamp(availSidebarH / 4.6f, 36.0f, 60.0f);` and `powerY = std::max(tab2Bottom + spacing, bodyH - btnH - 12.0f * ui_scale);`. 0px overlap across all screen heights.
- **Full Viewport Scaling:** `headerH`, `closeBtnSize`, `xRadius`, `badgeH`, `badgeW`, `contentGapX/Y`, `colGap`, `colW`, `WindowPadding`, `ItemSpacing`, `FramePadding`, and `GrabMinSize` scaled with `* ui_scale`.
- **High-DPI Font Sharpness:** `LoadCustomIconFonts(io)` in `main.cpp` bakes `inter_medium` at 22px and `inter_bold` at 25px.
- **Dynamic Theme Reactivity:** Sidebar active state derives alpha directly from `theme_orchid::ColPrimary`: `(ColPrimary & 0x00FFFFFF) | (48 << 24)`.
- **Header Drag Boundary Clamping:** `s_menu_pos.x = std::clamp(s_menu_pos.x, 0.0f, realW - winW);` and `s_menu_pos.y = std::clamp(s_menu_pos.y, 0.0f, realH - winH);`. Menu cannot be lost off-screen.
- **Touch Scroll Gesture:** `ImGuiHoveredFlags_AllowWhenBlockedByActiveItem` prevents widgets from swallowing swipe gestures.
- **Format Safety:** Guarded target name with `targetName = g_currentTarget.name[0] ? g_currentTarget.name : "Enemy"`.
- **Clean Telemetry & Layout Balance:** Tab 2 left column filled with `Air Flight`, `Phantom Mode`, and `MOVEMENT DIAGNOSTICS`. Duplicate slider labels removed.
