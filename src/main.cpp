#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCApplication.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/loader/Mod.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <array>
#include <vector>

using namespace geode::prelude;

// ===== Global State =====
std::atomic<bool> g_cbfEnabled = true;
std::atomic<int> g_pollingRate = 1000;
std::atomic<bool> g_threadRunning = false;
std::atomic<bool> g_touchHeld = false;
std::atomic<bool> g_holding = false;
std::thread g_inputThread;

// ===== Counter Settings =====
std::atomic<bool> g_showCounter = false;
std::atomic<float> g_counterScale = 0.5f;
std::atomic<float> g_counterPosX = 0.0f;
std::atomic<float> g_counterPosY = 0.0f;
std::atomic<bool> g_showFps = true;
std::atomic<bool> g_showTps = true;
std::atomic<int> g_counterColor = 0;

// FPS/TPS data
std::atomic<float> g_currentFps = 0.0f;
std::atomic<int> g_currentTps = 0;

std::chrono::steady_clock::time_point g_lastUpdate = std::chrono::steady_clock::now();
int g_frameCount = 0;
int g_inputCount = 0;

CCLabelBMFont* g_counterLabel = nullptr;

constexpr float UPDATE_INTERVAL = 0.5f;

// Color presets
ccColor3B COLOR_WHITE = {255, 255, 255};
ccColor3B COLOR_GREEN = {0, 255, 0};
ccColor3B COLOR_YELLOW = {255, 255, 0};
ccColor3B COLOR_RED = {255, 0, 0};

ccColor3B getColor(int index) {
    switch (index) {
        case 1: return COLOR_GREEN;
        case 2: return COLOR_YELLOW;
        case 3: return COLOR_RED;
        default: return COLOR_WHITE;
    }
}

// ===== Smooth Mode =====
std::atomic<bool> g_smoothMode = true;
std::atomic<int> g_inputQueueSize = 0;

constexpr int QUEUE_SIZE = 16;
struct InputEvent {
    std::chrono::steady_clock::time_point timestamp;
    bool isPress;
};
std::array<InputEvent, QUEUE_SIZE> g_inputQueue;
std::atomic<int> g_queueHead = 0;
std::atomic<int> g_queueTail = 0;

// ===== Input Precision =====
std::atomic<bool> g_trackPrecision = false;
std::atomic<float> g_avgPrecisionMs = 0.0f;
std::atomic<int> g_subFrameClicks = 0;
std::atomic<int> g_totalClicks = 0;

std::vector<std::chrono::microseconds> g_clickOffsets;
std::chrono::steady_clock::time_point g_lastFrameTime;

constexpr int MAX_PRECISION_SAMPLES = 1000;

