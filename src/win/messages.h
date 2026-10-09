#pragma once
#include <windows.h>

namespace rl {
inline constexpr UINT WM_APP_CORSAIR_SESSION = WM_APP + 1;  // wParam = CorsairSessionState
inline constexpr UINT WM_APP_CORSAIR_DEVICE = WM_APP + 2;   // a Corsair device connected
inline constexpr UINT WM_APP_TRAY = WM_APP + 3;             // Shell_NotifyIcon callback
inline constexpr UINT WM_APP_SHOW_PANEL = WM_APP + 4;       // second instance: open panel
inline constexpr UINT WM_APP_RELOAD = WM_APP + 5;           // --set: re-read settings.ini

inline constexpr UINT_PTR kRetryTimerId = 1;     // 10 s device retry
inline constexpr UINT_PTR kThrottleTimerId = 2;  // panel slider flush

inline constexpr UINT IDM_TOGGLE = 101;
inline constexpr UINT IDM_PANEL = 102;
inline constexpr UINT IDM_AUTOSTART = 103;
inline constexpr UINT IDM_QUIT = 104;
}  // namespace rl
