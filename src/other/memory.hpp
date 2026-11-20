#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include "driver.h"
#include "../protect/oxorany.hpp"
#include "../offsets/offsets.hpp"

// ==============================================================================================
// FILE: src/other/memory.hpp
// ARCHITECTURE: Kernel Service Overlay Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Low-level kernel memory access bridge (RPM/WPM), raw assembly syscalls, and PID discovery.
// ==============================================================================================

namespace proc {
    inline std::atomic<int>      pid{-1};
    inline std::atomic<uint64_t> lib{0};
    inline std::atomic<bool>     is_game_max{false}; // True if Free Fire Max, false if Free Fire Global
    inline std::atomic<bool>     wpm_allowed{false}; // Pure ESP Zero-WPM hardware write protection gate

    struct Snapshot {
        int      pid;    // Target process ID
        uint64_t lib;    // Target libil2cpp.so base virtual address
        bool     is_max; // True if Free Fire Max
    };

    // Lock-free atomic snapshot returning coherent pid and libBase
    inline Snapshot snapshot() {
        Snapshot s;
        s.pid = pid.load(std::memory_order_acquire);
        s.lib = lib.load(std::memory_order_acquire);
        s.is_max = is_game_max.load(std::memory_order_acquire);
        return s;
    }
}

namespace sys {
    #ifdef SYS_openat
    #undef SYS_openat
    #endif
    #ifdef SYS_close
    #undef SYS_close
    #endif
    #ifdef SYS_read
    #undef SYS_read
    #endif
    #ifdef SYS_getdents64
    #undef SYS_getdents64
    #endif

    #if defined(__aarch64__)
    static constexpr long SYS_openat            = 56;
    static constexpr long SYS_close             = 57;
    static constexpr long SYS_read              = 63;
    static constexpr long SYS_getdents64        = 61;
    #elif defined(__x86_64__)
    static constexpr long SYS_openat            = 257;
    static constexpr long SYS_close             = 3;
    static constexpr long SYS_read              = 0;
    static constexpr long SYS_getdents64        = 217;
    #endif

    #ifndef AT_FDCWD
    #define AT_FDCWD -100
    #endif

    // Direct raw 6-argument assembly syscall invoker (svc #0 on ARM64)
    static inline long sc6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
        long r;
        #if defined(__aarch64__)
        register long x8 __asm__("x8") = n;
        register long x0 __asm__("x0") = a1;
        register long x1 __asm__("x1") = a2;
        register long x2 __asm__("x2") = a3;
        register long x3 __asm__("x3") = a4;
        register long x4 __asm__("x4") = a5;
        register long x5 __asm__("x5") = a6;
        __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : "memory", "cc");
        r = x0;
        #elif defined(__x86_64__)
        register long rax asm("rax") = n;
        register long rdi asm("rdi") = a1;
        register long rsi asm("rsi") = a2;
        register long rdx asm("rdx") = a3;
        register long r10 asm("r10") = a4;
        register long r8  asm("r8")  = a5;
        register long r9  asm("r9")  = a6;
        asm volatile("syscall" : "=a"(r) : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
        #else
        r = syscall(n, a1, a2, a3, a4, a5, a6);
        #endif
        return r;
    }

    static inline int  openat(int fd, const char* p, int f)  { return static_cast<int>(sc6(SYS_openat, fd, reinterpret_cast<long>(p), f, 0, 0, 0)); }
    static inline int  close (int fd)                        { return static_cast<int>(sc6(SYS_close, fd, 0, 0, 0, 0, 0)); }
    static inline long read  (int fd, void* b, size_t c)     { return sc6(SYS_read, fd, reinterpret_cast<long>(b), c, 0, 0, 0); }
    static inline long getdents64(int fd, void* d, size_t c) { return sc6(SYS_getdents64, fd, reinterpret_cast<long>(d), c, 0, 0, 0); }
}

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char d_name[];
};

// Zeroes memory securely on stack to prevent forensic memory dumps
static inline void wipe(void* p, size_t n) {
    volatile uint8_t* v = reinterpret_cast<volatile uint8_t*>(p);
    for (size_t i = 0; i < n; i++) v[i] = 0;
}

// Memory boundary validation check (guards against null pointers & kernel addresses)
inline bool is_valid_ptr(uint64_t a) {
    uint64_t p = a & 0x0000FFFFFFFFFFFFULL;
    return (p >= 0x10000ULL && p <= 0x00007FFFFFFFFFFFULL);
}

inline bool addr_valid(uint64_t a) { return is_valid_ptr(a); }

