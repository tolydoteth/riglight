#pragma once
#include <optional>
#include <string>

namespace rl {
std::wstring exeDir();        // directory of RigLight.exe, ends with '\'
std::wstring settingsPath();  // %APPDATA%\RigLight\settings.ini (creates the folder)
std::optional<std::string> readTextFile(const std::wstring& path);
bool writeTextFileAtomic(const std::wstring& path, const std::string& text);
}  // namespace rl
