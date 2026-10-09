#include "win/board_id.h"

#include <windows.h>

#include "win/text.h"

namespace rl {

std::string baseBoardProduct() {
    wchar_t buf[256];
    DWORD size = sizeof buf;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardProduct",
                     RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return {};
    std::string s = narrow(buf);
    const auto first = s.find_first_not_of(" \t");
    const auto last = s.find_last_not_of(" \t");
    return first == std::string::npos ? std::string() : s.substr(first, last - first + 1);
}

}  // namespace rl
