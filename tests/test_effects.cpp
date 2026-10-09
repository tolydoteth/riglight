#include <algorithm>

#include "check.h"
#include "core/effects.h"

using namespace rl;

static LightState make(Effect e, Rgb c, int pct) {
    LightState s;
    s.effect = e;
    s.color = c;
    s.brightness = pct;
    return s;
}

TEST(effects_off_is_black) {
    LightState s = make(Effect::Rainbow, Rgb{255, 255, 255}, 100);
    s.on = false;
    CHECK(ramColorAt(s, 0) == Rgb{});
    CHECK(ramColorAt(s, 12345) == Rgb{});
    CHECK(!isAnimated(s));
}

TEST(effects_static_scales_by_brightness) {
    const LightState s = make(Effect::Static, Rgb{255, 100, 0}, 50);
    CHECK(ramColorAt(s, 0) == (Rgb{128, 50, 0}));
    CHECK(ramColorAt(s, 999999) == (Rgb{128, 50, 0}));
    CHECK(!isAnimated(s));
}

TEST(effects_scale_color_rounds_and_clamps) {
    CHECK(scaleColor(Rgb{255, 255, 255}, 1.0) == (Rgb{255, 255, 255}));
    CHECK(scaleColor(Rgb{255, 255, 255}, 0.0) == Rgb{});
    CHECK(scaleColor(Rgb{3, 3, 3}, 0.5) == (Rgb{2, 2, 2}));
    CHECK(scaleColor(Rgb{200, 0, 0}, 2.0) == (Rgb{255, 0, 0}));
}

TEST(effects_breathing_curve) {
    // Same order as the board's hardware pulse (seen live): starts bright, fades, holds dark,
    // rises. Starting dark put the RAM half a period (~1.1 s) behind the board.
    const LightState s = make(Effect::Breathing, Rgb{200, 0, 0}, 100);
    CHECK(isAnimated(s));
    CHECK(ramColorAt(s, 0) == (Rgb{200, 0, 0}));            // peak at apply
    CHECK(ramColorAt(s, 500) == (Rgb{100, 0, 0}));          // half-way down
    CHECK(ramColorAt(s, 1000) == Rgb{});                    // bottom
    CHECK(ramColorAt(s, 1100) == Rgb{});                    // hold at minimum
    CHECK(ramColorAt(s, 1700) == (Rgb{100, 0, 0}));         // half-way up
    CHECK(ramColorAt(s, 2200) == (Rgb{200, 0, 0}));         // next period peak
}

TEST(effects_breathing_respects_brightness) {
    const LightState s = make(Effect::Breathing, Rgb{200, 0, 0}, 50);
    CHECK(ramColorAt(s, 0) == (Rgb{100, 0, 0}));
}

TEST(effects_cycle_colors) {
    CHECK(cycleColor(0) == (Rgb{255, 0, 0}));
    CHECK(cycleColor(7) == cycleColor(0));
    CHECK(cycleColor(-1) == cycleColor(6));
    for (int i = 0; i < 7; ++i) {
        const Rgb c = cycleColor(i);
        const int mx = std::max({c.r, c.g, c.b});
        CHECK_EQ(mx, 255);  // full value
        CHECK(!(cycleColor(i) == cycleColor(i + 1)));
    }
}

TEST(effects_rainbow_steps) {
    const LightState s = make(Effect::Rainbow, Rgb{0, 0, 0}, 100);
    CHECK(isAnimated(s));
    using namespace timing;
    CHECK(ramColorAt(s, kCycleFadeMs) == cycleColor(0));              // fade done, hold
    CHECK(ramColorAt(s, kCycleStepMs - 1) == cycleColor(0));          // still holding
    CHECK(ramColorAt(s, kCycleStepMs + kCycleFadeMs) == cycleColor(1));
    CHECK(ramColorAt(s, 7 * kCycleStepMs + kCycleFadeMs) == cycleColor(0));  // wraps
    // mid-fade from step 6 (magenta, b = 219) to step 0 (red, b = 0) is between them on blue
    const Rgb mid = ramColorAt(s, kCycleFadeMs / 2);
    CHECK(mid.b < cycleColor(6).b && mid.b > 0);
}

TEST(effects_rainbow_respects_brightness) {
    const LightState s = make(Effect::Rainbow, Rgb{}, 50);
    CHECK(ramColorAt(s, timing::kCycleFadeMs) == (Rgb{128, 0, 0}));
}
