#pragma once
#include <Geode/Result.hpp>
#include <cstdint>
#include <string>

namespace clickbeyondframes::checker {

    // Complete runtime snapshot for verification
    struct RuntimeState {
        bool isCBFEnabled;          // Toggle on/off
        int pollingRateHz;          // Current polling rate
        bool isExceedingVanilla;    // true if > 480 Hz
        bool isInputThreadHealthy;  // Background thread running?
        bool isOverriddenByBot;     // Bot temporarily disabled CBF?
        uint64_t uptimeMs;          // How long CBF has been active
    };

    // Get current state for anti-cheat validation
    RuntimeState getState();

    // Cryptographic signature of the state (prevents spoofing)
    std::string getStateSignature(const RuntimeState& state);

    // Basic integrity check (flags debug builds)
    bool verifyIntegrity();

}
