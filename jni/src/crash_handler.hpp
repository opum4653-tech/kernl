#pragma once

namespace crash_guard {
    // Installs alternate signal stack and signal interception for SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT.
    void install();
}
