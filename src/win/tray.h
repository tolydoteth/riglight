#pragma once
#include <windows.h>
#include <shellapi.h>

#include <string>

namespace rl {

class Tray {
public:
    bool add(HWND hwnd, HICON icon, const std::wstring& tip);  // also after TaskbarCreated
    void update(HICON icon, const std::wstring& tip);
    void remove();
    RECT iconRect() const;  // screen rect of the icon; empty if unknown

private:
    NOTIFYICONDATAW nid_{};
    bool added_ = false;
};

}  // namespace rl
