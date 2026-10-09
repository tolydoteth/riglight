#include "win/hid.h"

#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}

namespace rl {

std::vector<std::wstring> findHidCollections(uint16_t vid, uint16_t pid, uint16_t usagePage, uint16_t usage) {
    std::vector<std::wstring> out;
    GUID guid;
    HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return out;
    SP_DEVICE_INTERFACE_DATA ifd{};
    ifd.cbSize = sizeof ifd;
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &ifd); ++i) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, nullptr, 0, &need, nullptr);
        if (need == 0) continue;
        std::vector<uint8_t> buf(need);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifd, detail, need, nullptr, nullptr)) continue;
        const std::wstring path = detail->DevicePath;
        // Access 0: enough to read attributes and caps, works even when another app has it open.
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        bool match = false;
        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof attr;
        if (HidD_GetAttributes(h, &attr) && attr.VendorID == vid && attr.ProductID == pid) {
            PHIDP_PREPARSED_DATA pre = nullptr;
            if (HidD_GetPreparsedData(h, &pre)) {
                HIDP_CAPS caps{};
                if (HidP_GetCaps(pre, &caps) == HIDP_STATUS_SUCCESS)
                    match = caps.UsagePage == usagePage && caps.Usage == usage;
                HidD_FreePreparsedData(pre);
            }
        }
        CloseHandle(h);
        if (match) out.push_back(path);
    }
    SetupDiDestroyDeviceInfoList(set);
    return out;
}

bool HidDevice::open(const std::wstring& path) {
    close();
    h_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                     OPEN_EXISTING, 0, nullptr);
    return isOpen();
}

void HidDevice::close() {
    if (isOpen()) CloseHandle(h_);
    h_ = INVALID_HANDLE_VALUE;
}

bool HidDevice::setFeature(const fusion2::Packet& p) {
    if (!isOpen()) return false;
    fusion2::Packet copy = p;  // HidD_SetFeature takes a non-const buffer
    return HidD_SetFeature(h_, copy.data(), (ULONG)copy.size()) != FALSE;
}

bool HidDevice::getFeature(fusion2::Packet& p) {
    if (!isOpen()) return false;
    return HidD_GetFeature(h_, p.data(), (ULONG)p.size()) != FALSE;
}

}  // namespace rl
