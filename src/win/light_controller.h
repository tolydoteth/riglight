#pragma once
#include <windows.h>

#include "core/light_state.h"
#include "win/corsair_ram.h"
#include "win/fusion2_device.h"

namespace rl {

class LightController {
public:
    explicit LightController(HWND hwnd);
    void start(const LightState& s);
    void apply(const LightState& s);
    void onResume();
    void onDeviceArrival(const wchar_t* path);
    void onDeviceRemoval(const wchar_t* path);
    void onCorsairSession(int state) { ram_.onSessionState(state); }
    void onCorsairDevice() { ram_.onDeviceConnected(); }
    void onRetryTimer();
    void shutdown() { ram_.shutdown(); }
    bool motherboardAvailable() const { return mb_.available(); }
    bool ramAvailable() const { return ram_.available(); }

private:
    Fusion2Device mb_;
    CorsairRam ram_;
    LightState state_;
};

}  // namespace rl
