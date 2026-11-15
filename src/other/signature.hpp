#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include "driver.h"
#include "memory.hpp"

// ============================================================================
// KERNEL DRIVER PATTERN & SIGNATURE SCANNER
// 100% Pure Kernel Driver Memory Reads — Zero Userland Syscall Scrapes
// Supports standard IDA/AOB signatures with wildcards (e.g. "1F 20 03 D5 ?? ??")
// ============================================================================

namespace kernel::sig {

    struct SigByte {
        uint8_t value;
        bool    is_wildcard;
    };

    inline std::vector<SigByte> parse_signature(const char* pattern) {
        std::vector<SigByte> result;
        if (!pattern) return result;

        std::istringstream ss(pattern);
        std::string byte_str;
        while (ss >> byte_str) {
            if (byte_str == "?" || byte_str == "??") {
                result.push_back({0x00, true});
            } else {
                uint8_t byte_val = static_cast<uint8_t>(strtol(byte_str.c_str(), nullptr, 16));
                result.push_back({byte_val, false});
            }
        }
        return result;
    }

    // Direct kernel-driver pattern scanner across target memory space
    inline uintptr_t scan(uintptr_t start_addr, size_t scan_size, const char* pattern) {
        if (!driver || !driver->is_driver_loaded() || start_addr < 0x10000 || scan_size == 0) {
            return 0;
        }

        auto sig = parse_signature(pattern);
        if (sig.empty() || sig.size() > scan_size) return 0;

        const size_t sig_len = sig.size();
        const size_t chunk_size = 4096;
        const size_t overlap = sig_len - 1;
        std::vector<uint8_t> buffer(chunk_size + overlap, 0);

        size_t scanned = 0;
        while (scanned < scan_size) {
            size_t bytes_to_read = std::min(chunk_size + overlap, scan_size - scanned);
            if (bytes_to_read < sig_len) break;

            uintptr_t current_addr = start_addr + scanned;
            if (!driver->read(current_addr, buffer.data(), bytes_to_read)) {
                scanned += chunk_size;
                continue;
            }

            for (size_t i = 0; i <= bytes_to_read - sig_len; ++i) {
                bool match = true;
                for (size_t j = 0; j < sig_len; ++j) {
                    if (!sig[j].is_wildcard && buffer[i + j] != sig[j].value) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    return current_addr + i;
                }
            }

            scanned += chunk_size;
        }

        return 0;
    }

    // Module-relative signature scan helper via kernel driver
    inline uintptr_t scan_module(const char* module_name, const char* pattern, size_t max_scan = 0x2000000) {
        if (!driver || !driver->is_driver_loaded()) return 0;
        uintptr_t mod_base = driver->getModuleBase(module_name);
        if (!mod_base) return 0;
        return scan(mod_base, max_scan, pattern);
    }

} // namespace kernel::sig
