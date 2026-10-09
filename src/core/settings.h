#pragma once
#include <optional>
#include <string>
#include <string_view>

#include "core/light_state.h"

namespace rl {

std::string serializeSettings(const LightState& s);
// Missing or garbled keys keep their LightState{} default; brightness is clamped.
LightState parseSettings(std::string_view text);
// "on" | "off" | "static:RRGGBB[:pct]" | "breathing:RRGGBB[:pct]" | "rainbow[:pct]".
// Every form except "off" turns the lights on. Unset parts are taken from `base`.
std::optional<LightState> parseStateSpec(std::string_view spec, const LightState& base);

}  // namespace rl
