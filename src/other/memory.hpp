#pragma once

#include <cstdint>
#include <cstring>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include "driver.h"
#include "../protect/oxorany.hpp"
#include "../offsets/offsets.hpp"

// ==============================================================================================
// FILE: src/other/memory.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Low-level kernel memory access bridge (RPM/WPM), raw assembly syscalls, and PID discovery.
//
// CROSS-FILE CALL GRAPH & CONSUMERS:
// ----------------------------------------------------------------------------------------------
// 1. ALL ENGINE SUBSYSTEMS (Universal Memory Layer):
//    - src/game/data.cpp: Uses rpm<T>(), mem_read() to walk Il2Cpp object graphs.
//    - src/func/aim/silent.hpp: Uses wpm<Vector3>() to steer bullet trajectory vectors.
//    - src/func/aim/target.hpp: Uses rpm<bool>() to check player state.
//    - src/main.cpp: Calls get_pid() and get_lib() to attach kernel driver to target game.
//
// 2. ZERO-USERLAND KERNEL DRIVER INVARIANT:
//    - Memory reading and writing are strictly dispatched via the active kernel driver backend
//      (c_driver in driver.h -> KPM / prctl / QX / RT / standard IOCTL).
//    - Zero userland ptrace / process_vm_readv fallbacks are used, preventing ptrace detection.
//
// 3. RAW ASSEMBLY SYSCALL LAYER (sys::sc6):
//    - Bypasses Android Bionic libc wrappers to prevent in-process hooked function interception.
//    - Invokes direct ARM64 assembly kernel transitions (svc #0) using registers x0-x8.
// ==============================================================================================

namespace proc {
    // Written by the render thread (process watchdog in main loop), read by the
    // data and aim worker threads on every read. Atomic keeps the pair coherent
    // per-field, and every read of the pair goes through the snapshot below.
    inline std::atomic<int>      pid{-1};
    inline std::atomic<uint64_t> lib{0};

    struct Snapshot {
        int      pid;   // Target process ID
        uint64_t lib;   // Target libil2cpp.so base virtual address
    };

    // Lock-free atomic snapshot returning coherent pid and libBase
    inline Snapshot snapshot() {
        Snapshot s;
        s.pid = pid.load(std::memory_order_acquire);
        s.lib = lib.load(std::memory_order_acquire);
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
    static constexpr long SYS_openat     = 56;
    static constexpr long SYS_close      = 57;
    static constexpr long SYS_read       = 63;
    static constexpr long SYS_getdents64 = 61;
    #elif defined(__x86_64__)
    static constexpr long SYS_openat     = 257;
    static constexpr long SYS_close      = 3;
    static constexpr long SYS_read       = 0;
    static constexpr long SYS_getdents64 = 217;
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

// STRICT KERNEL DRIVER READ (RPM): Dispatched to active kernel driver module
inline bool mem_read(uint64_t a, void* b, size_t l) {
    if (proc::pid.load(std::memory_order_relaxed) <= 0 || a < Offsets::MinValidAddress || l == 0 || b == nullptr) return false;
    if (!driver || !driver->is_driver_loaded()) {
        memset(b, 0, l);
        return false;
    }
    return driver->read(static_cast<uintptr_t>(a), b, l);
}

// STRICT KERNEL DRIVER WRITE (WPM): Dispatched to active kernel driver module
inline bool mem_write(uint64_t a, const void* b, size_t l) {
    if (proc::pid.load(std::memory_order_relaxed) <= 0 || !is_valid_ptr(a) || l == 0 || b == nullptr) return false;
    if (!driver || !driver->is_driver_loaded()) {
        return false;
    }
    return driver->write(static_cast<uintptr_t>(a), const_cast<void*>(b), l);
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
inline int get_pid() {
    int pfd = sys::openat(AT_FDCWD, oxorany("/proc"), O_RDONLY | O_DIRECTORY);
    if (pfd < 0) return -1;

    char buf[4096];
    int found_global = -1;
    int found_max = -1;

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
                            // Target package matching (Free Fire Global vs Max)
                            if (strstr(cmd, "com.dts.freefireth")) {
                                found_global = pid;
                            } else if (strstr(cmd, "com.dts.freefiremax")) {
                                found_max = pid;
                            }
                            wipe(cmd, sizeof(cmd));
                        }
                    }
                    if (found_global > 0) break;
                }
            }

            p += e->d_reclen;
        }
        if (found_global > 0) break;
    }

    sys::close(pfd);
    wipe(buf, sizeof(buf));

    int target_pid = (found_global > 0) ? found_global : found_max;
    if (target_pid > 0 && driver) {
        driver->initialize(static_cast<pid_t>(target_pid));
    }

    return target_pid;
}

// Queries kernel driver to resolve libil2cpp.so base address in target game virtual memory
inline uint64_t get_lib(const char* targetName = "libil2cpp.so") {
    if (proc::pid.load(std::memory_order_relaxed) <= 0) return 0;

    if (driver && driver->is_driver_loaded()) {
        uintptr_t base = driver->getModuleBase(targetName);
        if (base > 0) return static_cast<uint64_t>(base);
    }

    return 0;
}
