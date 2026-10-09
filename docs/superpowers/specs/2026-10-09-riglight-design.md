# RigLight — design

Date: 2026-10-09
Status: design approved in chat; spec reviewed by Fable (2026-10-09), review applied

## Purpose

The user sleeps in the same room as the PC and the RGB lighting is too bright at night.
Today the only way to change it is through two heavy vendor apps (Corsair iCUE and
GIGABYTE Control Center), and GCC runs elevated so it can't even be automated. RigLight
replaces both UIs with one small tray app: one click turns the PC lights off or on, and a
small panel picks color, effect and brightness for everything at once.

Success looks like:
- Lights off/on in one click from the tray, no vendor UI ever opened.
- Color / effect / brightness changes apply to the motherboard (incl. ARGB headers / case
  lighting) and the RAM together, and look close to in sync.
- After a reboot + login, or a sleep/resume, the lights come back in the last chosen
  state (off stays off).
- GCC can be uninstalled; iCUE stays installed but is never opened.

## What the user said vs. what is assumed

Said: control PC + RAM lights easily; scope = on/off + brightness + colors & effects;
keep iCUE hidden in the background, remove GCC.

Assumed (correct me): Windows only; starts with Windows; same stack/style as Keyflip
(C++20, Win32, MinGW static exe, tray app); visual style = the "Clear" look of the user's
games (functional, flat, no ornament).

## Hardware (this PC)

| Device | Path | How RigLight talks to it |
|---|---|---|
| GIGABYTE X870 EAGLE WIFI7 RGB Fusion 2 controller, ITE USB `048D:5711` | HID, MI_01 / COL02 | Own driver: HID feature reports, protocol ported from OpenRGB (see `docs/fusion2-protocol.md`). No admin. |
| Corsair Vengeance RGB DDR5 ×2 (CMH32GX5M2B6000C38) | SMBus, owned by iCUE | Corsair iCUE SDK v4.0.84 (`third_party/iCUESDK`), exclusive lighting control. |

Out of scope: GPU, fans' speed, Logitech/SteelSeries/Keychron peripherals.

## User-facing behaviour

**Tray icon** (bulb glyph; dimmed glyph when lights are off).
- Left-click: toggle lights on/off.
- Right-click menu: `Lights on/off`, `Open panel…`, `Start with Windows` ✓, `Quit`.
- Tooltip: `RigLight — On · Rainbow · 60%` or `RigLight — Off`; appends
  `· RAM unavailable` / `· Motherboard unavailable` when a device can't be reached.

**Panel** — small borderless popup anchored above the tray (like a Windows flyout),
closes when it loses focus. Contents top to bottom:
1. Power toggle (On/Off).
2. Color: 8 swatches (warm white, white, red, orange, green, cyan, blue, purple)
   + "Custom…" (Windows ChooseColor dialog). Disabled while effect = Rainbow.
3. Effect: segmented control `Static | Breathing | Rainbow`.
4. Brightness: slider 5–100 % (the minimum may be raised after the live check if the
   hardware floor turns out higher).
5. Footer: device status (`Motherboard ✓ · RAM ✓` or what is unavailable).
Every change applies immediately (no Apply button) and is saved.

**Persistence**: `%APPDATA%\RigLight\settings.ini`: `on`, `color` (hex RRGGBB),
`effect` (static|breathing|rainbow), `brightness` (5–100). Missing/garbled values fall
back to defaults (on, warm white, static, 60 %). Autostart is not in the ini: the HKCU
`Run` value is the single source of truth.

**Startup**: HKCU `Run` entry when `Start with Windows` is checked (default on at first
run). On launch the saved state is applied to every device. Single instance (named
mutex); a second launch opens the panel of the running instance.

**Before login / while asleep (accepted limitation)**: RigLight is a per-user app, so
between boot and login the board shows its firmware default and the DIMMs show iCUE's
hardware-lighting profile. In S3 sleep the board runs LEDs from standby power. Both are
handled by one-time setup (BIOS "LEDs in sleep/off" = off, iCUE hardware lighting =
off), not by the app. No flash writes from the app (see Flash wear).

## Architecture

One process, small units with one job each:

