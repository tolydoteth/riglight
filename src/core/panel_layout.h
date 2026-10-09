#pragma once
#include <array>
#include <cstdint>

#include "core/light_state.h"

namespace rl {

struct RectF {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

enum class PanelHit { None, Power, Swatch, Custom, Effect, Slider };
struct PanelHitResult {
    PanelHit kind = PanelHit::None;
    int index = -1;
};

// All values in DIPs (96 dpi units).
struct PanelLayout {
    float width = 0, height = 0;
    RectF title, power, colorLabel;
    std::array<RectF, 8> swatches;
    RectF custom, effectLabel;
    std::array<RectF, 3> effects;  // Static, Breathing, Rainbow
    RectF brightnessLabel, slider, track, footer;
};

inline constexpr std::array<Rgb, 8> kSwatches{{
    {0xFF, 0xB4, 0x6B},  // warm white
    {0xFF, 0xFF, 0xFF},  // white
    {0xFF, 0x00, 0x00},  // red
    {0xFF, 0x6A, 0x00},  // orange
    {0x00, 0xFF, 0x00},  // green
    {0x00, 0xFF, 0xFF},  // cyan
    {0x00, 0x40, 0xFF},  // blue
    {0x9B, 0x00, 0xFF},  // purple
}};

PanelLayout panelLayout();
PanelHitResult panelHitTest(const PanelLayout& l, float x, float y);
int sliderValueAt(const PanelLayout& l, float x);  // 5..100
float sliderKnobX(const PanelLayout& l, int pct);
int swatchIndexOf(Rgb c);  // -1 if not a preset

struct RectI {
    int left = 0, top = 0, right = 0, bottom = 0;
};
// Places a w x h panel next to the tray icon inside the monitor work area (12 px margin).
RectI placePanel(RectI icon, RectI work, int w, int h);

// Rate-limits device writes while dragging; the last requested value is always flushed.
class ApplyThrottle {
public:
    explicit ApplyThrottle(uint32_t intervalMs) : interval_(intervalMs) {}
    bool request(uint64_t nowMs);   // true: apply now; false: marked pending
    bool flushDue(uint64_t nowMs);  // true: apply the pending value now
    bool pending() const { return pending_; }

private:
    uint32_t interval_;
    uint64_t last_ = 0;
    bool any_ = false;
    bool pending_ = false;
};

}  // namespace rl