std::chrono::microseconds calculateSubFrameOffset() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::microseconds>(
        now - g_lastFrameTime
    );
    
    float fps = g_currentFps.load();
    if (fps <= 0) fps = 60.0f;
    auto frameTimeUs = std::chrono::microseconds((int)(1000000.0f / fps));
    
    auto offsetInFrame = timeSinceLastFrame % frameTimeUs;
    auto halfFrame = frameTimeUs / 2;
    if (offsetInFrame < halfFrame) {
        return offsetInFrame;
    } else {
        return frameTimeUs - offsetInFrame;
    }
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

            // Just track the hold state for touch handling
            bool held = g_touchHeld.load();
            g_holding = held;
            g_inputCount++;
            
            // Track precision
            if (g_trackPrecision.load()) {
                auto offset = calculateSubFrameOffset();
                g_clickOffsets.push_back(offset);
                
                if (g_clickOffsets.size() > MAX_PRECISION_SAMPLES) {
                    g_clickOffsets.erase(g_clickOffsets.begin());
                }
                
                long long totalOffset = 0;
                for (auto o : g_clickOffsets) {
                    totalOffset += o.count();
                }
                g_avgPrecisionMs = totalOffset / (float)g_clickOffsets.size() / 1000.0f;
                
                float fps = g_currentFps.load();
                if (fps > 0) {
                    auto halfFrameUs = std::chrono::microseconds((int)(500000.0f / fps));
                    int subFrameCount = 0;
                    for (auto o : g_clickOffsets) {
                        if (o < halfFrameUs) {
                            subFrameCount++;
                        }
                    }
                    g_subFrameClicks = subFrameCount;
                    g_totalClicks = g_clickOffsets.size();
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// ===== CCDirector Hook =====
class $modify(CCDirector) {
    void visit() {
        g_lastFrameTime = std::chrono::steady_clock::now();
        
        CCDirector::visit();
        
        g_frameCount++;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - g_lastUpdate).count();
        
        if (elapsed >= UPDATE_INTERVAL) {
            g_currentFps = g_frameCount / elapsed;
            g_currentTps = g_inputCount / elapsed;
            
            g_frameCount = 0;
            g_inputCount = 0;
            g_lastUpdate = now;
            
            if (g_showCounter.load() && g_counterLabel) {
                char buffer[64];
                if (g_showFps.load() && g_showTps.load()) {
                    sprintf(buffer, "FPS: %.0f | TPS: %d", 
                            g_currentFps.load(), 
                            g_currentTps.load());
                } else if (g_showFps.load()) {
                    sprintf(buffer, "FPS: %.0f", g_currentFps.load());
                } else if (g_showTps.load()) {
                    sprintf(buffer, "TPS: %d", g_currentTps.load());
                } else {
                    buffer[0] = '';
                }
                g_counterLabel->setString(buffer);
                g_counterLabel->setColor(getColor(g_counterColor.load()));
            }
        }
    }
};

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

        // Create counter label
        if (g_counterLabel) {
            g_counterLabel->removeFromParentAndCleanup(true);
        }
        
        char buffer[64];
        if (g_showFps.load() && g_showTps.load()) {
            sprintf(buffer, "FPS: 0 | TPS: 0");
        } else if (g_showFps.load()) {
            sprintf(buffer, "FPS: 0");
        } else if (g_showTps.load()) {
            sprintf(buffer, "TPS: 0");
        } else {
            buffer[0] = '';
        }
        
        g_counterLabel = CCLabelBMFont::create(buffer, "goldFont.fnt");
        g_counterLabel->setScale(g_counterScale.load());
        g_counterLabel->setAnchorPoint(ccp(1.0f, 1.0f));
        g_counterLabel->setPosition(ccp(
            CCDirector::sharedDirector()->getWinSize().width - 10 + g_counterPosX.load(),
            CCDirector::sharedDirector()->getWinSize().height - 10 + g_counterPosY.load()
        ));
        g_counterLabel->setVisible(g_showCounter.load());
        g_counterLabel->setOpacity(200);
        g_counterLabel->setColor(getColor(g_counterColor.load()));
        this->addChild(g_counterLabel, 10000);

        g_frameCount = 0;
        g_inputCount = 0;
        g_lastUpdate = std::chrono::steady_clock::now();

        return true;
    }

    void onExit() {
        g_threadRunning = false;
        if (g_inputThread.joinable()) {
            g_inputThread.join();
        }
        
        if (g_counterLabel) {
            g_counterLabel->removeFromParentAndCleanup(true);
            g_counterLabel = nullptr;
        }
        
        PlayLayer::onExit();
    }
};

// ===== Touch overrides =====
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

// ===== Settings listeners =====
$on_mod(Loaded) {
    listenForSettingChanges<bool>("cbf-enabled", [](bool value) {
        g_cbfEnabled = value;
    });
    listenForSettingChanges<int>("polling-rate", [](int value) {
        g_pollingRate = value;
    });
    listenForSettingChanges<bool>("show-counter", [](bool value) {
        g_showCounter = value;
        if (g_counterLabel) {
            g_counterLabel->setVisible(value);
        }
    });
    listenForSettingChanges<float>("counter-scale", [](float value) {
        g_counterScale = value;
        if (g_counterLabel) {
            g_counterLabel->setScale(value);
        }
    });
    listenForSettingChanges<float>("counter-pos-x", [](float value) {
        g_counterPosX = value;
        if (g_counterLabel) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            g_counterLabel->setPosition(ccp(
                winSize.width - 10 + value,
                winSize.height - 10 + g_counterPosY.load()
            ));
        }
    });
    listenForSettingChanges<float>("counter-pos-y", [](float value) {
        g_counterPosY = value;
        if (g_counterLabel) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            g_counterLabel->setPosition(ccp(
                winSize.width - 10 + g_counterPosX.load(),
                winSize.height - 10 + value
            ));
        }
    });
    listenForSettingChanges<bool>("show-fps", [](bool value) {
        g_showFps = value;
    });
    listenForSettingChanges<bool>("show-tps", [](bool value) {
        g_showTps = value;
    });
    listenForSettingChanges<int>("counter-color", [](int value) {
        g_counterColor = value;
        if (g_counterLabel) {
            g_counterLabel->setColor(getColor(value));
        }
    });
    listenForSettingChanges<bool>("smooth-mode", [](bool value) {
        g_smoothMode = value;
        if (!value) {
            g_queueHead = 0;
            g_queueTail = 0;
            g_inputQueueSize = 0;
        }
    });
    listenForSettingChanges<bool>("track-precision", [](bool value) {
        g_trackPrecision = value;
        if (!value) {
            g_clickOffsets.clear();
            g_avgPrecisionMs = 0.0f;
            g_subFrameClicks = 0;
            g_totalClicks = 0;
        }
    });
}
