#pragma once
#include <windows.h>

#include <memory>
#include <mutex>

#include "core/corsair_session.h"
#include "core/light_state.h"
#include "win/sdk_loader.h"

namespace rl {

// Owns the SDK and the session state machine. Public methods are called from the UI thread,
// except writeFrame (LightController's animation thread); one mutex serialises them all.
// SDK callbacks only post WM_APP_CORSAIR_* messages to `notify`.
class CorsairRam {
public:
    explicit CorsairRam(HWND notify);
    ~CorsairRam();
    CorsairRam(const CorsairRam&) = delete;
    CorsairRam& operator=(const CorsairRam&) = delete;

    void start();
    void apply(const LightState& s);
    void writeFrame(const LightState& s, uint32_t tMs);
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

    HWND hwnd_;
    LoadedSdk sdk_;
    std::unique_ptr<CorsairSession> session_;
    mutable std::mutex mu_;
};

}  // namespace rl
