#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCApplication.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/loader/Mod.hpp>

#include <thread>
#include <atomic>
#include <chrono>

using namespace geode::prelude;

// Global State
std::atomic<bool> g_cbfEnabled = true;
std::atomic<int> g_pollingRate = 1000;
std::atomic<bool> g_threadRunning = false;
std::atomic<bool> g_touchHeld = false;
std::thread g_inputThread;

// Counter
std::atomic<bool> g_showCounter = false;
std::atomic<float> g_currentFps = 0.0f;
std::atomic<int> g_currentTps = 0;
std::chrono::steady_clock::time_point g_lastUpdate = std::chrono::steady_clock::now();
int g_frameCount = 0;
int g_inputCount = 0;
CCLabelBMFont* g_counterLabel = nullptr;

// Input Thread
void inputThreadFunc() {
    g_threadRunning = true;
    while (g_threadRunning.load()) {
        if (g_cbfEnabled.load()) {
            int sleepMs = 1000 / g_pollingRate.load();
            if (sleepMs < 1) sleepMs = 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

            auto pl = PlayLayer::get();
            if (!pl || pl->m_isPaused) continue;

            g_inputCount++;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// CCDirector Hook
class $modify(CCDirector) {
    void visit() {
        CCDirector::visit();
        g_frameCount++;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - g_lastUpdate).count();
        
        if (elapsed >= 0.5f) {
            g_currentFps = g_frameCount / elapsed;
            g_currentTps = g_inputCount / elapsed;
            g_frameCount = 0;
            g_inputCount = 0;
            g_lastUpdate = now;
            
            if (g_showCounter.load() && g_counterLabel) {
                char buffer[64];
                sprintf(buffer, "FPS: %.0f | TPS: %d", g_currentFps.load(), g_currentTps.load());
                g_counterLabel->setString(buffer);
            }
        }
    }
};

// PlayLayer
class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool isPractice) {
        if (!PlayLayer::init(level, useReplay, isPractice)) return false;

        if (g_inputThread.joinable()) {
            g_threadRunning = false;
            g_inputThread.join();
        }
        g_threadRunning = true;
        g_inputThread = std::thread(inputThreadFunc);

        if (g_counterLabel) {
            g_counterLabel->removeFromParentAndCleanup(true);
        }
        
        g_counterLabel = CCLabelBMFont::create("FPS: 0 | TPS: 0", "goldFont.fnt");
        g_counterLabel->setScale(0.5f);
        g_counterLabel->setAnchorPoint(ccp(1.0f, 1.0f));
        g_counterLabel->setPosition(ccp(
            CCDirector::sharedDirector()->getWinSize().width - 10,
            CCDirector::sharedDirector()->getWinSize().height - 10
        ));
        g_counterLabel->setVisible(g_showCounter.load());
        this->addChild(g_counterLabel, 10000);

        return true;
    }

    void onExit() {
        g_threadRunning = false;
        if (g_inputThread.joinable()) g_inputThread.join();
        if (g_counterLabel) {
            g_counterLabel->removeFromParentAndCleanup(true);
            g_counterLabel = nullptr;
        }
        PlayLayer::onExit();
    }
};

// Touch overrides
class $modify(CCApplication) {
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            g_touchHeld = true;
            return true;
        }
        return CCApplication::ccTouchBegan(touch, event);
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            g_touchHeld = false;
            return;
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

// Settings
$on_mod(Loaded) {
    listenForSettingChanges<bool>("cbf-enabled", [](bool value) { g_cbfEnabled = value; });
    listenForSettingChanges<int>("polling-rate", [](int value) { g_pollingRate = value; });
    listenForSettingChanges<bool>("show-counter", [](bool value) {
        g_showCounter = value;
        if (g_counterLabel) g_counterLabel->setVisible(value);
    });
}
