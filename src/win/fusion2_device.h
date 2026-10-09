#pragma once
#include <string>
#include <vector>

#include "core/fusion2.h"
#include "core/light_state.h"
#include "win/hid.h"

namespace rl {

class Fusion2Device {
public:
    Fusion2Device();
    bool supportedBoard() const { return boardOk_; }
    bool reinit();                     // close, find, open, read info, run init script
    bool apply(const LightState& s);   // reinit first if needed; closes on failure
    void close();
    bool available() const { return ready_; }
    // True if a WM_DEVICECHANGE interface path is our control collection.
    static bool matchesPath(const wchar_t* path);
    // Read-only diagnostic: sends only CC 60 / CC 61 and reads the replies.
    static std::string probe();

private:
    static bool run(HidDevice& dev, const std::vector<fusion2::Step>& steps, std::vector<fusion2::Packet>* reads);
    HidDevice dev_;
    bool boardOk_ = false;
    bool ready_ = false;
};

}  // namespace rl
