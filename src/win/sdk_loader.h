#pragma once
#include <windows.h>

#include <string>

#include "core/corsair_session.h"

namespace rl {

struct LoadedSdk {
    HMODULE module = nullptr;
    SdkApi api;
    DWORD error = 0;  // GetLastError of the failed load (126 = missing VC++ runtime or DLL)
};

LoadedSdk loadCorsairSdk();  // <exe dir>\iCUESDK.x64_2019.dll
void unloadCorsairSdk(LoadedSdk& sdk);
// Read-only: connects, lists devices, disconnects. Never requests control or sets colors.
std::string corsairProbe();

}  // namespace rl
