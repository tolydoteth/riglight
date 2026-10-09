#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/light_state.h"

// Gigabyte RGB Fusion 2 USB (ITE IT5711). Protocol facts: docs/fusion2-protocol.md.
// Everything here is pure: it builds byte buffers and scripts; win/fusion2_device runs them.
namespace rl::fusion2 {

inline constexpr uint16_t kVid = 0x048D;
inline constexpr uint16_t kPid = 0x5711;
inline constexpr uint16_t kUsagePage = 0xFF89;
inline constexpr uint16_t kUsage = 0x00CC;
inline constexpr uint8_t kReportId = 0xCC;
inline constexpr size_t kPacketSize = 64;

using Packet = std::array<uint8_t, kPacketSize>;

struct Step {
    Packet bytes{};
    bool readAfter = false;  // GetFeature right after this SetFeature
    uint16_t waitMs = 0;     // sleep after the step
};

struct Zone {
    uint8_t reg;
    uint32_t mask;
    const char* name;
};

// GIGABYTE X870 EAGLE WIFI7 zones (register / zone bit), protocol doc §5.
inline constexpr const char* kBoardName = "X870 EAGLE WIFI7";
inline constexpr std::array<Zone, 5> kBoardZones{{
    {0x25, 0x20, "ARGB_V2_1"},
    {0x26, 0x40, "ARGB_V2_2"},
    {0x27, 0x80, "ARGB_V2_3"},
    {0x22, 0x04, "Chipset accent"},
    {0x24, 0x10, "LED_C"},
}};
inline constexpr uint32_t kBoardZoneMask = 0xF4;

inline constexpr uint8_t kEffectStatic = 1;
inline constexpr uint8_t kEffectPulse = 2;
inline constexpr uint8_t kEffectCycle = 4;
// Pulse max_brightness goes on the wire as 0-100 (OpenRGB caps breathing at 100 "due to
// controller quirks"). Flip if the live check shows breathing far dimmer than static.
inline constexpr bool kPulseBrightnessIsPercent = true;

struct Report {
    bool valid = false;
    uint8_t deviceNum = 0;
    uint8_t stripDetect = 0;
    std::array<uint8_t, 4> fw{};
    std::array<uint8_t, 3> argbCounts{};
    uint8_t supportCmdFlag = 0;
    std::string product;
    uint32_t chipId = 0;
    bool lampArray() const { return (supportCmdFlag & 0x02) != 0; }
};

Packet command(uint8_t cmd, uint8_t arg0 = 0, uint8_t arg1 = 0);
Report parseReport(const Packet& p);
std::string describeReport(const Report& r);
std::string hexDump(const Packet& p);
// Commands that write (or may write) controller flash: 0x5E, 0x47, 0x33, 0x34.
bool isForbidden(const Packet& p);

std::vector<Step> infoSteps();                 // CC 60 + read
std::vector<Step> probeSteps();                // CC 60 + read, CC 61 + read; nothing else
std::vector<Step> initSteps(const Report& r);  // run after infoSteps
Packet effectPacket(const Zone& z, const LightState& s);
std::vector<Step> applySteps(const LightState& s);
// One animation frame: every zone static ("direct", period0 = 0) at full max_brightness with the
// already-scaled frame color, then commit. Used for breathing/rainbow so the board follows
// RigLight's clock instead of its own effect timer (which drifted against the RAM).
std::vector<Step> frameSteps(Rgb color);

}  // namespace rl::fusion2
