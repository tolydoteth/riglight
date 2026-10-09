#include "win/tray.h"

#include <cwchar>

#include "win/messages.h"

namespace rl {

static void setTip(NOTIFYICONDATAW& nid, const std::wstring& tip) {
    wcsncpy(nid.szTip, tip.c_str(), ARRAYSIZE(nid.szTip) - 1);
    nid.szTip[ARRAYSIZE(nid.szTip) - 1] = L'\0';
}

bool Tray::add(HWND hwnd, HICON icon, const std::wstring& tip) {
    nid_ = NOTIFYICONDATAW{};
    nid_.cbSize = sizeof nid_;
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid_.uCallbackMessage = WM_APP_TRAY;
    nid_.hIcon = icon;
    setTip(nid_, tip);
    Shell_NotifyIconW(NIM_DELETE, &nid_);  // stale icon from before a TaskbarCreated, if any
    added_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    if (added_) {
        nid_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    }
    return added_;
}

void Tray::update(HICON icon, const std::wstring& tip) {
    if (!added_) return;
    nid_.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid_.hIcon = icon;
    setTip(nid_, tip);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void Tray::remove() {
    if (added_) Shell_NotifyIconW(NIM_DELETE, &nid_);
    added_ = false;
}

RECT Tray::iconRect() const {
    NOTIFYICONIDENTIFIER id{};
    id.cbSize = sizeof id;
    id.hWnd = nid_.hWnd;
    id.uID = nid_.uID;
    RECT r{};
    if (FAILED(Shell_NotifyIconGetRect(&id, &r))) r = RECT{};
    return r;
}

}  // namespace rl
