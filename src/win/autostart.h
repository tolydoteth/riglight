#pragma once

namespace rl {
bool autostartEnabled();      // HKCU Run value "RigLight" exists
bool setAutostart(bool on);   // writes "\"<exe path>\"" or deletes the value
}  // namespace rl
