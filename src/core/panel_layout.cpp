#include "core/panel_layout.h"

#include <algorithm>
#include <cmath>

namespace rl {

PanelLayout panelLayout() {
    PanelLayout l;
    const float pad = 16, inner = 264;
    l.width = 296;
    l.height = 290;
    l.title = {pad, 16, 180, 28};
    l.power = {l.width - pad - 52, 18, 52, 24};
    l.colorLabel = {pad, 60, inner, 18};
    for (int i = 0; i < 8; ++i) l.swatches[i] = {pad + i * 30.0f, 84, 24, 24};
    l.custom = {pad + 8 * 30.0f, 84, 24, 24};
    l.effectLabel = {pad, 124, inner, 18};
    for (int i = 0; i < 3; ++i) l.effects[i] = {pad + i * 88.0f, 148, 88, 30};
    l.brightnessLabel = {pad, 194, inner, 18};
    l.slider = {pad, 216, inner, 28};
    l.track = {pad + 10, 229, inner - 20, 2};
    l.footer = {pad, 256, inner, 18};
    return l;
}

PanelHitResult panelHitTest(const PanelLayout& l, float x, float y) {
    if (l.power.contains(x, y)) return {PanelHit::Power, -1};
    for (int i = 0; i < 8; ++i)
        if (l.swatches[i].contains(x, y)) return {PanelHit::Swatch, i};
    if (l.custom.contains(x, y)) return {PanelHit::Custom, -1};
    for (int i = 0; i < 3; ++i)
        if (l.effects[i].contains(x, y)) return {PanelHit::Effect, i};
    if (l.slider.contains(x, y)) return {PanelHit::Slider, -1};
    return {};
}

int sliderValueAt(const PanelLayout& l, float x) {
    const float f = std::clamp((x - l.track.x) / l.track.w, 0.0f, 1.0f);
    return clampBrightness(kMinBrightness + (int)std::lround(f * (kMaxBrightness - kMinBrightness)));
}

float sliderKnobX(const PanelLayout& l, int pct) {
    const float f = float(clampBrightness(pct) - kMinBrightness) / (kMaxBrightness - kMinBrightness);
    return l.track.x + f * l.track.w;
}

int swatchIndexOf(Rgb c) {
    for (int i = 0; i < (int)kSwatches.size(); ++i)
        if (kSwatches[i] == c) return i;
    return -1;
}

RectI placePanel(RectI icon, RectI work, int w, int h) {
    const int m = 12;
    const bool known = icon.right > icon.left && icon.bottom > icon.top;
    const int icx = (icon.left + icon.right) / 2, icy = (icon.top + icon.bottom) / 2;
    int x = work.right - m - w, y = work.bottom - m - h;  // default: bottom-right corner
    if (known) {
        if (icon.top >= work.bottom) {  // taskbar at the bottom
            x = icx - w / 2;
            y = work.bottom - m - h;
        } else if (icon.bottom <= work.top) {  // top
            x = icx - w / 2;
            y = work.top + m;
        } else if (icon.left >= work.right) {  // right
            x = work.right - m - w;
            y = icy - h / 2;
        } else if (icon.right <= work.left) {  // left
            x = work.left + m;
            y = icy - h / 2;
        }
    }
    x = std::clamp(x, work.left + m, work.right - m - w);
    y = std::clamp(y, work.top + m, work.bottom - m - h);
    return RectI{x, y, x + w, y + h};
}

bool ApplyThrottle::request(uint64_t now) {
    if (!any_ || now - last_ >= interval_) {
        any_ = true;
        last_ = now;
        pending_ = false;
        return true;
    }
    pending_ = true;
    return false;
}

bool ApplyThrottle::flushDue(uint64_t now) {
    if (pending_ && now - last_ >= interval_) {
        last_ = now;
        pending_ = false;
        return true;
    }
    return false;
}

}  // namespace rl
