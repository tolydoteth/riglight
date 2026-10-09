#pragma once
#include <windows.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "core/corsair_session.h"
#include "core/light_state.h"
#include "win/sdk_loader.h"

namespace rl {

// Owns the SDK, the session state machine and the animation thread. Every public method runs
// on the UI thread; SDK callbacks only post WM_APP_CORSAIR_* messages to `notify`.
class CorsairRam {
public:
    explicit CorsairRam(HWND notify);
    ~CorsairRam();
    CorsairRam(const CorsairRam&) = delete;
    CorsairRam& operator=(const CorsairRam&) = delete;

    void start();
    void apply(const LightState& s);
    void onSessionState(int state);
    void onDeviceConnected();
    void onResume();
    void onRetryTimer();
    void shutdown();
    bool available() const;
    bool needsRetry() const;

private:
    static void onStateCb(void* ctx, const CorsairSessionStateChanged* e);
    static void onEventCb(void* ctx, const CorsairEvent* e);
    void animLoop();

    HWND hwnd_;
    LoadedSdk sdk_;
    std::unique_ptr<CorsairSession> session_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::thread anim_;
    bool quit_ = false;
    LightState state_;
    ULONGLONG t0_ = 0;
};

}  // namespace rl
