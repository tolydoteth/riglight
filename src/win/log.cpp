#include "win/log.h"

#include <windows.h>

#include <cstdio>
#include <mutex>

#include "win/files.h"

namespace rl {

static std::mutex g_logMu;

static std::wstring logPath() {
    std::wstring p = settingsPath();
    return p.substr(0, p.find_last_of(L'\\') + 1) + L"riglight.log";
}

static void append(const std::string& text, bool truncate) {
    std::lock_guard lk(g_logMu);
    HANDLE h = CreateFileW(logPath().c_str(), truncate ? GENERIC_WRITE : FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           truncate ? CREATE_ALWAYS : OPEN_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(h, text.data(), (DWORD)text.size(), &wrote, nullptr);
    CloseHandle(h);
}

void logReset() { append("", true); }

void logLine(const std::string& line) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[32];
    std::snprintf(stamp, sizeof stamp, "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    append(stamp + line + "\r\n", false);
}

}  // namespace rl
