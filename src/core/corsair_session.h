#pragma once
#include <iCUESDK/iCUESDK.h>

#include <functional>
#include <string>
#include <vector>

#include "core/light_state.h"

namespace rl {

// The iCUE SDK entry points, filled by GetProcAddress (win/sdk_loader) or by a test fake.
struct SdkApi {
    decltype(&CorsairConnect) connect = nullptr;
    decltype(&CorsairDisconnect) disconnect = nullptr;
    decltype(&CorsairGetDevices) getDevices = nullptr;
    decltype(&CorsairGetLedPositions) getLedPositions = nullptr;
    decltype(&CorsairSubscribeForEvents) subscribeForEvents = nullptr;
    decltype(&CorsairUnsubscribeFromEvents) unsubscribeFromEvents = nullptr;
    decltype(&CorsairRequestControl) requestControl = nullptr;
    decltype(&CorsairReleaseControl) releaseControl = nullptr;
    decltype(&CorsairSetLedColors) setLedColors = nullptr;
    bool complete() const;
};

struct Dimm {
    std::string id;
    std::string model;
    std::vector<CorsairLedColor> leds;  // ids from CorsairGetLedPositions
    bool controlled = false;
    CorsairError lastSetError = CE_Success;  // setLedColors errors are logged on change only
};

// Session + exclusive-control lifecycle for the RAM. Not thread-safe: the owner (win/corsair_ram)
// serialises every call with one mutex, and never calls it from inside an SDK callback.
class CorsairSession {
public:
    explicit CorsairSession(const SdkApi& api);
    void setLogger(std::function<void(const std::string&)> log) { log_ = std::move(log); }

    CorsairError start(CorsairSessionStateChangedHandler onState, CorsairEventHandler onEvent, void* ctx);
    void onSessionState(CorsairSessionState state);
    void onDeviceConnected();  // a device (re)appeared: re-enumerate, re-take control, re-apply
    void onResume();           // after sleep: same as onDeviceConnected
    void onRetryTimer();       // re-request control for DIMMs that don't have it
    void apply(const LightState& s);                     // remember + write frame t = 0
    void writeFrame(const LightState& s, uint32_t tMs);  // animation thread, every frame
    void stop();               // release control, unsubscribe, disconnect (exit only)

    bool connected() const { return connected_; }
    bool available() const;
    bool needsRetry() const;
    const std::vector<Dimm>& dimms() const { return dimms_; }

private:
    void enumerate();
    void requestControl(Dimm& d);
    void writeColor(Rgb c);
    void reapply();
    void log(const std::string& s) const {
        if (log_) log_(s);
    }

    SdkApi api_;
    std::function<void(const std::string&)> log_;
    CorsairEventHandler onEvent_ = nullptr;
    void* ctx_ = nullptr;
    bool started_ = false;
    bool connected_ = false;
    bool subscribed_ = false;
    bool haveState_ = false;
    LightState state_;
    std::vector<Dimm> dimms_;
};

}  // namespace rl
