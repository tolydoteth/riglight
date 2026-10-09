#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/fusion2.h"

namespace rl {

// Device paths of present HID collections with this VID/PID and top-level usage page/usage.
std::vector<std::wstring> findHidCollections(uint16_t vid, uint16_t pid, uint16_t usagePage, uint16_t usage);

class HidDevice {
public:
    HidDevice() = default;
    ~HidDevice() { close(); }
    HidDevice(const HidDevice&) = delete;
    HidDevice& operator=(const HidDevice&) = delete;

    bool open(const std::wstring& path);
    void close();
    bool isOpen() const { return h_ != INVALID_HANDLE_VALUE; }
    bool setFeature(const fusion2::Packet& p);
    bool getFeature(fusion2::Packet& p);  // p[0] must hold the report id

private:
    HANDLE h_ = INVALID_HANDLE_VALUE;
};

}  // namespace rl
