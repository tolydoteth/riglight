#include "win/light_controller.h"

namespace rl {

LightController::LightController(HWND hwnd) : ram_(hwnd) {}

void LightController::start(const LightState& s) {
    state_ = s;
    mb_.apply(s);  // reinit happens inside on first use
    ram_.start();
    ram_.apply(s);
}

void LightController::apply(const LightState& s) {
    state_ = s;
    mb_.apply(s);
    ram_.apply(s);
}

void LightController::onResume() {
    mb_.reinit();  // the controller may have reset to its firmware default
    mb_.apply(state_);
    ram_.onResume();
}

void LightController::onDeviceArrival(const wchar_t* path) {
    if (!Fusion2Device::matchesPath(path)) return;  // other HID devices must not blink the lights
    mb_.reinit();
    mb_.apply(state_);
}

void LightController::onDeviceRemoval(const wchar_t* path) {
    if (Fusion2Device::matchesPath(path)) mb_.close();
}

void LightController::onRetryTimer() {
    if (!mb_.available() && mb_.supportedBoard()) mb_.apply(state_);
    if (ram_.needsRetry()) ram_.onRetryTimer();
}

}  // namespace rl
