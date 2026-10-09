#include "core/icon.h"

#include <algorithm>
#include <cmath>

namespace rl {

static uint32_t premul(uint32_t rgb, double a) {
    const auto ch = [&](int shift) { return (uint32_t)std::lround(((rgb >> shift) & 0xFF) * a); };
    return ((uint32_t)std::lround(a * 255) << 24) | (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

std::vector<uint32_t> bulbIconPixels(int size, bool on) {
    const double s = size;
    const double cx = s / 2, cy = s * 0.42, r = s * 0.33;
    const double ring = std::max(1.0, s / 14.0);  // outline width when off
    const double bx0 = s * 0.35, bx1 = s * 0.65, by0 = s * 0.76, by1 = s * 0.92;
    const uint32_t warm = 0xFFC24B, grey = 0x9A9A9A;
    std::vector<uint32_t> px((size_t)size * size, 0);
    const int sub = 4;  // 4x4 supersampling
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            int bulb = 0, base = 0;
            for (int sy = 0; sy < sub; ++sy)
                for (int sx = 0; sx < sub; ++sx) {
                    const double fx = x + (sx + 0.5) / sub, fy = y + (sy + 0.5) / sub;
                    const double d = std::hypot(fx - cx, fy - cy);
                    if (on ? d <= r : (d <= r && d >= r - ring)) ++bulb;
                    else if (fx >= bx0 && fx <= bx1 && fy >= by0 && fy <= by1) ++base;
                }
            const double n = sub * sub;
            if (bulb) px[(size_t)y * size + x] = premul(on ? warm : grey, bulb / n);
            else if (base) px[(size_t)y * size + x] = premul(grey, base / n);
        }
    return px;
}

}  // namespace rl