```
src/
  core/   (no Windows calls; unit-tested)
    LightState      struct {on, color, effect, brightness} + defaults
    Settings        LightState <-> ini text
    Effects         pure math: ramColorAt(state, t_ms) for the RAM's software effects,
                    timing constants shared with Fusion2Script
    Fusion2Report   parses the 64-byte CC 60 info report (product, fw, LED counts,
                    support_cmd_flag)
    Fusion2Script   pure: init(report) -> vector<Step>, apply(state) -> vector<Step>;
                    Step = {bytes[64], readAfter, waitMs}. Holds the board zone map.
    CorsairSession  pure state machine over a SdkApi struct of function pointers:
                    session state × control state × retry; decides when to enumerate,
                    request control, re-apply. Tested with a fake SdkApi.
  win/
    Hid             SetupAPI/HID: find 048D:5711 MI_01 COL02, open, SetFeature/GetFeature
    Fusion2Device   runs Fusion2Script steps through a tiny send/recv/sleep interface
    SdkLoader       LoadLibraryExW(exe dir\iCUESDK.x64_2019.dll), GetProcAddress -> SdkApi
    CorsairRam      owns CorsairSession + the animation thread
    LightController owns both devices; apply(LightState) fans out; availability
    Tray            Shell_NotifyIcon, menu, tooltip
    Panel           Direct2D/DirectWrite popup drawing + hit testing
    Autostart       HKCU Run read/write
    BoardId         reads HKLM\HARDWARE\DESCRIPTION\System\BIOS\BaseBoardProduct
    main.cpp        WinMain, single-instance, message loop, power/device notifications
tests/            core unit tests (CTest), same check.h style as Keyflip
third_party/iCUESDK  Corsair SDK v4.0.84 (header, DLL; DLL copied next to the exe)
```

Interfaces: every device implements `apply(const LightState&)`, `reinit()` and
`available() const`. LightController doesn't know protocol details. UI only ever changes
LightState and calls `controller.apply(state)` + `settings.save(state)`.

### Effect mapping

Brightness `pct` is 5–100. `b255 = round(pct * 255 / 100)`.

| LightState | Motherboard: per mapped zone | RAM: every LED (a = 255 always) |
|---|---|---|
| off | effect 1 (static), max_brightness 0, color 000000, periods 0 | 000000, exclusive control held, written once |
| static | effect 1, max_brightness `b255`, color unscaled, periods 0 | color × pct, written once |
| breathing | effect 2 (pulse), max_brightness **`pct`** (0–100, controller quirk cap), min 0, color, period0 = period1 = 1000 ms, period2 = 200 ms | software pulse: linear rise 1000 ms, linear fall 1000 ms, hold at minimum 200 ms; color × pct × curve |
| rainbow | effect 4 (color cycle), max_brightness `b255`, period0 = 700 ms, period1 = 500 ms, effect_param0 = 7 | software: steps through 7 evenly spaced hues, 700 ms fade + 500 ms hold per step, × pct |

The pulse brightness scale is one named constant in Fusion2Script so the live check can
flip it if 0–100 turns out wrong. The rainbow's 7-hue order on the RAM is a guess at the
hardware's sequence; adjust by eye at the live check. Both sides start in phase on each
apply; long-term phase lock is not a goal.

After the zone packets: `CC 28 F4 00 00 00` (OR of the five zone masks).

ARGB headers (case fans/strips) run the same hardware effect as onboard zones — no
per-LED streaming: init sends `CC 32 00` so every header uses its hardware effect.

### Fusion2 specifics (from `docs/fusion2-protocol.md`)

- Open the HID collection on **MI_01 / COL02** (usage page 0xFF89, usage 0xCC); all
  packets are 64-byte feature reports starting with report ID 0xCC.
- Init script (normal): `CC 60` + read info report; `CC 48 00` only if the report's
  support_cmd_flag LampArray bit is set; reset `CC 20..27 00 00`, `CC 90..92 00 00`,
  `CC 28 FF 07`; `CC 31 00`; `CC 32 00` then wait 50 ms.
- `--probe` sends only `CC 60` and `CC 61` (each followed by a read) and prints the
  parsed report (incl. support_cmd_flag) — no other writes.
- Zones on the X870 EAGLE WIFI7 (re-typed from OpenRGB's layout, not copied; register /
  mask): ARGB_V2_1 0x25/0x20, ARGB_V2_2 0x26/0x40, ARGB_V2_3 0x27/0x80, Chipset accent
  0x22/0x04, LED_C 0x24/0x10.
- Effect packet: byte 1 register, bytes 2–5 zone mask, byte 11 effect, byte 12 max
  brightness, byte 13 min brightness, bytes 14–17 color as B, G, R, 0, bytes 22–29
  period0..3 (uint16 LE, ms), byte 30 effect_param0.
- Board identity: read `BaseBoardProduct` from the registry, trim, compare exactly to
  `X870 EAGLE WIFI7`; any other board → motherboard unavailable.
- If colors come out swapped on a header, permute the color bytes per register in
  Fusion2Script (never a calibration write).

### Flash wear

RigLight never sends `CC 5E` (save to flash), `CC 47` (persistence toggle), `CC 33`
(color-order write) or `CC 34` (LED-count config, flash behaviour unverified). State is
restored on every launch / resume instead.

### iCUE SDK: loading and threading

