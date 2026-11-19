#pragma once
#include "memory.hpp"
#include "../offsets/offsets.hpp"
#include <cstdint>

// ==============================================================================================
// FILE: src/other/utils.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: High-speed batch traversal parser for Il2Cpp System.Collections.Generic.Dictionary.
//
// CROSS-FILE CALL GRAPH & CONSUMERS:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/game/data.cpp -> data::update():
//      * Instantiates MonoDictionary(dictAddr) on Match_PlayerDict (0xC0).
//      * Calls dict.read_block(i, blk) to pull 64 enemy pointers in a single kernel read.
//
// 2. IL2CPP 64-BIT DICTIONARY MEMORY LAYOUT:
//    - Dictionary Object:
//      * +0x18 -> _entries (pointer to Il2Cpp managed array of entries).
//      * +0x20 -> _count (active entry count in dictionary).
//    - Managed Array Header:
//      * +0x00 -> Il2CppClass* / Monitor synchronization header.
//      * +0x18 -> Array bounds / length (int32).
//      * +0x20 -> First element array payload (ArrayHeader = 0x20).
//    - Entry Struct Layout (EntryStride = 0x18 = 24 bytes):
//      * +0x00 (int32_t)  hashCode : Hash of key (negative if slot is unallocated or tombstone).
//      * +0x04 (int32_t)  next     : Index of next entry in bucket chain.
//      * +0x08 (uint64_t) key      : Dictionary Key (e.g., player index or network ID).
//      * +0x10 (uint64_t) value    : Dictionary Value (Il2Cpp pointer to ReplicationEntity*).
//
// 3. PERFORMANCE BATCHING ARCHITECTURE:
//    - Replaces 64 individual remote memory calls with 1 batch read of 1536 bytes (64 * 24 bytes).
// ==============================================================================================

class MonoDictionary {
public:
    static constexpr size_t kBlockSize = 64; // Block size for chunked batch reads

    // Output block buffer holding extracted 64-bit entity pointers
    struct EntryBlock {
        uint64_t value[kBlockSize];
    };

    // Raw 24-byte in-memory representation of an Il2Cpp dictionary slot
    struct RawEntry {
        int32_t  hashCode; // Hashcode (positive = valid occupied slot)
        int32_t  next;     // Linked list next index
        uint64_t key;      // Key payload
        uint64_t value;    // Entity pointer payload (ReplicationEntity)
    };

    uint64_t address;      // Dictionary base address in target memory
    uint64_t entries_head; // Memory pointer to start of entries array elements
    int      count;        // Total element count in dictionary
    bool     valid;        // True if header validation succeeded

    // Constructs and validates dictionary pointers from target memory address
    explicit MonoDictionary(uint64_t addr) : address(addr), entries_head(0), count(0), valid(false) {
        if (!addr_valid(addr)) return;

        // Pull entries pointer and count in a single 16-byte memory read
        struct { uint64_t entries; uint64_t count_and_pad; } header{};
        if (!mem_read(addr + Offsets::Dict::Entries, &header, sizeof(header))) return;

        if (!addr_valid(header.entries)) return;

        // Skip the 32-byte Il2Cpp array header to reach first RawEntry element
        entries_head = header.entries + Offsets::Dict::ArrayHeader;
        count        = static_cast<int>(header.count_and_pad & 0xFFFFFFFFu);

        // Sanity check entry count to reject corrupted dictionary memory
        valid        = (count > 0 && count < 0x1000);
    }

    // Returns total entries in dictionary
    inline int getNumValues() const { return count; }

    // Batch-reads a chunk of 64 entries in a single kernel read call
    inline bool read_block(int start_idx, EntryBlock& out_blk) const {
        if (!valid || start_idx < 0 || start_idx >= count) return false;
        size_t to_read = (start_idx + static_cast<int>(kBlockSize) <= count) ?
                         kBlockSize : static_cast<size_t>(count - start_idx);

        RawEntry raw[kBlockSize];
        // Read full 1536-byte block from target memory
        if (!mem_read(entries_head + start_idx * sizeof(RawEntry), raw, to_read * sizeof(RawEntry))) {
            return false;
        }

        // Filter valid entries (hashCode >= 0 and non-zero key indicate active, occupied slot)
        for (size_t b = 0; b < kBlockSize; ++b) {
            if (b < to_read) {
                if (raw[b].hashCode >= 0 && raw[b].key != 0 && is_valid_ptr(raw[b].value)) {
                    out_blk.value[b] = raw[b].value;
                } else {
                    out_blk.value[b] = 0;
                }
            } else {
                out_blk.value[b] = 0;
            }
        }
        return true;
    }
};
