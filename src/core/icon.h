#pragma once
#include <cstdint>
#include <vector>

namespace rl {
// size x size premultiplied 0xAARRGGBB pixels, top-down. On: filled warm bulb; off: grey outline.
std::vector<uint32_t> bulbIconPixels(int size, bool on);
}  // namespace rl
