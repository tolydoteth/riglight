#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rl {

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb&) const = default;
};

enum class Effect { Static, Breathing, Rainbow };

inline constexpr int kMinBrightness = 5;
inline constexpr int kMaxBrightness = 100;
inline constexpr Rgb kWarmWhite{0xFF, 0xB4, 0x6B};

struct LightState {
    bool on = true;
    Rgb color = kWarmWhite;
    Effect effect = Effect::Static;
    int brightness = 60;
    bool operator==(const LightState&) const = default;
};

int clampBrightness(int pct);
const char* effectName(Effect e);   // "static" | "breathing" | "rainbow" (file format)
const char* effectLabel(Effect e);  // "Static" | "Breathing" | "Rainbow" (UI)
std::optional<Effect> parseEffect(std::string_view s);  // case-insensitive
std::string colorHex(Rgb c);                            // "FFB46B"
std::optional<Rgb> parseColorHex(std::string_view s);   // 6 hex digits, optional '#'

}  // namespace rl
