#include "win/files.h"

#include <windows.h>
#include <shlobj.h>

namespace rl {

std::wstring exeDir() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    return p.substr(0, p.find_last_of(L'\\') + 1);
}

std::wstring settingsPath() {
    PWSTR roaming = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) dir = roaming;
    CoTaskMemFree(roaming);
    dir += L"\\RigLight";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\settings.ini";
}

std::optional<std::string> readTextFile(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::nullopt;
    std::string out;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(h, buf, sizeof buf, &got, nullptr) && got > 0) out.append(buf, got);
    CloseHandle(h);
    return out;
}

bool writeTextFileAtomic(const std::wstring& path, const std::string& text) {
    const std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const bool ok = WriteFile(h, text.data(), (DWORD)text.size(), &wrote, nullptr) && wrote == text.size();
    CloseHandle(h);
    if (!ok) return false;
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

}  // namespace rl
