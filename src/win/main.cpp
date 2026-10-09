#include <windows.h>
#include <shellapi.h>

#include <string>

#include "win/files.h"
#include "win/fusion2_device.h"

using namespace rl;

static int runProbe(const std::wstring& outPath) {
    const std::string text = Fusion2Device::probe();
    return writeTextFileAtomic(outPath, text) ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int rc = 0;
    if (argc >= 2 && std::wstring(argv[1]) == L"--probe")
        rc = runProbe(argc >= 3 ? std::wstring(argv[2]) : exeDir() + L"riglight-probe.txt");
    LocalFree(argv);
    return rc;
}
