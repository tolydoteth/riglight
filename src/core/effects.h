#pragma once
#include <cstdint>

#include "core/light_state.h"

namespace rl {

// Shared by the RAM's software effects and the Fusion2 hardware effect packets, so the two
// start in step on every apply.
namespace timing {
inline constexpr uint32_t kPulseRiseMs = 1000;
inline constexpr uint32_t kPulseFallMs = 1000;
inline constexpr uint32_t kPulseHoldMs = 200;  // held at minimum (verify by eye)
inline constexpr uint32_t kPulsePeriodMs = kPulseRiseMs + kPulseFallMs + kPulseHoldMs;
inline constexpr uint32_t kCycleFadeMs = 700;
inline constexpr uint32_t kCycleHoldMs = 500;
inline constexpr int kCycleSteps = 7;
inline constexpr uint32_t kCycleStepMs = kCycleFadeMs + kCycleHoldMs;
inline constexpr uint32_t kFrameMs = 50;  // RAM animation: 20 fps
}  // namespace timing

Rgb scaleColor(Rgb c, double factor);  // rounds, clamps to 0..255
Rgb cycleColor(int step);              // 7 evenly spaced full-saturation hues, step 0 = red
bool isAnimated(const LightState& s);  // on and not static
Rgb ramColorAt(const LightState& s, uint32_t tMs);

}  // namespace rl
