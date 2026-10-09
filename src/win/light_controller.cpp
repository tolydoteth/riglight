#include "win/light_controller.h"

#include <chrono>

#include "core/effects.h"

namespace rl {

LightController::LightController(HWND hwnd) : ram_(hwnd) {}

LightController::~LightController() { shutdown(); }

void LightController::restartClock(const LightState& s) {
    {
        std::lock_guard lk(animMu_);
        state_ = s;
        t0_ = GetTickCount64();
    }
    cv_.notify_all();
}

void LightController::start(const LightState& s) {
    {
        std::lock_guard lk(mbMu_);
        mb_.apply(s);  // reinit happens inside on first use
    }
    ram_.start();
    ram_.apply(s);
    restartClock(s);
    anim_ = std::thread([this] { animLoop(); });
}

void LightController::apply(const LightState& s) {
    {
        std::lock_guard lk(mbMu_);
        mb_.apply(s);
    }
    ram_.apply(s);
    restartClock(s);
}

void LightController::setRamLeadMs(int ms) {
    std::lock_guard lk(animMu_);
    leadMs_ = ms;
}

void LightController::animLoop() {
    std::unique_lock lk(animMu_);
    while (!quit_) {
        if (!isAnimated(state_)) {
            cv_.wait(lk);
            continue;
        }
        const LightState s = state_;
        const uint32_t t = (uint32_t)(GetTickCount64() - t0_);
        const int lead = leadMs_;
        lk.unlock();  // device writes happen without animMu_ so apply() never waits on them
        {
            std::lock_guard mlk(mbMu_);
            mb_.writeFrame(ramColorAt(s, t));
        }
        ram_.writeFrame(s, t + (uint32_t)lead);
        lk.lock();
        cv_.wait_for(lk, std::chrono::milliseconds(timing::kFrameMs));
    }
}

void LightController::onResume() {
    {
        std::lock_guard lk(mbMu_);
        mb_.reinit();  // the controller may have reset to its firmware default
    }
    LightState s;
    {
        std::lock_guard lk(animMu_);
        s = state_;
    }
    {
        std::lock_guard lk(mbMu_);
        mb_.apply(s);
    }
    ram_.onResume();
    restartClock(s);
}

void LightController::onDeviceArrival(const wchar_t* path) {
    if (!Fusion2Device::matchesPath(path)) return;  // other HID devices must not blink the lights
    LightState s;
    {
        std::lock_guard lk(animMu_);
        s = state_;
    }
    std::lock_guard lk(mbMu_);
    mb_.reinit();
    mb_.apply(s);
}

void LightController::onDeviceRemoval(const wchar_t* path) {
    if (!Fusion2Device::matchesPath(path)) return;
    std::lock_guard lk(mbMu_);
    mb_.close();
}

void LightController::onRetryTimer() {
    LightState s;
    {
        std::lock_guard lk(animMu_);
        s = state_;
    }
    {
        std::lock_guard lk(mbMu_);
        if (!mb_.available() && mb_.supportedBoard()) mb_.apply(s);
    }
    if (ram_.needsRetry()) ram_.onRetryTimer();
}

void LightController::shutdown() {
    {
        std::lock_guard lk(animMu_);
        quit_ = true;
    }
    cv_.notify_all();
    if (anim_.joinable()) anim_.join();  // joined before the RAM releases control
    ram_.shutdown();
}

bool LightController::motherboardAvailable() const {
    std::lock_guard lk(mbMu_);
    return mb_.available();
}

}  // namespace rl
