# RigLight — design

Date: 2026-10-09
Status: approved in chat (design), spec awaiting review

## Purpose

The user sleeps in the same room as the PC and the RGB lighting is too bright at night.
Today the only way to change it is through two heavy vendor apps (Corsair iCUE and
GIGABYTE Control Center), and GCC runs elevated so it can't even be automated. RigLight
replaces both UIs with one small tray app: one click turns the PC lights off or on, and a
small panel picks color, effect and brightness for everything at once.

Success looks like:
- Lights off/on in one click from the tray, no vendor UI ever opened.
- Color / effect / brightness changes apply to the motherboard (incl. ARGB headers / case
  lighting) and the RAM together, and look in sync.
- After a reboot the lights come back in the last chosen state (off stays off).
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
| GIGABYTE X870 EAGLE WIFI7 RGB Fusion 2 controller, ITE USB `048D:5711` | HID (MI_00 / MI_01) | Own driver: HID feature reports, protocol ported from OpenRGB (see `docs/fusion2-protocol.md`). No admin. |
| Corsair Vengeance RGB DDR5 ×2 (CMH32GX5M2B6000C38) | SMBus, owned by iCUE | Corsair iCUE SDK v4.0.84 (`third_party/iCUESDK`), exclusive lighting control. |

Out of scope: GPU, fans' speed, Logitech/SteelSeries/Keychron peripherals.

## User-facing behaviour

**Tray icon** (bulb glyph; dimmed glyph when lights are off).
- Left-click: toggle lights on/off.
- Right-click menu: `Lights on/off`, `Open panel…`, `Start with Windows` ✓, `Quit`.
- Double-click: open panel.
- Tooltip: `RigLight — On · Rainbow · 60%` or `RigLight — Off`; appends
  `· RAM unavailable` / `· Motherboard unavailable` when a device can't be reached.

**Panel** — small borderless popup anchored above the tray (like a Windows flyout),
closes when it loses focus. Contents top to bottom:
1. Power toggle (On/Off).
2. Color: 8 swatches (warm white, white, red, orange, green, cyan, blue, purple)
   + "Custom…" (Windows ChooseColor dialog).
   Disabled while effect = Rainbow.
3. Effect: segmented control `Static | Breathing | Rainbow`.
4. Brightness: slider 5–100 %.
Every change applies immediately (no Apply button) and is saved.

**Persistence**: `%APPDATA%\RigLight\settings.ini`:
`on`, `color` (hex RRGGBB), `effect` (static|breathing|rainbow), `brightness` (5–100),
`autostart`. Missing/garbled values fall back to defaults (on, warm white, static, 60 %).

**Startup**: HKCU `Run` entry when `Start with Windows` is checked (default on).
On launch the saved state is applied to every device. Single instance (named mutex);
a second launch opens the panel of the running instance.

## Architecture

One process, small units with one job each:

```
src/
  core/   (no Windows UI; unit-tested)
    LightState      struct {on, color, effect, brightness} + defaults
    Settings        load/save LightState + autostart <-> ini text
    Effects         pure math: colorAt(state, t) for software effects (breathing curve,
                    rainbow hue), shared timing constants (periods) used by both devices
    Fusion2Packets  builds the exact HID byte buffers for a LightState (init, per-zone
                    effect, apply) — pure, so the byte layout is unit-tested
  win/
    Fusion2Device   finds 048D:5711 via SetupAPI/HID, opens the right interface,
                    sends Fusion2Packets with HidD_SetFeature; reports availability
    CorsairRam      LoadLibrary("iCUESDK.x64_2019.dll"), CorsairConnect, find
                    CDT_MemoryModule devices, request CAL_ExclusiveLightingControl,
                    animation thread (~30 fps) for breathing/rainbow, single write for
                    static/off; reconnects when iCUE appears/restarts
    LightController owns both devices; apply(LightState) fans out; tracks availability
    Tray            Shell_NotifyIcon, menu, tooltip
    Panel           Direct2D/DirectWrite popup drawing + hit testing
    Autostart       HKCU Run read/write
    main.cpp        WinMain, single-instance, message loop
tests/            core unit tests (CTest), same check.h style as Keyflip
third_party/iCUESDK  Corsair SDK v4.0.84 (header, DLL; DLL copied next to the exe)
```

Interfaces: every device implements `apply(const LightState&)` and
`available() const`. LightController doesn't know protocol details. UI only ever
changes LightState and calls `controller.apply(state)` + `settings.save(state)`.

### Effect mapping

