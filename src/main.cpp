#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCApplication.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <android/input.h>
#include <android/looper.h>
#include <jni.h>

using namespace geode::prelude;

// ===== Global State =====
std::atomic<bool> g_cbfEnabled = true;
std::atomic<int> g_pollingRate = 1000;
std::atomic<bool> g_cbfOverride = false;
std::atomic<bool> g_threadRunning = false;
std::atomic<bool> g_holding = false;
std::thread g_inputThread;
std::chrono::steady_clock::time_point g_startTime;
static AInputQueue* g_inputQueue = nullptr;

// ===== JNI helper =====
AInputQueue* getInputQueueFromActivity() {
    JNIEnv* env = geode::android::getEnv();
    if (!env) return nullptr;
    jclass activityClass = env->FindClass("android/app/NativeActivity");
    jmethodID getQueue = env->GetMethodID(activityClass, "getInputQueue", "()Landroid/view/InputQueue;");
    jobject queueObj = env->CallObjectMethod(geode::android::getActivity(), getQueue);
    if (!queueObj) return nullptr;
    return AInputQueue_fromJava(env, queueObj);
}

// ===== Input Thread =====
void inputThreadFunc() {
    g_threadRunning = true;
    g_startTime = std::chrono::steady_clock::now();
    g_inputQueue = getInputQueueFromActivity();
    if (!g_inputQueue) {
        log::error("Click Beyond Frames: Failed to get Android input queue!");
        g_threadRunning = false;
        return;
    }
    while (g_threadRunning) {
        if (g_cbfEnabled.load() && !g_cbfOverride.load()) {
            int sleepMs = 1000 / g_pollingRate.load();
            if (sleepMs < 1) sleepMs = 1;
            ALooper_pollAll(sleepMs, nullptr, nullptr, nullptr);
            AInputEvent* event = nullptr;
            while (AInputQueue_getEvent(g_inputQueue, &event) >= 0) {
                if (AInputQueue_preDispatchEvent(g_inputQueue, event)) {
                    continue;
                }
                int32_t type = AInputEvent_getType(event);
                if (type == AINPUT_EVENT_TYPE_MOTION) {
                    int32_t action = AMotionEvent_getAction(event);
                    int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
                    if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                        if (!g_holding.exchange(true)) {
                            auto pl = PlayLayer::get();
                            if (pl && !pl->m_isPaused) {
                                pl->pushButton(0);
                            }
                        }
                    } else if (actionMasked == AMOTION_EVENT_ACTION_UP || actionMasked == AMOTION_EVENT_ACTION_POINTER_UP) {
                        if (g_holding.exchange(false)) {
                            auto pl = PlayLayer::get();
                            if (pl) {
                                pl->releaseButton(0);
                            }
                        }
                    }
                }
                AInputQueue_finishEvent(g_inputQueue, event, 0);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// ===== PlayLayer Hooks =====
class $modify(PlayLayer) {
    bool init(GJGameLevel* level) {
        if (!PlayLayer::init(level)) return false;
        if (!Mod::get()->getSavedValue<bool>("cbf-warning-shown")) {
            FLAlertLayer::create(
                "Click Beyond Frames",
                "This mod exceeds RobTop's 480 TPS cap.\n"
                "Records above 480 Hz may be rejected on some lists.\n"
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

// ===== Block vanilla touch events =====
class $modify(Application, CCApplication) {
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
        if (g_cbfEnabled.load() && PlayLayer::get()) {
            return true;
        }
        return CCApplication::ccTouchBegan(touch, event);
    }
};

// ===== Settings listeners =====
$on_mod(Loaded) {
    listenForSettingChanges("cbf-enabled", [](bool value) {
        g_cbfEnabled = value;
    });
    listenForSettingChanges("polling-rate", [](int value) {
        g_pollingRate = value;
    });
}
