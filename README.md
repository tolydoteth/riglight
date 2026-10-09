# RigLight

Tray app that turns this PC's RGB lighting (GIGABYTE X870 EAGLE WIFI7 RGB Fusion 2 + Corsair
Vengeance RGB DDR5) off and on in one click, and sets color / effect / brightness for both.

- Left-click the tray bulb: lights on/off. Right-click: menu (panel, start with Windows, quit).
- Breathing and rainbow are animated by RigLight itself (board and RAM on one clock), so they
  only run while RigLight is running. Static colors and off are kept by the hardware.
- Settings: `%APPDATA%\RigLight\settings.ini`. Advanced: `ram_lead_ms=` (0-2000) runs the RAM
  ahead if it ever lags the board. Log: `%APPDATA%\RigLight\riglight.log`.
- Autostart: HKCU Run value `RigLight`.
- `RigLight.exe --probe [file]`: read-only device report.
- `RigLight.exe --set off|on|static:RRGGBB[:pct]|breathing:RRGGBB[:pct]|rainbow[:pct]`
  (starts RigLight if it isn't running, otherwise updates the running one).

Needs iCUE running in the background with SDK access approved for RigLight (RAM), and GIGABYTE
Control Center uninstalled (board). Never writes the board's flash. Build: `build.bat` (MinGW g++
+ CMake + Ninja). Design: `docs/superpowers/specs/2026-10-09-riglight-design.md`; protocol:
`docs/fusion2-protocol.md`.
