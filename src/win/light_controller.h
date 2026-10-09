#pragma once
#include <windows.h>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "core/anim_clock.h"
#include "core/light_state.h"
#include "win/corsair_ram.h"
#include "win/fusion2_device.h"

namespace rl {

// Fans a LightState out to both devices. Breathing and rainbow are animated here, on one
// thread and one clock, so the board and the RAM can't drift apart (the board's own effect
// timer did).
//
// Locks: devMu_ serialises every device write (board and RAM) from the UI and animation
// threads; animMu_ guards clock_/leadMs_/quit_. Order is devMu_ -> animMu_ (the animation
// thread re-checks the clock generation under both); the UI thread never holds animMu_ while
// taking devMu_. apply() restarts the clock *before* writing, so a frame of the old effect
// is either written before the new state or skipped — never after it.
class LightController {
public:
    explicit LightController(HWND hwnd);
    ~LightController();
    void start(const LightState& s);
    void apply(const LightState& s);
    void setRamLeadMs(int ms);
    void onResume();
    void onDeviceArrival(const wchar_t* path);
    void onDeviceRemoval(const wchar_t* path);
    void onCorsairSession(int state) { ram_.onSessionState(state); }
    void onCorsairDevice() { ram_.onDeviceConnected(); }
    void onRetryTimer();
    void shutdown();
    bool motherboardAvailable() const;
    bool ramAvailable() const { return ram_.available(); }

private:
    void animLoop();
    void restartClock(const LightState& s);
    LightState currentState();

    Fusion2Device mb_;
    CorsairRam ram_;
    mutable std::mutex devMu_;

    std::mutex animMu_;
    std::condition_variable cv_;
    std::thread anim_;
    AnimClock clock_;
    int leadMs_ = 0;
    bool quit_ = false;
};

}  // namespace rl
