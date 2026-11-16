#pragma once
#include <cstdint>
#include "memory.hpp"

namespace func {
    inline uint64_t find_addr(uint64_t base, const uint64_t* offsets, int count) {
        // Snapshot once: the engine root must be resolved against the module base
        // that was live when the walk started, not one a teardown cleared halfway
        // through the pointer chain.
        const uint64_t libBase = proc::lib.load(std::memory_order_acquire);
        if (libBase == 0) return 0;

        uint64_t addr = libBase + base;

        for (int i = 0; i < count; i++) {
            addr = rpm<uint64_t>(addr); 
            if (addr == 0) return 0;
            addr += offsets[i];
        }
        return addr;
    }
}
