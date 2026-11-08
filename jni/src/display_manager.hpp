#pragma once
#include <cstdint>

struct SafeDisplayInfo {
    int32_t width = 1080;
    int32_t height = 2400;
    uint32_t orientation = 0; // 0=0deg, 1=90deg, 2=180deg, 3=270deg
};

namespace display_safe {
    // Queries screen dimensions safely using zero-fork sysfs and system properties first.
    // Completely isolated from volatile libgui.so binder calls to prevent SIGSEGV.
    // Results are cached in RAM for instant lookup in the render loop.
    SafeDisplayInfo get_info(bool force_refresh = false);
    void update_orientation(uint32_t rot);
}
