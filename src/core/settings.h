#pragma once
#include <optional>
#include <string>
#include <string_view>

#include "core/light_state.h"

namespace rl {

// How far ahead (ms) the RAM's frames run, to make up for any delay in iCUE pushing colors to
// the DIMMs. Board and RAM share one clock, so the default is 0. Advanced ini key
// `ram_lead_ms` (0-2000); written only when it differs from the default.
inline constexpr int kDefaultRamLeadMs = 0;

std::string serializeSettings(const LightState& s, int ramLeadMs = kDefaultRamLeadMs);
// Missing or garbled keys keep their LightState{} default; brightness is clamped.
LightState parseSettings(std::string_view text);
int parseRamLeadMs(std::string_view text);
// "on" | "off" | "static:RRGGBB[:pct]" | "breathing:RRGGBB[:pct]" | "rainbow[:pct]".
// Every form except "off" turns the lights on. Unset parts are taken from `base`.
std::optional<LightState> parseStateSpec(std::string_view spec, const LightState& base);

}  // namespace rl
