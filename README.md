# RigLight

A tiny Windows tray app that turns a PC's RGB lighting **off and on in one click** — and sets
one color, effect and brightness for everything — without opening the vendors' RGB suites.

Built for one specific machine (see [Supported hardware](#supported-hardware)), but small and
documented enough that you, or a coding agent, can adapt it to yours: see [AGENTS.md](AGENTS.md).

## Why

My PC sits in the room I sleep in, and its RGB is bright. Turning it off meant opening two heavy
apps — Corsair iCUE for the RAM and GIGABYTE Control Center for the motherboard and case
fans — and the Gigabyte one runs elevated, so it can't even be automated. RigLight replaces both
UIs with a light bulb in the tray: left-click for off at night, left-click again in the morning.

- **One click**: lights off / on from the tray. "Off" stays off across reboots.
- **One panel** for everything: 8 color swatches + custom color, Static / Breathing / Rainbow,
  brightness. Changes apply live.
- **In sync**: breathing and rainbow are driven frame-by-frame from one clock for the board *and*
  the RAM, so they pulse together (the board's own hardware effect timer drifts against the RAM).
- **Small**: ~0.5 MB static exe, ~15 MB RAM, no installer, no telemetry, no network.
- **Safe for the board**: never sends the controller's flash-writing commands (save-to-flash,
  persistence, calibration, LED-count) — every write is checked against a deny list first.

## Supported hardware

| Part | How | Status |
|---|---|---|
| GIGABYTE **X870 EAGLE WIFI7** RGB Fusion 2 (ITE IT5711, USB `048D:5711`): ARGB headers, chipset accent, 12 V RGB header | Direct USB HID, own driver ([protocol notes](docs/fusion2-protocol.md)) | Tested |
| Other Gigabyte boards with an IT5711/IT5702/IT8297 Fusion 2 controller | Same protocol, different zone layout | Not supported yet — needs a zone map ([AGENTS.md](AGENTS.md)) |
| **Corsair Vengeance RGB DDR5** RAM | Corsair iCUE SDK v4 (iCUE runs hidden in the background) | Tested |
| Other Corsair iCUE devices (fans, coolers, keyboards…) | Same SDK | Small change: widen the device filter |
| ASUS / MSI / ASRock / NZXT / … | — | Not supported. [OpenRGB](https://openrgb.org) covers these. |

On other boards RigLight leaves the motherboard alone (it checks the BIOS board name) and still
drives Corsair RAM.

## Install (on a supported PC)

1. Build it (below) or take `RigLight.exe` + `iCUESDK.x64_2019.dll` from a build, keep them in
   one folder.
2. Keep **iCUE** installed and running in the background (it already starts with Windows). The
   first time RigLight connects, iCUE asks *"new SDK client RigLight.exe"* — click **Approve**.
   If the RAM brightness in iCUE's device settings is turned down, turn it back up: it scales
   RigLight's colors too.
3. Uninstall **GIGABYTE Control Center** — it fights over the motherboard controller.
4. Windows Settings → Personalization → **Dynamic Lighting** → off (otherwise Windows drives the
   board's LampArray interface too).
5. Run `RigLight.exe`. On first run it adds itself to *Start with Windows* (toggle in the menu).
6. Optional: BIOS → "LEDs in Sleep/Hibernation/Soft Off" → Off, for darkness while the PC sleeps.

## Use

- **Left-click** the tray bulb: lights on / off.
- **Right-click**: *Open panel…*, *Start with Windows*, *Quit*.
- Breathing and rainbow are animated by RigLight, so they run while RigLight runs (solid colors
  and off are held by the hardware). When RigLight quits, iCUE takes the RAM back.

Command line:

```
RigLight.exe --set off | on | static:RRGGBB[:pct] | breathing:RRGGBB[:pct] | rainbow[:pct]
RigLight.exe --probe [file]     read-only device report (sends only "read info" commands)
```

`--set` starts RigLight if it isn't running, otherwise updates the running one.

Files: `%APPDATA%\RigLight\settings.ini` (state; advanced `ram_lead_ms=` shifts the RAM
animation ahead if it ever lags the board) and `%APPDATA%\RigLight\riglight.log`.

## Build

Requirements: Windows 10/11, MinGW-w64 g++ (tested with WinLibs GCC 15.2 UCRT), CMake ≥ 3.20,
Ninja.

```
build.bat
```

This downloads Corsair's iCUE SDK 4.0.84 from its official GitHub release on first build (it has
no redistribution license, so it isn't in this repo), builds, runs the 61 unit tests, and copies
`RigLight.exe` and the SDK DLL to the repo root.

## How it works

- `src/core` — pure, unit-tested logic: light state + settings format, effect math, the Fusion 2
  packet scripts (byte-exact tests), the Corsair session state machine (tested against a fake
  SDK), panel layout, status text.
- `src/win` — Win32: HID access, iCUE SDK loading (MSVC DLL from MinGW via `GetProcAddress`),
  one animation thread for both devices, tray icon, Direct2D panel, sleep/resume and USB
  re-plug handling.
- Design and history: [spec](docs/superpowers/specs/2026-10-09-riglight-design.md),
  [implementation plan](docs/superpowers/plans/2026-10-09-riglight.md),
  [Fusion 2 protocol notes](docs/fusion2-protocol.md).

## Credits and disclaimer

The Gigabyte Fusion 2 protocol knowledge comes from reading [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB)'s
open-source driver — thanks to its contributors. RigLight's code is its own (MIT). Corsair iCUE
SDK © Corsair, downloaded at build time.

Not affiliated with GIGABYTE or Corsair. It talks directly to lighting hardware: use at your own
risk (it never writes controller flash, but no warranty).
