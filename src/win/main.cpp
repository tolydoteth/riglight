#include <windows.h>
#include <shellapi.h>

#include <string>

#include "core/settings.h"
#include "win/app.h"
#include "win/autostart.h"
#include "win/files.h"
#include "win/fusion2_device.h"
#include "win/log.h"
#include "win/messages.h"
#include "win/sdk_loader.h"
#include "win/text.h"

using namespace rl;

static int runProbe(const std::wstring& outPath) {
    const std::string text = Fusion2Device::probe() + "\n" + corsairProbe();
    return writeTextFileAtomic(outPath, text) ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring probePath, setSpec;
    bool probe = false, haveSet = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--probe") {
            probe = true;
            if (i + 1 < argc) probePath = argv[++i];
        } else if (a == L"--set" && i + 1 < argc) {
            haveSet = true;
            setSpec = argv[++i];
        }
    }
    LocalFree(argv);
    if (probe) return runProbe(probePath.empty() ? exeDir() + L"riglight-probe.txt" : probePath);

    const std::wstring path = settingsPath();
    const auto text = readTextFile(path);
    const bool firstRun = !text.has_value();
    LightState state = text ? parseSettings(*text) : LightState{};
    if (haveSet) {
        const auto s = parseStateSpec(narrow(setSpec), state);
        if (!s) return 2;
        state = *s;
    }
    if (haveSet || firstRun) writeTextFileAtomic(path, serializeSettings(state, parseRamLeadMs(text.value_or(""))));

    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\RigLight.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND w = FindWindowW(kHiddenClass, nullptr))
            PostMessageW(w, haveSet ? WM_APP_RELOAD : WM_APP_SHOW_PANEL, 0, 0);
        CloseHandle(mutex);
        return 0;
    }
    if (firstRun) setAutostart(true);
    logReset();
    App app(inst);
    const int rc = app.run(state);
    CloseHandle(mutex);
    return rc;
}
