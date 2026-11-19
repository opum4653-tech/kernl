#pragma once
#include <cstdint>
#include "memory.hpp"

// ==============================================================================================
// FILE: src/other/addr.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Multi-level pointer chain resolution utility with atomic base address verification.
//
// CROSS-FILE CALL GRAPH:
// ----------------------------------------------------------------------------------------------
// - Relies on: proc::lib (in memory.hpp) and rpm<T>() (in memory.hpp).
// - Resolves pointer chains: Base + offsets[0] -> Read -> + offsets[1] -> Read -> ... -> Destination.
// ==============================================================================================

namespace func {
    // Walks a multi-level pointer chain starting from target module base (libil2cpp.so)
    inline uint64_t find_addr(uint64_t base, const uint64_t* offsets, int count) {
        // Snapshot module base atomically to prevent race condition during teardown
        const uint64_t libBase = proc::lib.load(std::memory_order_acquire);
        if (libBase == 0) return 0;

        uint64_t addr = libBase + base;

        // Sequentially dereference pointer chain
        for (int i = 0; i < count; i++) {
            addr = rpm<uint64_t>(addr); 
            if (addr == 0) return 0;
            addr += offsets[i];
        }
        return addr;
    }
}
