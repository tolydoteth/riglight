#include "core/fusion2.h"

#include <cstdio>

#include "core/effects.h"

namespace rl::fusion2 {

static void putU16(Packet& p, size_t at, uint16_t v) {
    p[at] = (uint8_t)(v & 0xFF);
    p[at + 1] = (uint8_t)(v >> 8);
}

static void putU32(Packet& p, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[at + i] = (uint8_t)((v >> (8 * i)) & 0xFF);
}

static uint32_t getU32(const Packet& p, size_t at) {
    return (uint32_t)p[at] | ((uint32_t)p[at + 1] << 8) | ((uint32_t)p[at + 2] << 16) |
           ((uint32_t)p[at + 3] << 24);
}

Packet command(uint8_t cmd, uint8_t arg0, uint8_t arg1) {
    Packet p{};
    p[0] = kReportId;
    p[1] = cmd;
    p[2] = arg0;
    p[3] = arg1;
    return p;
}

Report parseReport(const Packet& p) {
    Report r;
    r.valid = p[0] == kReportId;
    r.deviceNum = p[2];
    r.stripDetect = p[3];
    for (int i = 0; i < 4; ++i) r.fw[i] = p[4 + i];
    for (int i = 0; i < 3; ++i) r.argbCounts[i] = p[8 + i];
    r.supportCmdFlag = p[11];
    for (size_t i = 12; i < 40 && p[i] != 0; ++i) r.product += (char)p[i];
    r.chipId = getU32(p, 56);
    return r;
}

std::string describeReport(const Report& r) {
    char buf[256];
    std::snprintf(buf, sizeof buf,
                  "valid=%d product=\"%s\" fw=%u.%u.%u.%u device_num=%u strip_detect=%u "
                  "argb_counts=%02X %02X %02X support_cmd_flag=0x%02X (lamparray=%d) chip_id=0x%08X",
                  r.valid ? 1 : 0, r.product.c_str(), r.fw[0], r.fw[1], r.fw[2], r.fw[3],
                  r.deviceNum, r.stripDetect, r.argbCounts[0], r.argbCounts[1], r.argbCounts[2],
                  r.supportCmdFlag, r.lampArray() ? 1 : 0, r.chipId);
    return buf;
}

std::string hexDump(const Packet& p) {
    std::string out;
    char b[4];
    for (size_t i = 0; i < p.size(); ++i) {
        std::snprintf(b, sizeof b, "%02X", p[i]);
        out += b;
        out += (i % 16 == 15) ? '\n' : ' ';
    }
    return out;
}

bool isForbidden(const Packet& p) {
    const uint8_t c = p[1];
    return c == 0x5E || c == 0x47 || c == 0x33 || c == 0x34;
}

std::vector<Step> infoSteps() { return {Step{command(0x60), true, 0}}; }

std::vector<Step> probeSteps() {
    return {Step{command(0x60), true, 0}, Step{command(0x61), true, 0}};
}

std::vector<Step> initSteps(const Report& r) {
    std::vector<Step> v;
    if (r.lampArray()) v.push_back({command(0x48, 0x00)});  // LampArray (Dynamic Lighting) off
    for (uint8_t reg = 0x20; reg <= 0x27; ++reg) v.push_back({command(reg)});
    for (uint8_t reg = 0x90; reg <= 0x92; ++reg) v.push_back({command(reg)});
    v.push_back({command(0x28, 0xFF, 0x07)});  // apply all (5711 mask 0x07FF)
    v.push_back({command(0x31, 0x00)});        // audio "beat" mode off
    v.push_back({command(0x32, 0x00), false, 50});  // every ARGB header runs hardware effects
    return v;
}

Packet effectPacket(const Zone& z, const LightState& s) {
    using namespace rl::timing;
    Packet p{};
    p[0] = kReportId;
    p[1] = z.reg;
    putU32(p, 2, z.mask);
    uint8_t type = kEffectStatic, maxB = 0, param0 = 0;
    Rgb color{};
    uint16_t p0 = 0, p1 = 0, p2 = 0;
    const int pct = clampBrightness(s.brightness);
    const uint8_t b255 = (uint8_t)((pct * 255 + 50) / 100);
    if (s.on) {
        switch (s.effect) {
            case Effect::Static:
                maxB = b255;
                color = s.color;
                break;
            case Effect::Breathing:
                type = kEffectPulse;
                maxB = kPulseBrightnessIsPercent ? (uint8_t)pct : b255;
                color = s.color;
                p0 = (uint16_t)kPulseRiseMs;
                p1 = (uint16_t)kPulseFallMs;
                p2 = (uint16_t)kPulseHoldMs;
                break;
            case Effect::Rainbow:
                type = kEffectCycle;
                maxB = b255;
                p0 = (uint16_t)kCycleFadeMs;
                p1 = (uint16_t)kCycleHoldMs;
                param0 = (uint8_t)kCycleSteps;
                break;
        }
    }
    p[11] = type;
    p[12] = maxB;
    p[13] = 0;  // min brightness
    p[14] = color.b;
    p[15] = color.g;
    p[16] = color.r;
    putU16(p, 22, p0);
    putU16(p, 24, p1);
    putU16(p, 26, p2);
    p[30] = param0;
    return p;
}

std::vector<Step> applySteps(const LightState& s) {
    std::vector<Step> v;
    for (const Zone& z : kBoardZones) v.push_back({effectPacket(z, s)});
    Packet commit{};
    commit[0] = kReportId;
    commit[1] = 0x28;
    putU32(commit, 2, kBoardZoneMask);
    v.push_back({commit});
    return v;
}

}  // namespace rl::fusion2
