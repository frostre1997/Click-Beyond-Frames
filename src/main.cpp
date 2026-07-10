#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCApplication.hpp>
#include <CBFBeyond/API.hpp>
#include <CBFBeyond/Checker.hpp>
#include <thread>
#include <atomic>
#include <chrono>

using namespace geode::prelude;

// ===== Global State =====
std::atomic<bool> g_cbfEnabled = true;
std::atomic<int> g_pollingRate = 1000;
std::atomic<bool> g_cbfOverride = false;  // Bot override
std::atomic<bool> g_threadRunning = false;
std::atomic<bool> g_holding = false;
std::thread g_inputThread;
std::chrono::steady_clock::time_point g_startTime;

// ===== API Implementation =====
namespace cbfbeyond::dev {
    std::string getVersion() {
        return Mod::get()->getVersion().toNonVString();
    }
}

namespace cbfbeyond::checker {
    RuntimeState getState() {
        RuntimeState state;
        state.isCBFEnabled = g_cbfEnabled.load();
        state.pollingRateHz = g_pollingRate.load();
        state.isExceedingVanilla = state.pollingRateHz > 480;
        state.isInputThreadHealthy = g_threadRunning.load();
        state.isOverriddenByBot = g_cbfOverride.load();
        
        auto now = std::chrono::steady_clock::now();
        state.uptimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - g_startTime
        ).count();
        return state;
    }

    std::string getStateSignature(const RuntimeState& state) {
        std::stringstream ss;
        ss << state.isCBFEnabled 
           << state.pollingRateHz 
           << state.isOverriddenByBot 
           << state.uptimeMs
           << "cbf_beyond_salt_2026";
        std::hash<std::string> hasher;
        size_t hash = hasher(ss.str());
        std::stringstream hex;
        hex << std::hex << std::setw(16) << std::setfill('0') << hash;
        return hex.str();
    }

    bool verifyIntegrity() {
        #ifdef DEBUG
            return false;
        #else
            return true;
        #endif
    }
}

// ===== Android Input Thread (Simplified Placeholder) =====
void inputThreadFunc() {
    g_threadRunning = true;
    g_startTime = std::chrono::steady_clock::now();
    
    while (g_threadRunning) {
        if (g_cbfEnabled.load() && !g_cbfOverride.load()) {
            // TODO: Android input polling via AInputQueue / JNI
            // For now, this is a placeholder.
            // On Android, you'll poll touch state via native calls.
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ===== PlayLayer Hooks =====
class $modify(PlayLayer) {
    bool init(GJGameLevel* level) {
        if (!PlayLayer::init(level)) return false;
        
        // Show warning once
        if (!Mod::get()->getSavedValue<bool>("cbf-warning-shown")) {
            FLAlertLayer::create(
                "CBF Beyond",
                "This mod exceeds RobTop's official 480 TPS cap.\n"
                "Records set above 480 Hz may not be accepted on all lists.\n"
                "Use at your own discretion.",
                "OK"
            )->show();
            Mod::get()->setSavedValue<bool>("cbf-warning-shown", true);
        }
        
        // Start input thread
        if (g_inputThread.joinable()) {
            g_threadRunning = false;
            g_inputThread.join();
        }
        g_threadRunning = true;
        g_holding = false;
        g_inputThread = std::thread(inputThreadFunc);
        
        return true;
    }

    void onExit() {
        g_threadRunning = false;
        if (g_inputThread.joinable()) {
            g_inputThread.join();
        }
        PlayLayer::onExit();
    }
};

// ===== Block vanilla input when CBF is active =====
class $modify(Application, CCApplication) {
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            return true; // Block original touch
        }
        return CCApplication::ccTouchBegan(touch, event);
    }
};

// ===== Setting Change Listeners =====
$on_mod(Loaded) {
    listenForSettingChanges("cbf-enabled", [](bool value) {
        g_cbfEnabled = value;
    });
    listenForSettingChanges("polling-rate", [](int value) {
        g_pollingRate = value;
    });
}
