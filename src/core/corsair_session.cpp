#include "core/corsair_session.h"

#include "core/effects.h"

namespace rl {

bool SdkApi::complete() const {
    return connect && disconnect && getDevices && getLedPositions && subscribeForEvents &&
           unsubscribeFromEvents && requestControl && releaseControl && setLedColors;
}

CorsairSession::CorsairSession(const SdkApi& api) : api_(api) {}

CorsairError CorsairSession::start(CorsairSessionStateChangedHandler onState, CorsairEventHandler onEvent,
                                   void* ctx) {
    onEvent_ = onEvent;
    ctx_ = ctx;
    const CorsairError e = api_.connect(onState, ctx);
    log("connect -> error " + std::to_string((int)e));
    started_ = e == CE_Success;
    return e;
}

void CorsairSession::onSessionState(CorsairSessionState state) {
    log("session state " + std::to_string((int)state));
    if (state == CSS_Connected) {
        connected_ = true;
        subscribed_ = api_.subscribeForEvents(onEvent_, ctx_) == CE_Success;
        enumerate();
        reapply();
    } else {
        connected_ = false;
        subscribed_ = false;
        dimms_.clear();
    }
}

void CorsairSession::enumerate() {
    std::vector<std::string> held;  // control survives re-enumeration; asking again would fail
    for (const Dimm& d : dimms_)
        if (d.controlled) held.push_back(d.id);
    dimms_.clear();
    CorsairDeviceFilter filter{};
    filter.deviceTypeMask = CDT_MemoryModule;
    std::vector<CorsairDeviceInfo> infos(CORSAIR_DEVICE_COUNT_MAX);
    int n = 0;
    const CorsairError ge = api_.getDevices(&filter, (int)infos.size(), infos.data(), &n);
    log("getDevices(memory) -> error " + std::to_string((int)ge) + ", " + std::to_string(n) + " device(s)");
    if (ge != CE_Success) return;
    std::vector<CorsairLedPosition> pos(CORSAIR_DEVICE_LEDCOUNT_MAX);
    for (int i = 0; i < n; ++i) {
        Dimm d;
        d.id = infos[i].id;
        d.model = infos[i].model;
        int count = 0;
        const CorsairError pe = api_.getLedPositions(infos[i].id, (int)pos.size(), pos.data(), &count);
        if (pe == CE_Success) {
            for (int k = 0; k < count; ++k) d.leds.push_back(CorsairLedColor{pos[k].id, 0, 0, 0, 255});
        }
        log("  " + d.model + " " + d.id + ": getLedPositions -> error " + std::to_string((int)pe) + ", " +
            std::to_string(d.leds.size()) + " led(s)");
        bool wasHeld = false;
        for (const auto& id : held) wasHeld = wasHeld || id == d.id;
        if (wasHeld) d.controlled = true;
        else requestControl(d);
        dimms_.push_back(std::move(d));
    }
}

void CorsairSession::requestControl(Dimm& d) {
    CorsairError e = api_.requestControl(d.id.c_str(), CAL_ExclusiveLightingControl);
    if (e == CE_NoControl) {
        // iCUE also answers NoControl when *we* still hold it (e.g. after a short connection
        // drop). Release and ask once more; if another client holds it this still fails.
        api_.releaseControl(d.id.c_str());
        e = api_.requestControl(d.id.c_str(), CAL_ExclusiveLightingControl);
    }
    d.controlled = e == CE_Success;
    log("  requestControl(" + d.id + ") -> error " + std::to_string((int)e));
}

void CorsairSession::writeColor(Rgb c) {
    for (Dimm& d : dimms_) {
        if (!d.controlled || d.leds.empty()) continue;
        for (CorsairLedColor& l : d.leds) {
            l.r = c.r;
            l.g = c.g;
            l.b = c.b;
            l.a = 255;
        }
        const CorsairError e = api_.setLedColors(d.id.c_str(), (int)d.leds.size(), d.leds.data());
        if (e != lastSetError_) log("  setLedColors(" + d.id + ") -> error " + std::to_string((int)e));
        lastSetError_ = e;  // log changes only: this runs at 20 fps while animating
        if (e == CE_NoControl) d.controlled = false;
    }
}

void CorsairSession::reapply() {
    if (haveState_) writeFrame(state_, 0);
}

void CorsairSession::onDeviceConnected() {
    if (!connected_) return;
    enumerate();
    reapply();
}

void CorsairSession::onResume() { onDeviceConnected(); }

void CorsairSession::onRetryTimer() {
    if (!connected_) return;
    bool gained = false;
    for (Dimm& d : dimms_) {
        if (d.controlled) continue;
        requestControl(d);
        gained = gained || d.controlled;
    }
    if (gained) reapply();
}

void CorsairSession::apply(const LightState& s) {
    state_ = s;
    haveState_ = true;
    writeFrame(s, 0);
}

void CorsairSession::writeFrame(const LightState& s, uint32_t tMs) {
    if (!connected_) return;
    writeColor(ramColorAt(s, tMs));
}

void CorsairSession::stop() {
    if (connected_) {
        for (Dimm& d : dimms_)
            if (d.controlled) api_.releaseControl(d.id.c_str());
        if (subscribed_) api_.unsubscribeFromEvents();
    }
    if (started_) api_.disconnect();
    started_ = connected_ = subscribed_ = false;
    dimms_.clear();
}

bool CorsairSession::available() const {
    if (!connected_) return false;
    for (const Dimm& d : dimms_)
        if (d.controlled) return true;
    return false;
}

bool CorsairSession::needsRetry() const {
    if (!connected_) return false;
    for (const Dimm& d : dimms_)
        if (!d.controlled) return true;
    return false;
}

}  // namespace rl
