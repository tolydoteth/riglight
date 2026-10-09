#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "core/corsair_session.h"

using namespace rl;

namespace {
struct Fake {
    std::vector<std::string> calls;
    int devices = 2;
    int ledsPerDevice = 11;
    CorsairError controlResult = CE_Success;
    CorsairError setResult = CE_Success;
    int sets = 0;
    CorsairLedColor last{};
    int lastSize = 0;
    std::vector<std::string> held;  // like iCUE: requesting control you already hold fails
    CorsairError setResultDimm1 = CE_Success;
};
Fake g;

CorsairError fConnect(CorsairSessionStateChangedHandler, void*) { g.calls.push_back("connect"); return CE_Success; }
CorsairError fDisconnect() { g.calls.push_back("disconnect"); return CE_Success; }
CorsairError fGetDevices(const CorsairDeviceFilter* f, int sizeMax, CorsairDeviceInfo* d, int* size) {
    g.calls.push_back("getDevices");
    CHECK_EQ(f->deviceTypeMask, (int)CDT_MemoryModule);
    *size = 0;
    for (int i = 0; i < g.devices && i < sizeMax; ++i) {
        d[i] = CorsairDeviceInfo{};
        d[i].type = CDT_MemoryModule;
        std::snprintf(d[i].id, sizeof d[i].id, "{dimm-%d}", i);
        std::snprintf(d[i].model, sizeof d[i].model, "VENGEANCE RGB DDR5");
        d[i].ledCount = g.ledsPerDevice;
        ++*size;
    }
    return CE_Success;
}
CorsairError fGetLedPositions(const char*, int sizeMax, CorsairLedPosition* p, int* size) {
    *size = 0;
    for (int i = 0; i < g.ledsPerDevice && i < sizeMax; ++i) {
        p[i] = CorsairLedPosition{(CorsairLedLuid)(100 + i), 0, 0};
        ++*size;
    }
    return CE_Success;
}
CorsairError fSubscribe(CorsairEventHandler, void*) { g.calls.push_back("subscribe"); return CE_Success; }
CorsairError fUnsubscribe() { g.calls.push_back("unsubscribe"); return CE_Success; }
CorsairError fRequestControl(const char* id, CorsairAccessLevel level) {
    CHECK_EQ((int)level, (int)CAL_ExclusiveLightingControl);
    g.calls.push_back(std::string("request:") + id);
    for (const auto& h : g.held)
        if (h == id) return CE_NoControl;  // observed on the real SDK: re-request while holding
    if (g.controlResult == CE_Success) g.held.push_back(id);
    return g.controlResult;
}
CorsairError fReleaseControl(const char* id) {
    g.calls.push_back(std::string("release:") + id);
    std::erase(g.held, std::string(id));
    return CE_Success;
}
CorsairError fSetLedColors(const char* id, int size, const CorsairLedColor* c) {
    ++g.sets;
    g.lastSize = size;
    if (size > 0) g.last = c[0];
    if (g.setResultDimm1 != CE_Success && std::string(id) == "{dimm-1}") return g.setResultDimm1;
    return g.setResult;
}

SdkApi fakeApi() {
    SdkApi a;
    a.connect = fConnect;
    a.disconnect = fDisconnect;
    a.getDevices = fGetDevices;
    a.getLedPositions = fGetLedPositions;
    a.subscribeForEvents = fSubscribe;
    a.unsubscribeFromEvents = fUnsubscribe;
    a.requestControl = fRequestControl;
    a.releaseControl = fReleaseControl;
    a.setLedColors = fSetLedColors;
    return a;
}

int count(const std::string& prefix) {
    int n = 0;
    for (const auto& c : g.calls) n += c.rfind(prefix, 0) == 0;
    return n;
}

LightState red() {
    LightState s;
    s.color = Rgb{255, 0, 0};
    s.brightness = 100;
    return s;
}
}  // namespace

TEST(corsair_api_complete) {
    CHECK(fakeApi().complete());
    SdkApi a = fakeApi();
    a.setLedColors = nullptr;
    CHECK(!a.complete());
}

TEST(corsair_connect_enumerates_and_takes_control) {
    g = Fake{};
    CorsairSession s(fakeApi());
    CHECK_EQ(s.start(nullptr, nullptr, nullptr), CE_Success);
    CHECK_EQ(count("connect"), 1);
    CHECK(!s.available());
    s.onSessionState(CSS_Connected);
    CHECK(s.connected());
    CHECK(s.available());
    CHECK(!s.needsRetry());
    CHECK_EQ(s.dimms().size(), 2u);
    CHECK_EQ(s.dimms()[0].leds.size(), 11u);
    CHECK_EQ(count("request:"), 2);
    CHECK_EQ(count("subscribe"), 1);
}