| LightState | Motherboard (hardware effect) | RAM (SDK) |
|---|---|---|
| off | static, color 000000 (or the protocol's off effect) on every zone | all LEDs 000000, exclusive control held |
| static | static, color × brightness | all LEDs color × brightness, written once |
| breathing | pulse effect, color, max = brightness, min = 0, period P_breath | software: same curve/period from `Effects` |
| rainbow | color-cycle effect, brightness, period P_cycle | software: uniform hue rotation, same period |

Periods (`P_breath`, `P_cycle`) are chosen from the values the Fusion2 hardware supports
so the RAM's software animation can match them; they start in phase on each apply.
Perfect long-term phase lock is not a goal.

Brightness on the motherboard uses the effect's max-brightness field if the protocol has
one, otherwise color scaling. On the RAM it's color scaling.

ARGB headers (case fans/strips) get the same hardware effect as onboard zones — no
per-LED streaming: RigLight sends `CC 32 00` so every header runs its hardware effect.

### Fusion2 specifics (from `docs/fusion2-protocol.md`)

- Open the HID collection on **MI_01 / COL02** (usage page 0xFF89, usage 0xCC); all
  packets are 64-byte feature reports starting with report ID 0xCC.
- Init: `CC 60` + read info report (logged by `--probe`), `CC 61` + read (5711),
  `CC 48 00` only if the LampArray flag is set, reset registers, `CC 31 00`, `CC 32 00`.
- Zones on the X870 EAGLE WIFI7 (OpenRGB's layout for this board, re-typed, not copied):
  ARGB_V2_1 0x25/0x20, ARGB_V2_2 0x26/0x40, ARGB_V2_3 0x27/0x80, Chipset accent
  0x22/0x04, LED_C 0x24/0x10. Each gets one effect packet, then `CC 28 <OR of masks>`.
- Effect codes used: 1 static, 2 pulse, 4 color cycle; color bytes are B, G, R, 0;
  max brightness byte 12; timings bytes 22–29 (ms, 16-bit).
- Board identity check: RigLight reads the BIOS board name (WMI/registry) and only uses
  this zone map on `X870 EAGLE WIFI7`; any other board → motherboard unavailable.

### Flash wear

RigLight never sends `CC 5E` (save to flash), `CC 47` (persistence toggle), `CC 33`
(color-order write) or `CC 34` (LED-count config, flash behaviour unverified). Settings
are restored on every launch instead.

## Error handling

- iCUE not running / SDK disabled (`CE_NotConnected`): RAM marked unavailable; the SDK's
  session callback triggers re-apply when the session becomes `CSS_Connected`.
- Another client takes exclusive control (`CE_NoControl`): RAM unavailable, retry on the
  next state change or every 10 s.
- When RigLight exits, iCUE regains the RAM and shows its own lighting. Documented; the
  app is meant to be running all the time.
- Fusion2 device missing / open fails / write fails: motherboard unavailable; retried on
  next apply and on `WM_DEVICECHANGE`.
- Nothing ever shows a modal error; status is in the tooltip and the panel footer.

## One-time setup (done with the user)

1. iCUE → Settings → enable SDK / third-party control (via computer use).
2. iCUE → RAM device settings → brightness back to max (it was set to minimum earlier and
   would dim RigLight's output too).
3. User uninstalls GIGABYTE Control Center (needs admin), which removes its elevated
   startup task so nothing fights over the Fusion2 controller.

## Testing

- Unit tests (CTest): Settings round-trip + garbage tolerance; Effects curves (breathing
  bounds/period, rainbow hue continuity, brightness scaling); Fusion2Packets byte-exact
  buffers for off / static / breathing / rainbow checked against the protocol doc.
- `RigLight.exe --probe`: prints Fusion2 init read-back (product, firmware, LED counts) and
  the Corsair devices found, without changing any lights.
- Live check on this PC: Off → Static red → Breathing → Rainbow → Off; the user confirms
  by eye (Claude can't see the hardware).

## Non-goals

Per-zone colors, bedtime schedule, hotkeys, fan/GPU/peripheral lighting, music/screen
sync, installer, macOS.

## Open questions

- Nothing in the Fusion2 byte layout has been tried on this board yet; the first live
  step is `--probe` (read-only), then a single static-color apply.
- Whether the header's color-order setting affects effect colors (if red shows as green,
  swap order per header in Fusion2Packets — no flash writes).
- Whether the iCUE SDK DLL needs the VC++ 2019 runtime (iCUE already installs it).
