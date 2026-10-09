#include "core/light_state.h"

#include <algorithm>
#include <cctype>

namespace rl {

int clampBrightness(int pct) { return std::clamp(pct, kMinBrightness, kMaxBrightness); }

const char* effectName(Effect e) {
    switch (e) {
        case Effect::Breathing: return "breathing";
        case Effect::Rainbow: return "rainbow";
        default: return "static";
    }
}

const char* effectLabel(Effect e) {
    switch (e) {
        case Effect::Breathing: return "Breathing";
        case Effect::Rainbow: return "Rainbow";
        default: return "Static";
    }
}

static std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

std::optional<Effect> parseEffect(std::string_view s) {
    const std::string l = lower(s);
    if (l == "static") return Effect::Static;
    if (l == "breathing") return Effect::Breathing;
    if (l == "rainbow") return Effect::Rainbow;
    return std::nullopt;
}

std::string colorHex(Rgb c) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    for (uint8_t v : {c.r, c.g, c.b}) {
        out += digits[v >> 4];
        out += digits[v & 15];
    }
    return out;
}

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::optional<Rgb> parseColorHex(std::string_view s) {
    if (!s.empty() && s.front() == '#') s.remove_prefix(1);
    if (s.size() != 6) return std::nullopt;
    uint8_t v[3];
    for (int i = 0; i < 3; ++i) {
        const int hi = hexDigit(s[2 * i]), lo = hexDigit(s[2 * i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        v[i] = (uint8_t)(hi * 16 + lo);
    }
    return Rgb{v[0], v[1], v[2]};
}

}  // namespace rl
