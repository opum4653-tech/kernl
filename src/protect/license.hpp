#pragma once

#include <string>
#include <ctime>

// ==============================================================================================
// FILE: src/protect/license.hpp
// ARCHITECTURE: NinMods Kernel Game Engine (ARM64 / Out-of-Process Native Surface)
// PURPOSE: Cryptographic authorization, token validation, and subscription expiry gate interface.
//
// CROSS-FILE CALL GRAPH & CONSUMERS:
// ----------------------------------------------------------------------------------------------
// 1. CALLER:
//    - src/main.cpp -> check_license_gate(), print_dashboard():
//      * Calls lic::init() to initialize encryption tables.
//      * Calls lic::set_key(argv[i]) or lic::set_key(env_key) to authenticate token.
//      * Calls lic::is_valid() to gate entry into overlay launch loop.
//      * Calls lic::expiry_date() and lic::status_text() to populate startup dashboard telemetry.
//
// 2. IMPLEMENTATION:
//    - src/protect/license.cpp:
//      * Implements HMAC/SHA256 token verification, string decryption (oxorany), and time validation.
// ==============================================================================================

namespace lic {

    // Returns true if a valid, non-expired authorization token is currently authenticated
    bool is_valid();

    // Initializes cryptographic tables and session states
    void init();

    // Sets and validates a candidate authorization key string
    bool set_key(const std::string& key);

    // Returns UNIX timestamp of subscription expiry date (0 if dynamic / perpetual)
    time_t expiry_date();

    // Returns human-readable subscription status string (e.g., "Active", "Trial", "VIP")
    const char* status_text();

} // namespace lic
