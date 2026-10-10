# AGENTS.md — adapting RigLight to another PC

Instructions for coding agents (Claude Code, Codex, …) asked to make RigLight work on a
different setup. Read this whole file before changing code. Humans are welcome too.

## Ground rules

1. **Never write lighting-controller flash.** For Gigabyte Fusion 2 that means never sending
   commands `0x5E`, `0x47`, `0x33` or `0x34` (byte 1 of the packet). `fusion2::isForbidden()` is
   enforced in `Fusion2Device::run()` and by the test `fusion2_never_emits_flash_commands`; keep
   both. For any new controller, find out which commands persist and add the same kind of guard
   *before* the first write.
2. **Read before you write.** Add a read-only probe for new hardware first (see `--probe`), run
   it, and only then send a single harmless write (one static color) — and ask the human to
   confirm by eye. You cannot see the lights; never claim a lighting change worked without the
   human saying so.
3. **Core stays pure and tested.** Byte layouts, scripts, state machines and math go in
   `src/core` with unit tests in `tests/` (TDD: write the failing test first). `src/win` only
   executes them.
4. **Don't copy GPL code.** OpenRGB is the best reference for protocols (GPL-2.0). Read it,
   then write your own code and describe facts in your own words — RigLight is MIT.
5. **Don't commit vendor SDKs** without a redistribution license (Corsair's has none; `build.bat`
   downloads it).
6. Build and test with `build.bat` (all tests must pass, no new warnings).

## Map of the code

| File | Job |
|---|---|
| `src/core/light_state.*` | `LightState {on, color, effect, brightness}`, hex/effect parsing |
| `src/core/settings.*` | `settings.ini` format, `--set` spec parsing, `ram_lead_ms` |
| `src/core/effects.*` | `ramColorAt(state, t)`: the color of any effect at time t (used for every animated device) |
| `src/core/anim_clock.h` | animation clock with a generation counter (stale frames are dropped) + `RetryGate` |
| `src/core/fusion2.*` | Gigabyte Fusion 2: info-report parser, init/apply/frame scripts, **board zone map**, flash guard |
| `src/core/corsair_session.*` | iCUE SDK session/control state machine over a function-pointer `SdkApi` |
| `src/core/panel_layout.*`, `status.*`, `icon.*` | panel geometry, tooltip text, tray icon pixels |
| `src/win/hid.*` | find a HID collection by VID/PID/usage page/usage; feature reports |
| `src/win/fusion2_device.*` | runs the Fusion 2 scripts; board-name check; `probe()` |
| `src/win/sdk_loader.*`, `corsair_ram.*` | load `iCUESDK.x64_2019.dll`, own the Corsair session |
| `src/win/light_controller.*` | fans a `LightState` out to every device; **the one animation thread** |
| `src/win/app.*`, `tray.*`, `panel.*`, `main.cpp` | tray app, menu, panel, CLI, sleep/resume, device arrival |

Every device follows the same shape: `apply(LightState)` for a new state, `writeFrame(...)` for
one animation frame (called ~20×/s from `LightController::animLoop`), `available()`, and a way to
re-initialise after resume/re-plug. Device writes are serialised by `LightController::devMu_`.

## Recipe A — a different Gigabyte board (Fusion 2 USB controller)

1. Check the controller: Device Manager / `Get-PnpDevice` for `VID_048D` with PID `5711`, `5702`,
   `8297` or `8950`. Other PIDs differ in details (see `docs/fusion2-protocol.md` §1, §6); 5711 is
   the tested one. `findHidCollections()` must find the `0xFF89 / 0x00CC` collection.
2. Run `RigLight.exe --probe` (read-only) and read `riglight-probe.txt`: board name as Windows
   reports it (`BaseBoardProduct`), report validity, firmware, support flags.
3. Find the board's zone layout: which effect registers/zone bits it wires up (ARGB headers,
   onboard zones). Sources: OpenRGB's per-board layouts (look up the board name; read, don't
   copy), the board manual's header list, or trial: one zone at a time, static red, ask the
   human what lit up. Register/bit table: `docs/fusion2-protocol.md` §5.
4. Generalise the hard-coded map in `src/core/fusion2.h` (`kBoardName`, `kBoardZones`,
   `kBoardZoneMask`): e.g. a small table of `{boardName, zones[]}` and a lookup by board name;
   `Fusion2Device` uses the matched entry or reports "unsupported". Add tests: lookup, commit
   mask = OR of zone bits, flash guard over the new scripts.
5. ARGB headers 4 / onboard ARGB use registers `0x90–0x92` and `CC 32` bits `0x10/0x20/0x40`;
   `initSteps()` already resets them and sends `CC 32 00`.
6. If colors come out swapped on a header, permute the color bytes for that register in the
   packet builder (with a test) — never write the calibration command `0x33`.
7. Live check with the human: probe → static red → off → dim red → breathing → rainbow → off.

Shortcut worth trying on an unknown board (untested by RigLight): one "all zones" packet —
register `0x20`, zone mask `0x07FF`, then apply `CC 28 FF 07` — sets every zone at once.

## Recipe B — more Corsair devices

`CorsairSession::enumerate()` asks iCUE only for `CDT_MemoryModule`. To include fans, coolers,
keyboards, mice…, widen `filter.deviceTypeMask` (e.g. `CDT_All`, or an OR of types), update the
fake-SDK tests in `tests/test_corsair_session.cpp`, and check `--probe` lists the devices. iCUE
must be running and RigLight approved in iCUE's SDK prompt. Known iCUE behaviour (tested):
right after subscribing to events it fires a device-connected event; re-requesting exclusive
control you already hold returns `CE_NoControl` — keep held control across re-enumeration.

## Recipe C — another vendor (ASUS Aura, MSI, ASRock, NZXT, …)

Option 1, simplest and broadest: talk to **OpenRGB's SDK server** (TCP, default port 6742; the
user runs OpenRGB in the background). Write a `src/core` client for its documented network
protocol (request controller count/data, update LEDs/mode) with byte-exact tests, and a
`src/win` device that implements `apply` / `writeFrame` / `available` on top of it, then add it
to `LightController` next to `mb_` and `ram_`. RigLight then becomes a tray front-end for
anything OpenRGB supports.

Option 2: a native driver for one controller, following the Fusion 2 pattern: protocol notes
in `docs/`, pure packet builders + flash guard in `src/core`, a thin `src/win` device.

Avoid drivers that need raw SMBus/I²C access (e.g. writing DDR5 RAM controllers directly): they
need a kernel driver and admin rights, and a wrong write can corrupt the DIMM's SPD data. Prefer
the vendor SDK or OpenRGB for RAM.

## Wiring a new device into the app

- Add the device as a member of `LightController`; call it in `start`, `apply`, `onResume`,
  `onRetryTimer`, `shutdown`, and from `animLoop` (inside the `devMu_` block, after the
  generation check) with `ramColorAt(state, t)` as the frame color.
- Report availability through `LightController` and extend `tooltipText` / `footerText` (and
  their tests) if the UI should mention it.
- Device writes must never block for long: the animation thread runs every 50 ms and the UI
  thread shares `devMu_`.
- A lost device must not re-run discovery on every frame — use `RetryGate` / the 10 s retry
  timer, like `Fusion2Device`.

## Testing checklist

- `build.bat` → all unit tests pass, no new warnings.
- `RigLight.exe --probe` output makes sense for the new hardware.
- Live, with the human watching: static red (correct color on every part?), off (fully dark?),
  dim red, breathing (in sync?), rainbow, off.
- Quit and restart RigLight; sleep/resume once with lights off.
