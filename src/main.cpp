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
std::atomic<bool> g_touchHeld = false;
std::atomic<bool> g_holding = false;
std::thread g_inputThread;

// ===== Helper: simulate a tap at a given screen position =====
void simulateTapAt(CCPoint point) {
    auto pl = PlayLayer::get();
    if (!pl) return;

    auto dir = CCDirector::sharedDirector();
    auto glView = dir->getOpenGLView();
    if (!glView) return;

    // Create a touch
    auto touch = new CCTouch();
    touch->m_id = 0;
    touch->setLocation(point);
    touch->setGlobalLocation(glView->convertToGL(point));
    touch->setStartLocation(touch->getGlobalLocation());

    CCSet set;
    set.addObject(touch);

    // Begin
    pl->ccTouchBegan(touch, nullptr);
    // End immediately (tap)
    pl->ccTouchEnded(touch, nullptr);

    touch->release();
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

            bool held = g_touchHeld.load();
            bool wasHolding = g_holding.load();

            if (held && !wasHolding) {
                if (g_holding.compare_exchange_strong(wasHolding, true)) {
                    // Instead of pl->pushButton(0), simulate a tap
                    auto winSize = CCDirector::sharedDirector()->getWinSize();
                    // Adjust this position to match where your "button" is
                    CCPoint clickPos = ccp(winSize.width * 0.5f, winSize.height * 0.5f);
                    simulateTapAt(clickPos);
                }
            } else if (!held && wasHolding) {
                if (g_holding.compare_exchange_strong(wasHolding, false)) {
                    // For a simple tap we don't need a separate "release" action.
                    // If you need a held-down behavior, you'd implement that differently
                    // (e.g. keep calling simulateTapAt periodically while held).
                }
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

// ===== Touch overrides =====
// Do NOT modify CCApplication for touches. Instead, hook input at a higher level
// (e.g. CCKeyboardDispatcher, CDDLDispatcher, or your own layer). For a minimal fix,
// we remove the CCApplication modify entirely and just rely on g_touchHeld being set
// from elsewhere (e.g. a custom layer or keyboard hook). If you already have a layer
// handling touches/keys, wire g_touchHeld there.

// Example: if you want to hook keyboard (often used for click-to-play mods):
#include <Geode/modify/CCKeyboardDispatcher.hpp>

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyDown(enumKey key, bool repeat) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            // Example: treat space / up / mouse click key as "hold"
            // Adjust key handling to your needs.
            if (key == KEY_Space || key == KEY_Up) {
                g_touchHeld = true;
                // Don't block the original event unless you want to
                // return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyDown(key, repeat);
    }

    bool dispatchKeyUp(enumKey key, bool repeat) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            if (key == KEY_Space || key == KEY_Up) {
                g_touchHeld = false;
                // return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyUp(key, repeat);
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
