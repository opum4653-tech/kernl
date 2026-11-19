#pragma once

#include <android/log.h>
#include <cstdint>
#include <string>

#ifndef LOG_TAG
#define LOG_TAG "kworker"
#endif

// ==============================================================================
// KERNEL TELEMETRY & LOGGING INTERFACE (100% Volatile RAM / Zero Disk I/O)
// Pure in-memory streaming: zero file I/O, zero log persistence, zero trace.
// ==============================================================================

namespace logger {

    inline void init() {}
    inline const char* get_path() { return ""; }
    inline void log_info(const char*, ...) {}
    inline void log_found(const char*, ...) {}
    inline void log_not_found(const char*, ...) {}
    inline void log_diag(const char*, ...) {}
    inline void log_offset(bool, const char*, uintptr_t, uint64_t, uint64_t) {}
    constexpr bool should_log(const std::string&, int = 1000) { return false; }

} // namespace logger

#define LOG_INFO(...)  do {} while(0)
#define LOG_ERROR(...) do {} while(0)
#define LOG_WARN(...)  do {} while(0)
#define LOG_DEBUG(...) do {} while(0)
