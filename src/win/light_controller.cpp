#include "win/light_controller.h"

#include <chrono>

#include "core/effects.h"

namespace rl {

LightController::LightController(HWND hwnd) : ram_(hwnd) {}

LightController::~LightController() { shutdown(); }

void LightController::restartClock(const LightState& s) {
    {
        std::lock_guard lk(animMu_);
        clock_.restart(s, GetTickCount64());
    }
    cv_.notify_all();
}

LightState LightController::currentState() {
    std::lock_guard lk(animMu_);
    return clock_.state();
}

void LightController::start(const LightState& s) {
    restartClock(s);
    {
        std::lock_guard lk(devMu_);
        mb_.apply(s);  // reinit happens inside on first use
        ram_.start();
        ram_.apply(s);
    }
    anim_ = std::thread([this] { animLoop(); });
}

void LightController::apply(const LightState& s) {
    restartClock(s);  // first: any in-flight frame of the old state is now stale
    std::lock_guard lk(devMu_);
    mb_.apply(s);
    ram_.apply(s);
}

void LightController::setRamLeadMs(int ms) {
    std::lock_guard lk(animMu_);
    leadMs_ = ms;
}

void LightController::animLoop() {
    std::unique_lock lk(animMu_);
    while (!quit_) {
        if (!isAnimated(clock_.state())) {
            cv_.wait(lk);
            continue;
        }
        const AnimClock::Frame f = clock_.snapshot(GetTickCount64());
        const uint32_t lead = (uint32_t)leadMs_;
        lk.unlock();  // device writes happen without animMu_ so apply() never waits on them
        {
            std::lock_guard dlk(devMu_);
            bool current;
            {
                std::lock_guard alk(animMu_);
                current = clock_.isCurrent(f.generation) && !quit_;
            }
            if (current) {
                mb_.writeFrame(ramColorAt(f.state, f.tMs));
                ram_.writeFrame(f.state, f.tMs + lead);
            }
        }
        lk.lock();
        cv_.wait_for(lk, std::chrono::milliseconds(timing::kFrameMs));
    }
}

void LightController::onResume() {
    const LightState s = currentState();
    restartClock(s);
    std::lock_guard lk(devMu_);
    mb_.reinit();  // the controller may have reset to its firmware default
    mb_.apply(s);
    ram_.onResume();
}

void LightController::onDeviceArrival(const wchar_t* path) {
    if (!Fusion2Device::matchesPath(path)) return;  // other HID devices must not blink the lights
    const LightState s = currentState();
    std::lock_guard lk(devMu_);
    mb_.reinit();
    mb_.apply(s);
}

void LightController::onDeviceRemoval(const wchar_t* path) {
    if (!Fusion2Device::matchesPath(path)) return;
    std::lock_guard lk(devMu_);
    mb_.close();
}

void LightController::onRetryTimer() {
    const LightState s = currentState();
    std::lock_guard lk(devMu_);
    if (!mb_.available() && mb_.supportedBoard()) mb_.apply(s);
    if (ram_.needsRetry()) ram_.onRetryTimer();
}

void LightController::shutdown() {
    {
        std::lock_guard lk(animMu_);
        quit_ = true;
    }
    cv_.notify_all();
    if (anim_.joinable()) anim_.join();  // joined before the RAM releases control
    std::lock_guard lk(devMu_);
    ram_.shutdown();
}

bool LightController::motherboardAvailable() const {
    std::lock_guard lk(devMu_);
    return mb_.available();
}

}  // namespace rl
