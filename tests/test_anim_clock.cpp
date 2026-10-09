#include "check.h"
#include "core/anim_clock.h"

using namespace rl;

TEST(anim_clock_snapshot_and_time) {
    AnimClock c;
    LightState s;
    s.effect = Effect::Breathing;
    c.restart(s, 1000);
    const auto f = c.snapshot(1250);
    CHECK(f.state == s);
    CHECK_EQ(f.tMs, 250u);
    CHECK(c.isCurrent(f.generation));
}

TEST(anim_clock_restart_invalidates_old_frames) {
    // Review finding: a frame computed from the old (animated) state must not be written after
    // apply() switched to off/static. restart() bumps the generation; the frame is then stale.
    AnimClock c;
    LightState br;
    br.effect = Effect::Breathing;
    c.restart(br, 0);
    const auto f = c.snapshot(500);
    LightState off;
    off.on = false;
    c.restart(off, 600);
    CHECK(!c.isCurrent(f.generation));
    CHECK(c.isCurrent(c.snapshot(700).generation));
}

TEST(retry_gate_limits_attempts) {
    RetryGate g(2000);
    CHECK(g.allow(10000));   // first attempt goes through
    CHECK(!g.allow(10500));  // too soon
    CHECK(!g.allow(11999));
    CHECK(g.allow(12000));   // interval passed
    g.reset();
    CHECK(g.allow(12001));   // after success/reset, next failure may retry at once
}
