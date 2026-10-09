# Gigabyte RGB Fusion 2 USB (ITE IT5711, 048D:5711) — protocol notes

These notes describe the wire protocol of Gigabyte's RGB Fusion 2 USB lighting controller as
RigLight uses it. The facts were learned by reading OpenRGB's open-source implementation
(GitLab `CalcProgrammer1/OpenRGB`, `Controllers/GigabyteRGBFusion2USBController/`, master as of
2026-09 and tag `release_1.0`) and were then checked on a GIGABYTE X870 EAGLE WIFI7. They are
written here in our own words; no OpenRGB code is reproduced. Credit for the reverse engineering
belongs to the OpenRGB contributors.

Conventions: all multi-byte fields are little-endian. "`CC xx yy`" means a 64-byte feature
report whose byte 0 is `0xCC`, byte 1 is `xx`, byte 2 is `yy`, and every other byte is 0.

## 1. Transport

| Item | Value |
|---|---|
| USB IDs | VID `0x048D`, PID `0x5711` (OpenRGB also handles 0x8297, 0x8950, 0x5702 and 0x0414:0xA100) |
| HID collection | the one whose top-level caps are UsagePage `0xFF89`, Usage `0x00CC` |
| Report ID | `0xCC`, byte 0 of every packet in both directions |
| Report length | 64 bytes including the report ID |
| Report type | feature reports only (`HidD_SetFeature` / `HidD_GetFeature`); no input/output reports |

On the X870 EAGLE WIFI7 the device exposes three collections:

| Windows instance | UsagePage:Usage | Feature len | What it is |
|---|---|---|---|
| `MI_00` | 0x0059:0x0001 | 51 | HID LampArray (Windows Dynamic Lighting) |
| `MI_01&COL01` | 0xFF89:0x0010 | 17 | vendor collection, unused |
| `MI_01&COL02` | 0xFF89:0x00CC | 64 | **the Fusion 2 control channel — open this one** |

Before `HidD_GetFeature`, set `buf[0] = 0xCC`.

## 2. Start-up sequence

1. `CC 60`, then GetFeature → the 64-byte **info report** (layout below). The 0x60 write is
   required before the read.
2. (IT5711 only, optional) `CC 61`, then GetFeature → the strip **calibration report** for
   headers 3–6. RigLight reads it only in `--probe`.
3. If info byte 11 has bit 1 set (LampArray present): `CC 48 00` turns LampArray mode off.
4. Reset: an all-zero effect packet for each register `0x20`…`0x27` (i.e. `CC 20` … `CC 27`),
   on IT5711 also `CC 90`, `CC 91`, `CC 92`; then apply-all `CC 28 FF 07` (other PIDs: `CC 28 FF 00`).
5. `CC 31 00` turns the hardware audio-"beat" mode off.
6. `CC 32 <mask>` chooses, per addressable header, hardware effect (bit clear) or per-LED
   streaming (bit set). RigLight sends `CC 32 00` (all headers run effects), then waits 50 ms.

### Info report (reply to 0x60)

| Offset | Size | Meaning |
|---|---|---|
| 0 | 1 | report id 0xCC |
| 1 | 1 | product (unused) |
| 2 | 1 | device number (MCU index) |
| 3 | 1 | strip detect (1 = Gen2 strip detection supported) |
| 4 | 4 | firmware version, shown as b4.b5.b6.b7 |
| 8 | 1 | LED-count class: low nibble header 1, high nibble header 2 |
| 9 | 1 | LED-count class: low nibble header 3, high nibble header 4 |
| 10 | 1 | LED-count class: low nibble onboard ARGB 1, high nibble onboard ARGB 2 |
| 11 | 1 | support flags: bit 0 = persistence commands (0x47/0x5E) supported; bit 1 = LampArray present |
| 12 | 28 | product name, ASCII, NUL-terminated |
| 40 | 4 | channel-order calibration, spare 0 |
| 44 | 4 | channel order, addressable header 1 |
| 48 | 4 | channel order, addressable header 2 |
| 52 | 4 | channel order, single-color ("mainboard") outputs |
| 56 | 4 | chip id |
| 60 | 4 | channel-order calibration, spare 1 |

