#pragma once
#include <windows.h>

#include <memory>

#include "core/light_state.h"
#include "win/light_controller.h"
#include "win/panel.h"
#include "win/tray.h"

namespace rl {

inline constexpr const wchar_t* kHiddenClass = L"RigLightHidden";

class App {
public:
    explicit App(HINSTANCE inst) : inst_(inst) {}
    int run(const LightState& initial);

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void changeState(const LightState& s);
    void refreshTray();
    void showMenu(POINT pt);
    void showPanel();
    void quit();

    HINSTANCE inst_;
    HWND hwnd_ = nullptr;
    UINT taskbarCreated_ = 0;
    HDEVNOTIFY devNotify_ = nullptr;
    LightState state_;
    std::unique_ptr<LightController> lights_;
    Tray tray_;
    Panel panel_;
    HICON iconOn_ = nullptr;
    HICON iconOff_ = nullptr;
    bool shutDown_ = false;
};

}  // namespace rl
