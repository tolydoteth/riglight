#include "core/effects.h"

#include <algorithm>
#include <cmath>

namespace rl {

static uint8_t channel(double v) { return (uint8_t)std::clamp(std::lround(v), 0L, 255L); }

Rgb scaleColor(Rgb c, double f) {
    return Rgb{channel(c.r * f), channel(c.g * f), channel(c.b * f)};
}

Rgb cycleColor(int step) {
    const int n = timing::kCycleSteps;
    step = ((step % n) + n) % n;
    const double h = step * 360.0 / n;  // HSV with s = v = 1
    const int sector = (int)(h / 60.0) % 6;
    const double f = h / 60.0 - std::floor(h / 60.0);
    const double q = 1.0 - f, t = f;
    double r = 0, g = 0, b = 0;
    switch (sector) {
        case 0: r = 1; g = t; b = 0; break;
        case 1: r = q; g = 1; b = 0; break;
        case 2: r = 0; g = 1; b = t; break;
        case 3: r = 0; g = q; b = 1; break;
        case 4: r = t; g = 0; b = 1; break;
        default: r = 1; g = 0; b = q; break;
    }
    return Rgb{channel(r * 255), channel(g * 255), channel(b * 255)};
}

bool isAnimated(const LightState& s) { return s.on && s.effect != Effect::Static; }

static Rgb mix(Rgb a, Rgb b, double f) {
    return Rgb{channel(a.r + (b.r - a.r) * f), channel(a.g + (b.g - a.g) * f),
               channel(a.b + (b.b - a.b) * f)};
}

Rgb ramColorAt(const LightState& s, uint32_t tMs) {
    using namespace timing;
    if (!s.on) return Rgb{};
    const double pct = clampBrightness(s.brightness) / 100.0;
    switch (s.effect) {
        case Effect::Static:
            return scaleColor(s.color, pct);
        case Effect::Breathing: {
            // The board's hardware pulse starts bright: fall, hold dark, rise (matched live).
            const uint32_t p = tMs % kPulsePeriodMs;
            double f = 0;
            if (p < kPulseFallMs) f = 1.0 - double(p) / kPulseFallMs;
            else if (p >= kPulseFallMs + kPulseHoldMs) f = double(p - kPulseFallMs - kPulseHoldMs) / kPulseRiseMs;
            return scaleColor(s.color, pct * f);
        }
        case Effect::Rainbow: {
            const int step = (int)((tMs / kCycleStepMs) % kCycleSteps);
            const uint32_t p = tMs % kCycleStepMs;
            Rgb c = cycleColor(step);
            if (p < kCycleFadeMs) c = mix(cycleColor(step - 1), c, double(p) / kCycleFadeMs);
            return scaleColor(c, pct);
        }
    }
    return Rgb{};
}

}  // namespace rl