### Calibration report (reply to 0x61, IT5711)

Offsets 4, 8, 12 and 16 hold the channel-order words for headers 3, 4 and onboard ARGB 1, 2.

A channel-order word stores, in bytes 0/1/2, the position of B/G/R inside each 3-byte LED
triplet of a **per-LED streaming** packet (e.g. "GRB" = `02 00 01 00`). Effect packets always
carry the color as B, G, R (section 3); whether the firmware remaps effect colors per header was
not verified — on the X870 EAGLE WIFI7 static red showed red on every header.

Probe on the reference board: product "IT5711-GIGABYTE V1.0.29.6", fw 1.0.29.6, device 0,
strip detect 1, support flags 0x03.

## 3. Effect packet (registers 0x20–0x27, 0x90–0x92)

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 1 | report id | 0xCC |
| 1 | 1 | register | `0x20 + i` for zone index i = 0–7, `0x90 + (i − 8)` for i = 8–10 |
| 2 | 4 | zone mask | `1 << i` (all zones: `0x07FF` on IT5711, `0xFF` otherwise) |
| 6 | 4 | second zone mask | 0 |
| 10 | 1 | reserved | 0 |
| 11 | 1 | effect type | see below |
| 12 | 1 | max brightness | 0–255 (breathing: keep ≤ 100, a known controller quirk) |
| 13 | 1 | min brightness | 0 |
| 14 | 4 | color 0 | bytes **B, G, R, 0** |
| 18 | 4 | color 1 | 0 |
| 22 | 2 | period 0 (ms) | fade in / rise; must be **0** for a plain ("direct") static color |
| 24 | 2 | period 1 (ms) | fade out |
| 26 | 2 | period 2 (ms) | hold |
| 28 | 2 | period 3 (ms) | used by some wave modes only |
| 30 | 1 | parameter 0 | colors to cycle through (7 = full spectrum) |
| 31 | 1 | parameter 1 | mode-specific |
| 32 | 1 | parameter 2 | flash repeat count |
| 33 | 1 | parameter 3 | 0 |
| 34 | 30 | padding | 0 |

Effect types: 0 none, 1 static, 2 pulse (breathing), 3 flash, 4 color cycle, 6 wave,
8 random, 9–12 wave variants. There is no "off" type: off is static with color 0 and max
brightness 0.

Timings Gigabyte's own software uses: breathing 400–1000 ms in 100 ms steps then up to 1600 ms
in 200 ms steps (period 0 = period 1, hold 200 ms); color cycle period 0 300–2400 ms with
period 1 = period 0 − 200 ms and parameter 0 = 7.

Example — static red at full brightness on ARGB header 1 (register 0x25, zone bit 0x20):

```
00: CC 25 20 00 00 00 00 00 00 00 00 01 FF 00 00 00
10: FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
20..3F: 00
```

**RigLight's finding:** the board's hardware pulse/cycle timing drifts against software
animation running elsewhere (here, the Corsair RAM). For effects that must stay in sync with
other devices, RigLight writes one static ("direct", period 0 = 0) frame per zone at 20 fps
instead of using the hardware effects.

## 4. Apply and mode commands

- **Apply:** effect packets take effect only after `CC 28 <mask LE32> 00 00 00 00`, where mask
  is the OR of the zone masks just written (e.g. `CC 28 F4 00 00 00` after registers
  0x22, 0x24, 0x25, 0x26, 0x27). Apply-all: `CC 28 FF 07` (IT5711).
- **`CC 32 <mask>`** — per addressable header, bit set = per-LED streaming (built-in effect off),
  bit clear = hardware effect. Bits: 0x01 header 1, 0x02 header 2, 0x08 header 3, 0x10 header 4,
  0x20 onboard ARGB 1, 0x40 onboard ARGB 2 (0x04 unused). Wait 50 ms after sending.

