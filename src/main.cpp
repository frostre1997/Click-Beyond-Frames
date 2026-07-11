#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCApplication.hpp>
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
                    pl->pushButton(0);
                }
            } else if (!held && wasHolding) {
                if (g_holding.compare_exchange_strong(wasHolding, false)) {
                    pl->releaseButton(0);
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
class $modify(CCApplication) {
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            g_touchHeld = true;
            return true; // block the original event
        }
        return CCApplication::ccTouchBegan(touch, event);
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            g_touchHeld = false;
            return; // block the original event
        }
        CCApplication::ccTouchEnded(touch, event);
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            g_touchHeld = false;
            return;
        }
        CCApplication::ccTouchCancelled(touch, event);
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