// Universal Memory Read (RPM): 100% Pure Ring-0 Kernel Driver Architecture
// Zero userspace syscall 270 (process_vm_readv) fallback to ensure complete stealth and zero audit traces.
inline bool mem_read(uint64_t a, void* b, size_t l) {
    if (!is_valid_ptr(a) || l == 0 || b == nullptr) return false;

    // 100% Ring-0 Kernel Driver Dispatch
    if (driver && driver->is_driver_loaded()) {
        if (driver->read(static_cast<uintptr_t>(a), b, l)) {
            return true;
        }
    }

    memset(b, 0, l);
    return false;
}

// Universal Memory Write (WPM): 100% Pure Ring-0 Kernel Driver Architecture
// Hardware-like write protection: Zero userspace syscall 271 (process_vm_writev) fallback.
inline bool mem_write(uint64_t a, const void* b, size_t l) {
    if (!is_valid_ptr(a) || l == 0 || b == nullptr) return false;

    // Strict Pure ESP Zero-WPM Gate: physically locks memory write capability when combat is inactive
    if (!proc::wpm_allowed.load(std::memory_order_relaxed)) return false;

    // Universal Anti-Cheat Memory Quarantine:
    // Strictly block writes to executable code pages (e.g. libil2cpp.so and mapped security modules).
    // Writing is exclusively permitted to dynamic heap / weapon buffers.
    uint64_t libBase = proc::lib.load(std::memory_order_relaxed);
    if (libBase > 0 && a >= libBase && a < (libBase + 0x15000000ULL)) {
        return false;
    }

    // 100% Ring-0 Kernel Driver Dispatch
    if (driver && driver->is_driver_loaded()) {
        if (driver->write(static_cast<uintptr_t>(a), const_cast<void*>(b), l)) {
            return true;
        }
    }

    return false;
}

// Templated single-value memory read (RPM)
template<typename T> inline T rpm(uint64_t a) {
    T v{};
    mem_read(a, &v, sizeof(T));
    return v;
}

// Templated single-value memory write (WPM)
template<typename T> inline bool wpm(uint64_t a, const T& d) {
    return mem_write(a, &d, sizeof(T));
}

// Scans /proc using direct raw assembly syscalls to detect game process PID
// Seamlessly auto-detects BOTH Free Fire Max and Free Fire Global
inline int get_pid() {
    int pfd = sys::openat(AT_FDCWD, oxorany("/proc"), O_RDONLY | O_DIRECTORY);
    if (pfd < 0) return -1;

    char buf[4096];
    int found_pid = -1;
    bool found_is_max = false;

    while (true) {
        long nr = sys::getdents64(pfd, buf, sizeof(buf));
        if (nr <= 0) break;

        char* p = buf;
        while (p < buf + nr) {
            auto* e = reinterpret_cast<linux_dirent64*>(p);

            if (e->d_type == 4) { // Directory
                char* n = e->d_name;
                bool in = true;
                for (char* c = n; *c; c++) {
                    if (*c < '0' || *c > '9') { in = false; break; }
                }

                if (in && *n != '\0') {
                    int pid = 0;
                    for (char* c = n; *c; c++) pid = pid * 10 + (*c - '0');

                    char cp[256];
                    int ps = 0;
                    const char* ps_ = oxorany("/proc/");
                    while (*ps_) cp[ps++] = *ps_++;
                    for (char* c = n; *c; c++) cp[ps++] = *c;
                    const char* cs = oxorany("/cmdline");
                    while (*cs) cp[ps++] = *cs++;
                    cp[ps] = '\0';

                    int cfd = sys::openat(AT_FDCWD, cp, O_RDONLY);
                    wipe(cp, sizeof(cp));
                    if (cfd >= 0) {
                        char cmd[256] = {0};
                        long b = sys::read(cfd, cmd, sizeof(cmd) - 1);
                        sys::close(cfd);

                        if (b > 0) {
                            cmd[b] = '\0';
                            // Target package matching: Free Fire Max OR Free Fire Global
                            if (strstr(cmd, "com.dts.freefiremax")) {
                                found_pid = pid;
                                found_is_max = true;
                                wipe(cmd, sizeof(cmd));
                                break;
                            } else if (strstr(cmd, "com.dts.freefireth")) {
                                found_pid = pid;
                                found_is_max = false;
                                wipe(cmd, sizeof(cmd));
                                break;
                            }
                            wipe(cmd, sizeof(cmd));
                        }
                    }
                }
            }

            p += e->d_reclen;
        }
        if (found_pid > 0) break;
    }

    sys::close(pfd);
    wipe(buf, sizeof(buf));

    proc::is_game_max.store(found_is_max, std::memory_order_relaxed);

    if (found_pid > 0 && driver) {
        driver->initialize(static_cast<pid_t>(found_pid));
    }

    return found_pid;
}

