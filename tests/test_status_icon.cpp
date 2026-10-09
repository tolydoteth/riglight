#include "check.h"
#include "core/icon.h"
#include "core/status.h"

using namespace rl;

TEST(status_tooltip_on_off) {
    LightState s;
    s.effect = Effect::Rainbow;
    s.brightness = 60;
    CHECK_STR(tooltipText(s, true, true), "RigLight \xE2\x80\x94 On \xC2\xB7 Rainbow \xC2\xB7 60%");
    s.on = false;
    CHECK_STR(tooltipText(s, true, true), "RigLight \xE2\x80\x94 Off");
}

TEST(status_tooltip_unavailable_devices) {
    LightState s;
    s.on = false;
    CHECK_STR(tooltipText(s, false, false),
              "RigLight \xE2\x80\x94 Off \xC2\xB7 Motherboard unavailable \xC2\xB7 RAM unavailable");
    CHECK_STR(tooltipText(s, true, false), "RigLight \xE2\x80\x94 Off \xC2\xB7 RAM unavailable");
}

TEST(status_footer) {
    CHECK_STR(footerText(true, true), "Motherboard \xE2\x9C\x93 \xC2\xB7 RAM \xE2\x9C\x93");
    CHECK_STR(footerText(false, true), "Motherboard unavailable \xC2\xB7 RAM \xE2\x9C\x93");
}

TEST(icon_pixels_shape) {
    for (int size : {16, 20, 24, 32}) {
        const auto on = bulbIconPixels(size, true);
        const auto off = bulbIconPixels(size, false);
        CHECK_EQ(on.size(), (size_t)(size * size));
        CHECK_EQ(off.size(), (size_t)(size * size));
        CHECK_EQ(on[0] >> 24, 0u);                       // transparent corner
        const uint32_t c = on[(size * 42 / 100) * size + size / 2];  // bulb centre
        CHECK_EQ(c >> 24, 255u);                         // opaque
        CHECK(((c >> 16) & 0xFF) > 200);                 // warm: strong red channel
        const uint32_t oc = off[(size * 42 / 100) * size + size / 2];
        CHECK_EQ(oc >> 24, 0u);                          // off: outline only, hollow centre
    }
}
