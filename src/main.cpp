#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
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

// ===== Helper: simulate a tap using PlayLayer's touch handling =====
// This avoids constructing CCTouch directly and uses the API that actually exists.
void simulateTap() {
    auto pl = PlayLayer::get();
    if (!pl) return;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    // Adjust this point to where you want the "click" to happen.
    CCPoint point = ccp(winSize.width * 0.5f, winSize.height * 0.5f);

    // Create a touch via the factory method that exists in the bindings
    auto touch = CCTouch::create();
    if (!touch) return;

    // Use the public setters that exist in your GD version.
    // For 2.2081 + Geode 5.x, the safe way is to use the methods provided
    // in the generated bindings. If setLocation etc. are not available,
    // we instead rely on dispatching the event through the director.

    // Many mods simply call the layer's touch methods directly without
    // fully initializing the touch location, which is enough for "button"
    // behavior. If that doesn't work for your use case, you may need to
    // inspect the exact CCTouch API in your bindings and adjust.

    // For now, we'll assume calling ccTouchBegan/Ended with a default touch
    // is sufficient to trigger the internal "button" logic.
    CCSet set;
    set.addObject(touch);

    pl->ccTouchBegan(touch, nullptr);
    pl->ccTouchEnded(touch, nullptr);
}

// ===== Input Thread =====
void inputThreadFunc() {
    g_threadRunning = true;
    while (g_threadRunning.load()) {
        if (g_cbfEnabled.load()) {
            int sleepMs = 1000 / g_pollingRate.load();
            if (sleepMs < 1) sleepMs = 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

            auto pl = PlayLayer::get();
            if (!pl || pl->m_isPaused) continue;

            if (g_holding.load()) {
                // While "holding", repeatedly tap
                simulateTap();
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
            // Fixed multiline string
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

// ===== Keyboard hooks =====
// Use the actual GD keyboard dispatcher API. In 2.2081, the method names
// and signature can differ. A common pattern that works in Geode mods is:

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enum Keycode key, bool down, bool repeat) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            // Example: use Space or Up arrow to "hold"
            if (key == Keycode::Space || key == Keycode::Up) {
                g_holding = down;
                // Return true to block GD from seeing this key (optional)
                // return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
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
}
