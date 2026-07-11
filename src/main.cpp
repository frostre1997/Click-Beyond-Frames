#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/Mod.hpp>

#include <thread>
#include <atomic>
#include <chrono>

using namespace geode::prelude;

// ===== Global State =====
std::atomic<bool> g_cbfEnabled = true;
std::atomic<int> g_pollingRate = 1000;
std::atomic<bool> g_threadRunning = false;
std::atomic<bool> g_holding = false;
std::thread g_inputThread;

// ===== Input Thread =====
// This version does NOT try to construct CCTouch manually.
// Instead, it just toggles a flag that you can wire to your own input source
// (e.g. a UI button, another mod, or a custom layer you add later).
// If you want "always on", just ignore g_holding and always run the tap logic.
void inputThreadFunc() {
    g_threadRunning = true;
    while (g_threadRunning.load()) {
        if (g_cbfEnabled.load()) {
            int sleepMs = 1000 / g_pollingRate.load();
            if (sleepMs < 1) sleepMs = 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

            auto pl = PlayLayer::get();
            if (!pl || pl->m_isPaused) continue;

            // If you want the mod to always click, just remove the `if (g_holding)` check
            // and always execute the block below.
            if (g_holding.load()) {
                // Placeholder for "do a click".
                // Because CCTouch::create() is not available and constructing
                // CCTouch manually fails, you should implement the actual clicking
                // in a way that matches your GD version's bindings.
                //
                // Common options:
                //   - Hook into an existing button (e.g. play button) and call its callback.
                //   - Add your own CCLayer that receives real touches and forwards them.
                //   - Use a different input source (e.g. a UI toggle) that already
                //     integrates with GD's event system.
                //
                // For now, this is a no-op placeholder that at least compiles.
                // Replace this with your actual click implementation once you know
                // which API your bindings expose for simulating input.
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// ===== PlayLayer Hooks =====
class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool isPractice) {
        if (!PlayLayer::init(level, useReplay, isPractice)) return false;

        if (!Mod::get()->getSavedValue<bool>("cbf-warning-shown")) {
            // Fixed multiline string – all on one line with 

            FLAlertLayer::create(
                "Click Beyond Frames",
                "This mod exceeds RobTop's 480 TPS cap.
"
                "Records above 480 Hz may be rejected on some lists.
"
                "Use at your own discretion.",
                "OK"
            )->show();
            Mod::get()->setSavedValue<bool>("cbf-warning-shown", true);
        }

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

// ===== Settings listeners =====
$on_mod(Loaded) {
    listenForSettingChanges<bool>("cbf-enabled", [](bool value) {
        g_cbfEnabled = value;
    });
    listenForSettingChanges<int>("polling-rate", [](int value) {
        g_pollingRate = value;
    });

    // Example: always hold by default; you can expose this as a setting too.
    g_holding = true;
}