- Loading from MinGW: x64 has one calling convention, `bool` is 1 byte and enums 4 bytes
  on both compilers, no bitfields — layouts are compatible. `#define _LIB` before
  including `iCUESDK.h` (strips dllimport), declare `decltype(&CorsairConnect)` pointers,
  `GetProcAddress` the exports, load with `LoadLibraryExW(<exe dir>\iCUESDK.x64_2019.dll,
  LOAD_WITH_ALTERED_SEARCH_PATH)`. `static_assert`: `sizeof(CorsairDeviceInfo)==396`,
  `sizeof(CorsairLedColor)==8`, `sizeof(CorsairLedPosition)==24`,
  `sizeof(CorsairSessionStateChanged)==40`, `sizeof(CorsairEvent)==16`. The DLL needs the
  VC++ 2015–2022 runtime (iCUE installs it); load failure → RAM unavailable, not fatal.
- Threading: SDK callbacks (session state, device events) only `PostMessage` to the main
  window (`WM_APP_CORSAIR_SESSION`, `WM_APP_CORSAIR_DEVICE`); no `Corsair*` call ever runs
  inside a callback. All `Corsair*` calls run on the UI thread except the animation
  thread's per-frame `CorsairSetLedColors` (synchronous, ~20 fps). The animation thread
  reads a mutex-protected LightState snapshot and is stopped/joined before
  `CorsairReleaseControl` / `CorsairDisconnect`.
- Session contract: `CorsairConnect` once at start; `CorsairDisconnect` only at exit (the
  SDK reconnects by itself through Timeout/ConnectionLost → Connected). On every
  `CSS_Connected`: `CorsairGetDevices(CDT_MemoryModule)`, `CorsairGetLedPositions` per
  DIMM, `CorsairRequestControl(id, CAL_ExclusiveLightingControl)`, re-apply. Subscribe to
  events; a memory module `isConnected=true` event → request control + re-apply for it.
  `CE_NoControl` → RAM unavailable, retry via a 10 s `WM_TIMER`.

## Sleep, resume and device changes

- `WM_POWERBROADCAST` / `PBT_APMRESUMEAUTOMATIC`: Fusion2 close + reopen + full init +
  re-apply; RAM re-enumerate + re-request control + re-apply (the session may survive
  while exclusive control does not).
- `RegisterDeviceNotification(GUID_DEVINTERFACE_HID)` on the main window. Re-init the
  Fusion2 only when *its* interface path arrives (or after a failed write) — not on every
  `WM_DEVICECHANGE`, since init resets every register and would blink the lights.
- `WM_QUERYENDSESSION` / Quit: stop animation, release control, disconnect. The DIMMs
  then go back to iCUE's profile (documented behaviour).

## Error handling

- iCUE not running / SDK disabled (`CE_NotConnected`): RAM unavailable until the session
  reaches `CSS_Connected`, then re-apply.
- Fusion2 device missing / open fails / write fails: motherboard unavailable; retried on
  next apply, on its interface arrival and on resume.
- Nothing ever shows a modal error; status is in the tooltip and the panel footer.

## One-time setup (done with the user)

1. iCUE → Settings → enable SDK / third-party control (via computer use).
2. iCUE → RAM device settings → brightness back to max (it was set to minimum earlier and
   would dim RigLight's output too).
3. iCUE → RAM hardware lighting (shown when iCUE/RigLight aren't running) → off or dim.
4. User uninstalls GIGABYTE Control Center (needs admin), which removes its elevated
   startup task so nothing fights over the Fusion2 controller.
5. Windows Settings → Personalization → Dynamic Lighting: off for the motherboard device
   (otherwise Windows drives MI_00 and may fight). The user does this (system setting).
6. BIOS: "LEDs in Sleep/Hibernation/Soft Off" → off (user, optional).

## Testing

- Unit tests (CTest), core only:
  - Settings round-trip + garbage tolerance.
  - Effects: pulse bounds/period, rainbow 7-step sequence and timing, pct scaling, a=255.
  - Fusion2Report parsing from a captured/constructed report.
  - Fusion2Script: byte-exact init sequence (incl. LampArray conditional and 50 ms wait),
    byte-exact apply for off / static / breathing / rainbow, the protocol doc's worked
    example (static red on 0x25), commit packet `CC 28 F4 00 00 00`, and a guard test
    that no step ever starts with `CC 5E`, `CC 47`, `CC 33` or `CC 34`.
  - CorsairSession with a fake SdkApi: connect → enumerate → control → apply; NoControl
    retry; ConnectionLost → Connected re-applies; device reconnect event re-applies.
- `RigLight.exe --probe`: read-only, prints the Fusion2 report and the Corsair devices.
- Live check on this PC: probe → Static red → Off → Breathing → Rainbow → Off; the user
  confirms by eye (Claude can't see the hardware). Then sleep/resume once.

## Non-goals

Per-zone colors, bedtime schedule, hotkeys, fan/GPU/peripheral lighting, music/screen
sync, installer, macOS, flash persistence.

## Open questions (settled at the live check)

- Nothing in the Fusion2 byte layout has been tried on this board yet.
- Pulse brightness scale (0–100 vs 0–255) and the low-brightness floor.
- Whether the header's color order affects effect colors.
- The hardware color cycle's 7-color order.
