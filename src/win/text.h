#pragma once
#include <string>
#include <string_view>

namespace rl {
std::wstring widen(std::string_view s);   // UTF-8 -> UTF-16
std::string narrow(std::wstring_view s);  // UTF-16 -> UTF-8
}  // namespace rl
