#include "win/app.h"

#include <dbt.h>
#include <windowsx.h>
extern "C" {
#include <hidsdi.h>
}

#include "core/settings.h"
#include "core/status.h"
#include "win/autostart.h"
#include "win/files.h"
#include "win/icon_win.h"
#include "win/messages.h"
#include "win/text.h"

namespace rl {

int App::run(const LightState& initial) {
    state_ = initial;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = &App::wndProc;
    wc.hInstance = inst_;
    wc.lpszClassName = kHiddenClass;
    RegisterClassExW(&wc);
    // A hidden top-level window (not message-only): it must receive WM_POWERBROADCAST and
    // TaskbarCreated broadcasts.
    hwnd_ = CreateWindowExW(0, kHiddenClass, L"RigLight", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, inst_, this);
    if (!hwnd_) return 1;
    taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");

    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof filter;
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HidD_GetHidGuid(&filter.dbcc_classguid);
    devNotify_ = RegisterDeviceNotificationW(hwnd_, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);

    const int iconSize = GetSystemMetrics(SM_CXSMICON);
    iconOn_ = makeBulbIcon(iconSize, true);
    iconOff_ = makeBulbIcon(iconSize, false);
    lights_ = std::make_unique<LightController>(hwnd_);
    ramLeadMs_ = parseRamLeadMs(readTextFile(settingsPath()).value_or(""));
    lights_->setRamLeadMs(ramLeadMs_);
    lights_->start(state_);
    panel_.create(inst_, [this](const LightState& s) { changeState(s); });
    tray_.add(hwnd_, state_.on ? iconOn_ : iconOff_, L"RigLight");
    refreshTray();
    SetTimer(hwnd_, kRetryTimerId, 10000, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (devNotify_) UnregisterDeviceNotification(devNotify_);
    DestroyIcon(iconOn_);
    DestroyIcon(iconOff_);
    return 0;
}

LRESULT CALLBACK App::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app && app->hwnd_) return app->handle(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void App::changeState(const LightState& s) {
    state_ = s;
    state_.brightness = clampBrightness(state_.brightness);
    writeTextFileAtomic(settingsPath(), serializeSettings(state_, ramLeadMs_));
    lights_->apply(state_);
    refreshTray();
}

void App::refreshTray() {
    const std::wstring tip =
        widen(tooltipText(state_, lights_->motherboardAvailable(), lights_->ramAvailable()));
    tray_.update(state_.on ? iconOn_ : iconOff_, tip);
    panel_.update(state_, lights_->motherboardAvailable(), lights_->ramAvailable());
}

void App::showPanel() {
    panel_.show(tray_.iconRect(), state_, lights_->motherboardAvailable(), lights_->ramAvailable());
}

void App::showMenu(POINT pt) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_TOGGLE, state_.on ? L"Turn lights off" : L"Turn lights on");
    AppendMenuW(m, MF_STRING, IDM_PANEL, L"Open panel…");
    AppendMenuW(m, MF_STRING | (autostartEnabled() ? MF_CHECKED : 0), IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_QUIT, L"Quit");
    SetForegroundWindow(hwnd_);
    TrackPopupMenuEx(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(m);
}

void App::quit() {
    if (shutDown_) return;
    shutDown_ = true;
    KillTimer(hwnd_, kRetryTimerId);
    lights_->shutdown();  // stop animation, release control, disconnect
    tray_.remove();
    panel_.destroy();
    DestroyWindow(hwnd_);
}

LRESULT App::handle(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == taskbarCreated_ && taskbarCreated_ != 0) {  // Explorer restarted
        tray_.add(hwnd_, state_.on ? iconOn_ : iconOff_, L"RigLight");
        refreshTray();
        return 0;
    }
    switch (msg) {
        case WM_APP_TRAY:
            switch (LOWORD(lp)) {
                case NIN_SELECT:
                case NIN_KEYSELECT: {
                    LightState s = state_;
                    s.on = !s.on;
                    changeState(s);
                    return 0;
                }
                case WM_CONTEXTMENU:
                    showMenu(POINT{GET_X_LPARAM(wp), GET_Y_LPARAM(wp)});
                    return 0;
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDM_TOGGLE: {
                    LightState s = state_;
                    s.on = !s.on;
                    changeState(s);
                    return 0;
                }
                case IDM_PANEL:
                    showPanel();
                    return 0;
                case IDM_AUTOSTART:
                    setAutostart(!autostartEnabled());
                    return 0;
                case IDM_QUIT:
                    quit();
                    return 0;
            }
            return 0;
        case WM_APP_SHOW_PANEL:
            showPanel();
            return 0;
        case WM_APP_RELOAD:
            if (auto text = readTextFile(settingsPath())) {
                ramLeadMs_ = parseRamLeadMs(*text);
                lights_->setRamLeadMs(ramLeadMs_);
                changeState(parseSettings(*text));
            }
            return 0;
        case WM_APP_CORSAIR_SESSION:
            lights_->onCorsairSession((int)wp);
            refreshTray();
            return 0;
        case WM_APP_CORSAIR_DEVICE:
            lights_->onCorsairDevice();
            refreshTray();
            return 0;
        case WM_TIMER:
            if (wp == kRetryTimerId) {
                lights_->onRetryTimer();
                refreshTray();
            }
            return 0;
        case WM_POWERBROADCAST:
            if (wp == PBT_APMRESUMEAUTOMATIC) {
                lights_->onResume();
                refreshTray();
            }
            return TRUE;
        case WM_DEVICECHANGE: {
            auto* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lp);
            if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                auto* di = reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE_W*>(lp);
                if (wp == DBT_DEVICEARRIVAL) lights_->onDeviceArrival(di->dbcc_name);
                else if (wp == DBT_DEVICEREMOVECOMPLETE) lights_->onDeviceRemoval(di->dbcc_name);
                refreshTray();
            }
            return TRUE;
        }
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wp) quit();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace rl
