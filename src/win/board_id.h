#pragma once
#include <string>

namespace rl {
// HKLM\HARDWARE\DESCRIPTION\System\BIOS\BaseBoardProduct, trimmed; "" if unreadable.
std::string baseBoardProduct();
}  // namespace rl
