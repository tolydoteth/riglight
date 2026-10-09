#include "win/sdk_loader.h"

#include "win/files.h"

// MinGW and MSVC agree on these layouts (x64: one calling convention, 1-byte bool, 4-byte enums).
static_assert(sizeof(CorsairDeviceInfo) == 396);
static_assert(sizeof(CorsairLedColor) == 8);
static_assert(sizeof(CorsairLedPosition) == 24);
static_assert(sizeof(CorsairSessionStateChanged) == 40);
static_assert(sizeof(CorsairEvent) == 16);
static_assert(sizeof(bool) == 1);

namespace rl {

template <class Fn>
static void bind(HMODULE m, Fn& fn, const char* name) {
    fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(m, name)));
}

LoadedSdk loadCorsairSdk() {
    LoadedSdk s;
    const std::wstring dll = exeDir() + L"iCUESDK.x64_2019.dll";
    s.module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!s.module) {
        s.error = GetLastError();
        return s;
    }
    bind(s.module, s.api.connect, "CorsairConnect");
    bind(s.module, s.api.disconnect, "CorsairDisconnect");
    bind(s.module, s.api.getDevices, "CorsairGetDevices");
    bind(s.module, s.api.getLedPositions, "CorsairGetLedPositions");
    bind(s.module, s.api.subscribeForEvents, "CorsairSubscribeForEvents");
    bind(s.module, s.api.unsubscribeFromEvents, "CorsairUnsubscribeFromEvents");
    bind(s.module, s.api.requestControl, "CorsairRequestControl");
    bind(s.module, s.api.releaseControl, "CorsairReleaseControl");
    bind(s.module, s.api.setLedColors, "CorsairSetLedColors");
    if (!s.api.complete()) {
        s.error = ERROR_PROC_NOT_FOUND;
        unloadCorsairSdk(s);
    }
    return s;
}

void unloadCorsairSdk(LoadedSdk& sdk) {
    if (sdk.module) FreeLibrary(sdk.module);
    sdk.module = nullptr;
    sdk.api = SdkApi{};
}

namespace {
struct ProbeCtx {
    HANDLE connected = nullptr;
    volatile LONG lastState = 0;
};
void probeStateCb(void* ctx, const CorsairSessionStateChanged* e) {
    auto* c = static_cast<ProbeCtx*>(ctx);
    InterlockedExchange(&c->lastState, (LONG)e->state);
    if (e->state == CSS_Connected) SetEvent(c->connected);  // no Corsair* calls in here
}
}  // namespace

std::string corsairProbe() {
    std::string out = "== RAM (iCUE SDK) ==\n";
    LoadedSdk sdk = loadCorsairSdk();
    if (!sdk.module) return out + "SDK DLL failed to load, error " + std::to_string(sdk.error) + "\n";
    ProbeCtx ctx;
    ctx.connected = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    sdk.api.connect(probeStateCb, &ctx);
    if (WaitForSingleObject(ctx.connected, 5000) != WAIT_OBJECT_0) {
        out += "not connected after 5 s (last session state " + std::to_string(ctx.lastState) +
               ") - is iCUE running with SDK enabled?\n";
    } else {
        CorsairDeviceFilter f{};
        f.deviceTypeMask = CDT_All;
        static CorsairDeviceInfo devs[CORSAIR_DEVICE_COUNT_MAX];
        int n = 0;
        sdk.api.getDevices(&f, CORSAIR_DEVICE_COUNT_MAX, devs, &n);
        out += "devices: " + std::to_string(n) + "\n";
        for (int i = 0; i < n; ++i)
            out += "  type=" + std::to_string((unsigned)devs[i].type) + " model=\"" + devs[i].model +
                   "\" leds=" + std::to_string(devs[i].ledCount) + " id=" + devs[i].id + "\n";
    }
    sdk.api.disconnect();
    CloseHandle(ctx.connected);
    unloadCorsairSdk(sdk);
    return out;
}

}  // namespace rl