TEST(corsair_apply_writes_opaque_colors_to_every_dimm) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    s.apply(red());
    CHECK_EQ(g.sets, 2);
    CHECK_EQ(g.lastSize, 11);
    CHECK_EQ(g.last.r, 255);
    CHECK_EQ(g.last.a, 255);
    LightState off = red();
    off.on = false;
    s.apply(off);
    CHECK_EQ(g.last.r, 0);
    CHECK_EQ(g.last.a, 255);  // black must be opaque
}

TEST(corsair_apply_before_connect_is_written_on_connect) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.apply(red());
    CHECK_EQ(g.sets, 0);
    s.onSessionState(CSS_Connected);
    CHECK_EQ(g.sets, 2);
    CHECK_EQ(g.last.r, 255);
}

TEST(corsair_no_control_then_retry) {
    g = Fake{};
    g.controlResult = CE_NoControl;
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.apply(red());
    s.onSessionState(CSS_Connected);
    CHECK(!s.available());
    CHECK(s.needsRetry());
    CHECK_EQ(g.sets, 0);
    g.controlResult = CE_Success;
    s.onRetryTimer();
    CHECK(s.available());
    CHECK(!s.needsRetry());
    CHECK_EQ(g.sets, 2);
}

TEST(corsair_losing_control_on_write_marks_retry) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    g.setResult = CE_NoControl;
    s.apply(red());
    CHECK(!s.available());
    CHECK(s.needsRetry());
}

TEST(corsair_connection_lost_and_back_reapplies) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    s.apply(red());
    s.onSessionState(CSS_ConnectionLost);
    CHECK(!s.connected());
    CHECK(!s.available());
    CHECK(!s.needsRetry());
    CHECK(s.dimms().empty());
    const int before = g.sets;
    s.onRetryTimer();  // nothing to do while disconnected
    CHECK_EQ(g.sets, before);
    s.onSessionState(CSS_Connected);
    CHECK(s.available());
    CHECK_EQ(g.sets, before + 2);
    CHECK_EQ(count("connect"), 1);  // the SDK reconnects by itself; no second CorsairConnect
}

TEST(corsair_resume_and_device_event_reenumerate_and_reapply) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    LightState off = red();
    off.on = false;
    s.apply(off);
    const int sets = g.sets, gets = count("getDevices");
    s.onResume();
    CHECK_EQ(count("getDevices"), gets + 1);
    CHECK_EQ(g.sets, sets + 2);
    CHECK_EQ(g.last.r, 0);
    s.onDeviceConnected();
    CHECK_EQ(count("getDevices"), gets + 2);
    CHECK_EQ(g.sets, sets + 4);
}

TEST(corsair_device_event_after_connect_keeps_control) {
    // iCUE fires a device-connected event right after subscribing; re-enumerating must not
    // lose the control we already hold.
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    s.onDeviceConnected();
    CHECK(s.available());
    CHECK(!s.needsRetry());
    s.apply(red());
    CHECK_EQ(g.last.r, 255);
}

TEST(corsair_reconnect_after_connection_lost_requests_control_again) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    s.onSessionState(CSS_ConnectionLost);
    g.held.clear();  // iCUE restarted: nobody holds anything
    s.onSessionState(CSS_Connected);
    CHECK(s.available());
}

TEST(corsair_write_errors_log_once_per_dimm) {
    // Review finding: with DIMMs returning different errors, a shared "last error" logged every
    // frame (40 lines/s). Each DIMM tracks its own last error.
    g = Fake{};
    g.setResultDimm1 = CE_InvalidArguments;  // not NoControl, so it stays "controlled"
    CorsairSession s(fakeApi());
    int lines = 0;
    s.setLogger([&](const std::string& l) { lines += l.find("setLedColors") != std::string::npos; });
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    LightState br = red();
    br.effect = Effect::Breathing;
    for (uint32_t t = 0; t < 2000; t += 50) s.writeFrame(br, t);
    CHECK_EQ(lines, 1);  // dimm-1's error, logged once
}

TEST(corsair_write_frame_uses_effect_time) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    LightState br = red();
    br.effect = Effect::Breathing;
    s.writeFrame(br, 0);  // pulse peak
    CHECK_EQ(g.last.r, 255);
    s.writeFrame(br, 1000);  // pulse bottom
    CHECK_EQ(g.last.r, 0);
}

TEST(corsair_stop_releases_and_disconnects) {
    g = Fake{};
    CorsairSession s(fakeApi());
    s.start(nullptr, nullptr, nullptr);
    s.onSessionState(CSS_Connected);
    s.stop();
    CHECK_EQ(count("release:"), 2);
    CHECK_EQ(count("unsubscribe"), 1);
    CHECK_EQ(count("disconnect"), 1);
    CHECK(!s.available());
}
