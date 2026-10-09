#include "win/corsair_ram.h"

#include "win/log.h"
#include "win/messages.h"

namespace rl {

CorsairRam::CorsairRam(HWND notify) : hwnd_(notify) {}

CorsairRam::~CorsairRam() { shutdown(); }

void CorsairRam::onStateCb(void* ctx, const CorsairSessionStateChanged* e) {
    PostMessageW(static_cast<HWND>(ctx), WM_APP_CORSAIR_SESSION, (WPARAM)e->state, 0);
}

void CorsairRam::onEventCb(void* ctx, const CorsairEvent* e) {
    if (e && e->id == CEI_DeviceConnectionStatusChangedEvent && e->deviceConnectionStatusChangedEvent &&
        e->deviceConnectionStatusChangedEvent->isConnected)
        PostMessageW(static_cast<HWND>(ctx), WM_APP_CORSAIR_DEVICE, 0, 0);
}

void CorsairRam::start() {
    sdk_ = loadCorsairSdk();
    if (!sdk_.module) {  // RAM unavailable (missing DLL or VC++ runtime)
        logLine("iCUE SDK DLL failed to load, error " + std::to_string(sdk_.error));
        return;
    }
    std::lock_guard lk(mu_);
    session_ = std::make_unique<CorsairSession>(sdk_.api);
    session_->setLogger([](const std::string& s) { logLine("ram: " + s); });
    session_->start(&CorsairRam::onStateCb, &CorsairRam::onEventCb, hwnd_);
}

void CorsairRam::apply(const LightState& s) {
    std::lock_guard lk(mu_);
    if (session_) session_->apply(s);
}

void CorsairRam::writeFrame(const LightState& s, uint32_t tMs) {
    std::lock_guard lk(mu_);
    if (session_) session_->writeFrame(s, tMs);
}

void CorsairRam::onSessionState(int state) {
    std::lock_guard lk(mu_);
    if (session_) session_->onSessionState((CorsairSessionState)state);
}

void CorsairRam::onDeviceConnected() {
    std::lock_guard lk(mu_);
    if (session_) session_->onDeviceConnected();
}

void CorsairRam::onResume() {
    std::lock_guard lk(mu_);
    if (session_) session_->onResume();
}

void CorsairRam::onRetryTimer() {
    std::lock_guard lk(mu_);
    if (session_) session_->onRetryTimer();
}

void CorsairRam::shutdown() {
    std::lock_guard lk(mu_);
    if (session_) {
        session_->stop();
        session_.reset();
    }
    unloadCorsairSdk(sdk_);
}

bool CorsairRam::available() const {
    std::lock_guard lk(mu_);
    return session_ && session_->available();
}

bool CorsairRam::needsRetry() const {
    std::lock_guard lk(mu_);
    return session_ && session_->needsRetry();
}

}  // namespace rl
