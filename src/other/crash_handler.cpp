#include "crash_handler.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <android/log.h>

static void safe_sig_handler(int sig, siginfo_t* info, void* context) {
    (void)sig;
    (void)info;
    (void)context;
    _exit(0);
}

namespace crash_guard {
    void install() {
        // Allocate dedicated alternate signal stack so stack overflows can be caught safely
        static uint8_t s_alt_stack[SIGSTKSZ * 4];
        stack_t ss{};
        memset(&ss, 0, sizeof(ss));
        ss.ss_sp = s_alt_stack;
        ss.ss_size = sizeof(s_alt_stack);
        ss.ss_flags = 0;
        sigaltstack(&ss, nullptr);

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sa.sa_sigaction = safe_sig_handler;

        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
        sigaction(SIGFPE, &sa, nullptr);
        sigaction(SIGILL, &sa, nullptr);
        sigaction(SIGABRT, &sa, nullptr);
    }
}
