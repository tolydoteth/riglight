#include "check.h"
#include "core/panel_layout.h"

using namespace rl;

static float cx(const RectF& r) { return r.x + r.w / 2; }
static float cy(const RectF& r) { return r.y + r.h / 2; }

TEST(panel_hit_test_every_control) {
    const PanelLayout l = panelLayout();
    CHECK(panelHitTest(l, cx(l.power), cy(l.power)).kind == PanelHit::Power);
    for (int i = 0; i < 8; ++i) {
        const auto h = panelHitTest(l, cx(l.swatches[i]), cy(l.swatches[i]));
        CHECK(h.kind == PanelHit::Swatch);
        CHECK_EQ(h.index, i);
    }
    CHECK(panelHitTest(l, cx(l.custom), cy(l.custom)).kind == PanelHit::Custom);
    for (int i = 0; i < 3; ++i) {
        const auto h = panelHitTest(l, cx(l.effects[i]), cy(l.effects[i]));
        CHECK(h.kind == PanelHit::Effect);
        CHECK_EQ(h.index, i);
    }
    CHECK(panelHitTest(l, cx(l.slider), cy(l.slider)).kind == PanelHit::Slider);
    CHECK(panelHitTest(l, 2, 2).kind == PanelHit::None);
    CHECK(panelHitTest(l, cx(l.footer), cy(l.footer)).kind == PanelHit::None);
}

TEST(panel_controls_fit_inside) {
    const PanelLayout l = panelLayout();
    auto inside = [&](const RectF& r) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= l.width && r.y + r.h <= l.height; };
    CHECK(inside(l.power) && inside(l.custom) && inside(l.slider) && inside(l.footer));
    for (const auto& r : l.swatches) CHECK(inside(r));
    for (const auto& r : l.effects) CHECK(inside(r));
}

TEST(panel_slider_math) {
    const PanelLayout l = panelLayout();
    CHECK_EQ(sliderValueAt(l, l.track.x - 50), 5);
    CHECK_EQ(sliderValueAt(l, l.track.x), 5);
    CHECK_EQ(sliderValueAt(l, l.track.x + l.track.w), 100);
    CHECK_EQ(sliderValueAt(l, l.track.x + l.track.w + 50), 100);
    for (int pct : {5, 33, 60, 100}) CHECK_EQ(sliderValueAt(l, sliderKnobX(l, pct)), pct);
}

TEST(panel_swatch_index) {
    CHECK_EQ(swatchIndexOf(kSwatches[0]), 0);
    CHECK_EQ(swatchIndexOf(kSwatches[7]), 7);
    CHECK_EQ(swatchIndexOf(Rgb{1, 2, 3}), -1);
}

TEST(panel_place_bottom_taskbar) {
    const RectI work{0, 0, 1920, 1032};
    const RectI icon{1700, 1040, 1724, 1064};
    const RectI r = placePanel(icon, work, 300, 290);
    CHECK_EQ(r.bottom, 1032 - 12);
    CHECK_EQ(r.right - r.left, 300);
    CHECK(r.left <= 1712 && r.right >= 1712);  // over the icon
}

TEST(panel_place_clamps_to_right_edge) {
    const RectI r = placePanel(RectI{1900, 1040, 1916, 1064}, RectI{0, 0, 1920, 1032}, 300, 290);
    CHECK_EQ(r.right, 1920 - 12);
}

TEST(panel_place_top_and_left_taskbars) {
    const RectI top = placePanel(RectI{1700, 4, 1724, 28}, RectI{0, 48, 1920, 1080}, 300, 290);
    CHECK_EQ(top.top, 48 + 12);
    const RectI left = placePanel(RectI{10, 900, 34, 924}, RectI{48, 0, 1920, 1080}, 300, 290);
    CHECK_EQ(left.left, 48 + 12);
    CHECK(left.top <= 912 && left.bottom >= 912);
}

TEST(panel_place_unknown_icon_uses_corner) {
    const RectI r = placePanel(RectI{0, 0, 0, 0}, RectI{0, 0, 1920, 1032}, 300, 290);
    CHECK_EQ(r.right, 1920 - 12);
    CHECK_EQ(r.bottom, 1032 - 12);
}

TEST(throttle_limits_rate_and_flushes_last) {
    ApplyThrottle t(40);
    CHECK(t.request(1000));    // first change applies at once
    CHECK(!t.request(1010));   // too soon: pending
    CHECK(!t.request(1020));
    CHECK(t.pending());
    CHECK(!t.flushDue(1030));  // still too soon
    CHECK(t.flushDue(1040));   // the latest value goes out
    CHECK(!t.pending());
    CHECK(!t.flushDue(2000));  // nothing pending
    CHECK(t.request(2000));
}
