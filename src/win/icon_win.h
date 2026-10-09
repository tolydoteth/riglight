#pragma once
#include <windows.h>

namespace rl {
HICON makeBulbIcon(int size, bool on);  // caller DestroyIcon()s it
}  // namespace rl
