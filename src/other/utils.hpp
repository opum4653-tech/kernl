#pragma once
#include "memory.hpp"
#include <cstdint>

// Dictionary<Byte, Player> layout (IL2CPP 64-bit)
//   +0x18 -> _entries (managed array header)
//   +0x20 -> _count
//   entries[i] = ptr + 0x20 (skip array header) + i * 0x18
//   Entry.value = +0x10 (ReplicationEntity*)
class MonoDictionary {
public:
    static constexpr size_t kBlockSize = 64;

    struct EntryBlock {
        uint64_t value[kBlockSize];
    };

    uint64_t address;
    uint64_t entries_head;
    int      count;
    bool     valid;

    explicit MonoDictionary(uint64_t addr) : address(addr), entries_head(0), count(0), valid(false) {
        if (!addr_valid(addr)) return;

        struct { uint64_t entries; uint64_t count_and_pad; } header{};
        if (!mem_read(addr + 0x18, &header, sizeof(header))) return;

        if (!addr_valid(header.entries)) return;

        entries_head = header.entries + 0x20;               // skip il2cpp array header
        count        = static_cast<int>(header.count_and_pad & 0xFFFFFFFFu);
        valid        = (count > 0 && count < 0x1000);
    }

    inline uint64_t getValues()   const { return entries_head; }
    inline int      getNumValues() const { return count; }

    inline bool read_block(int start_idx, EntryBlock& out_blk) const {
        if (!valid || start_idx < 0 || start_idx >= count) return false;
        for (size_t b = 0; b < kBlockSize; ++b) {
            int idx = start_idx + static_cast<int>(b);
            if (idx >= count) {
                out_blk.value[b] = 0;
                continue;
            }
            out_blk.value[b] = entry(idx);
        }
        return true;
    }

    // ReplicationEntity* de uma entry — validate hashCode & key to eliminate ghost/dead entries
    inline uint64_t entry(int i) const {
        if (!valid || i < 0 || i >= count) return 0;
        int hashCode = rpm<int>(entries_head + i * 0x18);
        if (hashCode < 0) return 0; // Slot is empty or deleted in il2cpp Dictionary
        uint64_t key = rpm<uint64_t>(entries_head + i * 0x18 + 0x08);
        if (!key) return 0;
        return rpm<uint64_t>(entries_head + i * 0x18 + 0x10);
    }
};
