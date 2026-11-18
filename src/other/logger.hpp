#pragma once

#include <android/log.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstring>
#include <mutex>
#include <string>
#include <chrono>
#include <unordered_map>
#include <unistd.h>
#include <sys/stat.h>

#ifndef LOG_TAG
#define LOG_TAG "ninmods-kernel"
#endif

// ==============================================================================
// MASTER DIAGNOSTIC LOGGING SWITCH (0 = Release Mode / Clean Production)
// Set to 0: Zero file I/O, zero logcat, zero mutex locks, zero performance overhead.
// Set to 1: Diagnostic file logging enabled to /sdcard/Download/kernel_debug.txt.
// Can also be overridden at build time via -DNIN_ENABLE_DEBUG_LOGGING=1
// ==============================================================================
#ifndef NIN_ENABLE_DEBUG_LOGGING
#define NIN_ENABLE_DEBUG_LOGGING 0
#endif

namespace logger {

#if NIN_ENABLE_DEBUG_LOGGING

    class DiagnosticLogger {
    private:
        FILE* fp = nullptr;
        std::mutex mtx;
        std::string active_path = "";
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> throttle_map;
        bool initialized = false;

        void open_file() {
            if (fp) return;

            const char* candidate_paths[] = {
                "/sdcard/Download/kernel_debug.txt",
                "/storage/emulated/0/Download/kernel_debug.txt",
                "/data/local/tmp/kernel_debug.txt"
            };

            for (const char* path : candidate_paths) {
                fp = fopen(path, "a");
                if (fp) {
                    active_path = path;
                    break;
                }
            }

            if (fp) {
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);
                char time_str[64];
                if (t) {
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
                } else {
                    snprintf(time_str, sizeof(time_str), "UNKNOWN_TIME");
                }

                fprintf(fp, "\n================================================================================\n");
                fprintf(fp, "[%s] [INIT] NinMods Kernel Offset Diagnostic Logger Started\n", time_str);
                fprintf(fp, "[%s] [INIT] Log Target Path: %s\n", time_str, active_path.c_str());
                fprintf(fp, "================================================================================\n");
                fflush(fp);
            }
        }

    public:
        static DiagnosticLogger& get() {
            static DiagnosticLogger instance;
            return instance;
        }

        void init() {
            std::lock_guard<std::mutex> lk(mtx);
            open_file();
            initialized = true;
        }

        const char* get_path() const {
            return active_path.c_str();
        }

        bool should_log(const std::string& key, int min_interval_ms = 1000) {
            std::lock_guard<std::mutex> lk(mtx);
            auto now = std::chrono::steady_clock::now();
            auto it = throttle_map.find(key);
            if (it == throttle_map.end()) {
                throttle_map[key] = now;
                return true;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
            if (elapsed >= min_interval_ms) {
                throttle_map[key] = now;
                return true;
            }
            return false;
        }

        void write_entry(const char* tag, const char* fmt, va_list args) {
            char buffer[1024];
            vsnprintf(buffer, sizeof(buffer), fmt, args);

            // Emit to Android logcat
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[%-9s] %s", tag, buffer);

            // Emit to physical file on disk
            std::lock_guard<std::mutex> lk(mtx);
            if (!fp) {
                open_file();
            }

            if (fp) {
                time_t now = time(nullptr);
                struct tm* t = localtime(&now);
                char time_str[32];
                if (t) {
                    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);
                } else {
                    snprintf(time_str, sizeof(time_str), "00:00:00");
                }

                fprintf(fp, "[%s] [%-9s] %s\n", time_str, tag, buffer);
                fflush(fp);
            }
        }

        void log(const char* tag, const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            write_entry(tag, fmt, args);
            va_end(args);
        }

        void offset_status(bool found, const char* name, uintptr_t offset, uint64_t base, uint64_t resolved_val) {
            if (found && resolved_val != 0) {
                log("FOUND", "%-28s : 0x%lX (Offset +0x%lX from Base 0x%lX)", name, (unsigned long)resolved_val, (unsigned long)offset, (unsigned long)base);
            } else {
                log("NOT FOUND", "%-28s : 0x0 (Offset +0x%lX from Base 0x%lX)", name, (unsigned long)offset, (unsigned long)base);
            }
        }
    };

    inline void init() {
        DiagnosticLogger::get().init();
    }

    inline const char* get_path() {
        return DiagnosticLogger::get().get_path();
    }

    inline void log_info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        DiagnosticLogger::get().write_entry("INFO", fmt, args);
        va_end(args);
    }

    inline void log_found(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        DiagnosticLogger::get().write_entry("FOUND", fmt, args);
        va_end(args);
    }

    inline void log_not_found(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        DiagnosticLogger::get().write_entry("NOT FOUND", fmt, args);
        va_end(args);
    }

    inline void log_diag(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        DiagnosticLogger::get().write_entry("DIAG", fmt, args);
        va_end(args);
    }

    inline void log_offset(bool found, const char* name, uintptr_t offset, uint64_t base, uint64_t resolved_val) {
        DiagnosticLogger::get().offset_status(found, name, offset, base, resolved_val);
    }

    inline bool should_log(const std::string& key, int min_interval_ms = 1000) {
        return DiagnosticLogger::get().should_log(key, min_interval_ms);
    }

#else

    // ==============================================================================
    // ZERO-COST PRODUCTION INLINE STUBS (Dead code eliminated by compiler)
    // ==============================================================================
    inline void init() {}
    inline const char* get_path() { return ""; }
    inline void log_info(const char*, ...) {}
    inline void log_found(const char*, ...) {}
    inline void log_not_found(const char*, ...) {}
    inline void log_diag(const char*, ...) {}
    inline void log_offset(bool, const char*, uintptr_t, uint64_t, uint64_t) {}
    constexpr bool should_log(const std::string&, int = 1000) { return false; }

#endif

} // namespace logger

#if NIN_ENABLE_DEBUG_LOGGING
#define LOG_INFO(...)  logger::log_info(__VA_ARGS__)
#define LOG_ERROR(...) logger::log_not_found(__VA_ARGS__)
#define LOG_WARN(...)  logger::log_info(__VA_ARGS__)
#define LOG_DEBUG(...) logger::log_diag(__VA_ARGS__)
#else
#define LOG_INFO(...)  do {} while(0)
#define LOG_ERROR(...) do {} while(0)
#define LOG_WARN(...)  do {} while(0)
#define LOG_DEBUG(...) do {} while(0)
#endif
