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

    struct RawEntry {
        int32_t  hashCode;
        int32_t  next;
        uint64_t key;
        uint64_t value;
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

    inline int      getNumValues() const { return count; }

    // Batch read an entire block of 64 entries with a single process_vm_readv syscall
    inline bool read_block(int start_idx, EntryBlock& out_blk) const {
        if (!valid || start_idx < 0 || start_idx >= count) return false;
        size_t to_read = (start_idx + static_cast<int>(kBlockSize) <= count) ?
                         kBlockSize : static_cast<size_t>(count - start_idx);

        RawEntry raw[kBlockSize];
        if (!mem_read(entries_head + start_idx * sizeof(RawEntry), raw, to_read * sizeof(RawEntry))) {
            return false;
        }

        for (size_t b = 0; b < kBlockSize; ++b) {
            if (b < to_read) {
                if (raw[b].hashCode >= 0 && raw[b].key != 0) {
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