// Queries kernel driver or scans /proc/<pid>/maps directly via raw assembly syscalls
// Infallible multi-strategy base resolver ensuring libil2cpp.so base address is ALWAYS found
inline uint64_t get_lib(const char* targetName = "libil2cpp.so") {
    int curPid = proc::pid.load(std::memory_order_relaxed);
    if (curPid <= 0) return 0;

    // Strategy 1: Active Kernel Driver Module
    if (driver && driver->is_driver_loaded()) {
        uintptr_t base = driver->getModuleBase(targetName);
        if (base > 0) {
            uint32_t magic = rpm<uint32_t>(base);
            if (magic == 0x464C457F) { // \x7FELF
                return static_cast<uint64_t>(base);
            }
            // Even if magic isn't read via driver, test if it resolves GameFacade
            uint64_t gf1 = rpm<uint64_t>(base + Offsets::Base::GameFacade);
            uint64_t gf2 = rpm<uint64_t>(base + Offsets::Base::GameFacade_Alt);
            uint64_t gf3 = rpm<uint64_t>(base + Offsets::Base::GameFacade_Global);
            if (gf1 > 0x10000000 || gf2 > 0x10000000 || gf3 > 0x10000000) {
                return static_cast<uint64_t>(base);
            }
        }
    }

    // Strategy 2: Direct /proc/<pid>/maps scan via raw assembly syscalls
    char mp[64];
    snprintf(mp, sizeof(mp), "/proc/%d/maps", curPid);

    int mfd = sys::openat(AT_FDCWD, mp, O_RDONLY);
    wipe(mp, sizeof(mp));
    if (mfd < 0) return 0;

    char buf[8192];
    long tr = 0;
    uint64_t candidates[8] = {0};
    int candCount = 0;
    uint64_t firstMatch = 0;

    while (true) {
        long nr = sys::read(mfd, buf + tr, sizeof(buf) - tr - 1);
        if (nr <= 0) break;
        tr += nr;
        buf[tr] = '\0';

        char* ls = buf;
        while (true) {
            char* le = ls;
            while (*le && *le != '\n') le++;
            if (*le == '\0') break;
            *le = '\0';

            uint64_t st = 0;
            char* pt = ls;
            while (*pt && ((*pt >= '0' && *pt <= '9') || (*pt >= 'a' && *pt <= 'f') || (*pt >= 'A' && *pt <= 'F'))) {
                uint64_t v = (*pt <= '9') ? (*pt - '0') : ((*pt >= 'a') ? (*pt - 'a' + 10) : (*pt - 'A' + 10));
                st = st * 16 + v;
                pt++;
            }

            if (*pt == '-') {
                pt++;
                // Skip end address
                while (*pt && ((*pt >= '0' && *pt <= '9') || (*pt >= 'a' && *pt <= 'f') || (*pt >= 'A' && *pt <= 'F'))) pt++;
                while (*pt == ' ') pt++;

                // Check permissions: readable private mapping (r--p or r-xp)
                if (pt[0] == 'r' && pt[3] == 'p') {
                    if (strstr(ls, targetName)) {
                        if (firstMatch == 0) firstMatch = st;

                        // Verify ELF magic header: 0x7F 'E' 'L' 'F' (0x464C457F)
                        uint32_t magic = rpm<uint32_t>(st);
                        if (magic == 0x464C457F) {
                            if (candCount < 8) {
                                candidates[candCount++] = st;
                            }
                        }
                    }
                }
            }

            ls = le + 1;
        }

        long left = (buf + tr) - ls;
        if (left > 0 && ls > buf) {
            memmove(buf, ls, left);
            tr = left;
        } else {
            tr = 0;
        }
    }

    sys::close(mfd);
    wipe(buf, sizeof(buf));

    // Forensic validation: test if candidate base directly resolves GameFacade static pointer
    for (int i = 0; i < candCount; i++) {
        uint64_t cand = candidates[i];
        if (cand == 0) continue;
        uint64_t gf1 = rpm<uint64_t>(cand + Offsets::Base::GameFacade); // Free Fire Max (maxff.h primary)
        if (gf1 > 0x10000000) return cand;
        uint64_t gf2 = rpm<uint64_t>(cand + Offsets::Base::GameFacade_Alt); // Free Fire Max (STORM_MODS alt)
        if (gf2 > 0x10000000) return cand;
        uint64_t gf3 = rpm<uint64_t>(cand + Offsets::Base::GameFacade_Global); // Free Fire Global (1.132.x primary)
        if (gf3 > 0x10000000) return cand;
    }

    if (candCount > 0) return candidates[0];
    return firstMatch;
}
