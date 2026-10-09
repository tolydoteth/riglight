#include "core/status.h"

namespace rl {

static const char* kDash = " \xE2\x80\x94 ";  // " — "
static const char* kDot = " \xC2\xB7 ";       // " · "
static const char* kTick = "\xE2\x9C\x93";    // "✓"

std::string tooltipText(const LightState& s, bool mbOk, bool ramOk) {
    std::string t = std::string("RigLight") + kDash;
    if (s.on)
        t += std::string("On") + kDot + effectLabel(s.effect) + kDot +
             std::to_string(clampBrightness(s.brightness)) + "%";
    else
        t += "Off";
    if (!mbOk) t += std::string(kDot) + "Motherboard unavailable";
    if (!ramOk) t += std::string(kDot) + "RAM unavailable";
    return t;
}

std::string footerText(bool mbOk, bool ramOk) {
    return std::string("Motherboard ") + (mbOk ? kTick : "unavailable") + kDot + "RAM " +
           (ramOk ? kTick : "unavailable");
}

}  // namespace rl
