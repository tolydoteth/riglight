#pragma once
#include <string>

namespace rl {
// %APPDATA%\RigLight\riglight.log: one timestamped line per call; logReset() truncates it at startup.
void logReset();
void logLine(const std::string& line);
}  // namespace rl
