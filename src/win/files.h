#pragma once
#include <optional>
#include <string>

namespace rl {
std::wstring exeDir();        // directory of RigLight.exe, ends with '\'
std::wstring settingsPath();  // %APPDATA%\RigLight\settings.ini (creates the folder)
enum class ReadStatus { Ok, NotFound, Error };
// Error (sharing violation, sync/AV lock...) is distinct from NotFound: only NotFound means
// "first run" — anything else must not overwrite settings or touch autostart.
std::optional<std::string> readTextFile(const std::wstring& path, ReadStatus* status = nullptr);
bool writeTextFileAtomic(const std::wstring& path, const std::string& text);
}  // namespace rl
