# NINMODS KERNEL — Operational & Engineering Rules

This document establishes the mandatory design, architecture, and coding rules for the `ninmods-kernel` native C++ codebase. Every developer and AI agent working on this project must strictly comply with these rules.

---

## 1. Display Resolution & Projection Invariants

1. **Dynamic Resolution Synchronization:**
   - Never assume a fixed screen resolution (e.g. 1920x1080). Modern Android gaming phones have wide aspects (2400x1080, 2340x1080, 2712x1220, 3200x1440).
   - Global projection variables `g_sw` and `g_sh` must strictly reflect the active display viewport:
     - Initialized from `displayInfo` in `main.cpp` (`start_overlay`).
     - Synchronized dynamically in `drawBegin()` whenever orientation changes.
     - Kept in sync with `ImGui::GetIO().DisplaySize` in `ui::menu::render()`.
   - Failing to sync `g_sw` causes the WorldToScreen matrix projection to drift, rendering ESP boxes and tracer lines in empty space off the enemy's body.

2. **ESP Tracer Line Snapping Doctrine:**
   - Tracer lines must snap to logical body anchors depending on their origin:
     - **Top Origin (`tracer_pos == 0`):** Snaps to the top-center (`midX, topY`) of the enemy bounding box (Head).
     - **Center Origin (`tracer_pos == 1`):** Snaps to the chest/pelvis center (`midX, (topY + botY) * 0.5f`) of the enemy.
     - **Bottom Origin (`tracer_pos == 2`):** Snaps to the feet/bottom-center (`midX, botY`) of the enemy.
   - Drawing a bottom tracer line all the way to the head point slices through the player's model and causes awkward visual overlap.

---

## 2. Floating UI & Touch Target Invariants

1. **Floating Logo Visibility & Sizing:**
   - The floating circular 'N' logo must remain prominent and effortlessly touchable on high-DPI screens.
   - Logo base radius must be maintained at **`38.0f * ui_scale`** (diameter ~76px).
   - Never shrink the logo below `35.0f * ui_scale` on modern high-resolution displays.
   - The logo incorporates:
     - Dark disc base (`IM_COL32(18, 14, 26, 245)`)
     - Electric Orchid outer border (`theme_orchid::ColPrimary`)
     - Bold "N" letter sized proportionally (`fontSize = radius * 1.15f`)
     - Status dot showing kernel daemon connectivity (Green = Connected, Red = Disconnected)

2. **Touch Penetration Barrier:**
   - Whenever the menu or floating logo is rendered, the corresponding UI rect must be forwarded to the Touch subsystem:
     `Touch_SetUiRect(x, y, w, h, true)`
   - Touches within the logo or menu bounds must never penetrate into the underlying game canvas.

---

## 3. Kernel Memory & Safety Standards

1. **Ring 0 Kernel-Space Memory Operations:**
   - Memory reading must strictly utilize the kernel driver channel (`driver->read(...)` via IOCTL).
   - Userspace fallbacks (`process_vm_readv` or `/proc/pid/mem`) are strictly prohibited in production builds to prevent detection flags.

2. **Zero Persistent Storage Trace:**
   - The engine must never write temporary configuration, log, or position files to `/data/local/tmp` or `/sdcard`.
   - Logo positions and runtime configurations are preserved strictly in RAM.

3. **Performance & Syscall Minimization:**
   - Utilize batched memory structures (`PlayerSpan`) to extract all player entity fields in a single read rather than making isolated syscalls per field.
   - Dictionary slots must be extracted in blocks of 64 (`MonoDictionary::read_block`).
