#pragma once
#include "common.hpp"

namespace aimbot {

    inline void restore_headshot_override() {
        // Zero entity memory footprint
    }

    // Evaluates headshot lock
    // Unconditionally locks to head for 100% Red Headshots
    inline bool evaluate_headshot(const PlayerData& target, bool isScoping) {
        (void)target;
        (void)isScoping;
        return true;
    }

} // namespace aimbot
