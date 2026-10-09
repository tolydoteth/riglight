#pragma once
#include <string>

#include "core/light_state.h"

namespace rl {
std::string tooltipText(const LightState& s, bool mbOk, bool ramOk);  // UTF-8
std::string footerText(bool mbOk, bool ramOk);                         // UTF-8
}  // namespace rl