Single-byte commands (`CC <cmd> <arg>`):

| Cmd | Meaning | RigLight |
|---|---|---|
| 0x31 | audio "beat" mode off/on | sends 0 at init |
| 0x48 | LampArray mode off/on | sends 0 at init if flag bit 1 |
| 0x60 | request info report | yes |
| 0x61 | request calibration report (IT5711) | probe only |
| 0x47 | lighting persistence flag | **never** |
| 0x5E | save lighting state to flash | **never** |
| 0x33 | write channel-order calibration (persistent) | **never** |
| 0x34 | LED-count classes per header (persistence unknown) | **never** |
| 0x38–0x3F | Gen2 strip scan | never |

## 5. Zone map

| Index | Effect register | Zone bit | Streaming register | 0x32 bit | Typical use |
|---|---|---|---|---|---|
| 0 | 0x20 | 0x001 | – | – | single-color zone |
| 1 | 0x21 | 0x002 | – | – | single-color zone |
| 2 | 0x22 | 0x004 | – | – | single-color zone |
| 3 | 0x23 | 0x008 | – | – | single-color zone |
| 4 | 0x24 | 0x010 | – | – | single-color zone |
| 5 | 0x25 | 0x020 | 0x58 | 0x01 | addressable header 1 |
| 6 | 0x26 | 0x040 | 0x59 | 0x02 | addressable header 2 |
| 7 | 0x27 | 0x080 | 0x62 | 0x08 | addressable header 3 |
| 8 | 0x90 | 0x100 | 0x63 | 0x10 | addressable header 4 |
| 9 | 0x91 | 0x200 | 0x64 | 0x20 | onboard ARGB 1 |
| 10 | 0x92 | 0x400 | 0x65 | 0x40 | onboard ARGB 2 |

Which indices a board actually wires up differs per model. OpenRGB keeps per-board layouts
keyed by the exact DMI baseboard name (Windows: `HKLM\HARDWARE\DESCRIPTION\System\BIOS`,
`BaseBoardProduct`). **GIGABYTE X870 EAGLE WIFI7** uses:

| Zone | Register / bit |
|---|---|
| ARGB_V2_1 (addressable) | 0x25 / 0x20 |
| ARGB_V2_2 (addressable) | 0x26 / 0x40 |
| ARGB_V2_3 (addressable) | 0x27 / 0x80 |
| Chipset accent | 0x22 / 0x04 |
| LED_C (12 V RGB header, presumably) | 0x24 / 0x10 |

Unknown boards: OpenRGB falls back to a generic IT5711 layout (headers 1–4, indices 0–4 and
8–10). The "all zones" form (register 0x20 with mask 0x07FF, then `CC 28 FF 07`) is the simplest
way to set one color everywhere without a layout, but it is untested by RigLight.

### Per-LED streaming (not used by RigLight)

Register 0x58/0x59/0x62–0x65 packets carry: byte 1 register, bytes 2–3 byte offset into the
strip (first LED × 3), byte 4 byte count (≤ 57), then up to 19 LED triplets ordered by the
header's channel-order word. No apply packet follows.

### LED-count classes (0x34, not used by RigLight)

Bytes 2–4 carry six nibbles (headers 1–4, onboard 1–2): 0 ≤ 32 LEDs, 1 ≤ 64, 2 ≤ 256, 3 ≤ 512,
4 more. IT57xx strings are limited to 256 LEDs.

## 6. Safety

- Effects written with the commands RigLight uses live in controller RAM only.
- 0x5E saves lighting to flash, 0x47 toggles persistence, 0x33 rewrites channel order
  persistently, 0x34's persistence is unknown — RigLight refuses to send any of them
  (`fusion2::isForbidden`, checked before every write).
- Whether a board keeps lighting across a power cycle without 0x5E was not verified.

## 7. Open questions

- Meaning of info byte 1, 0x32 bit 0x04, and the second color / second zone fields.
- Whether effect colors honour the channel-order words on every header type.
- Whether the "all zones" packet behaves the same on every IT5711 board.
