#pragma once
#include <string>
#include <vector>

#include "core/anim_clock.h"
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
    // One animation frame (already-scaled color). Never reinits: it runs 20x a second on the
    // animation thread, so a lost device waits for the UI thread's retry/resume/arrival.
    bool writeFrame(Rgb color);
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
    RetryGate applyReinitGate_{2000};  // apply() may be called 25x/s (slider) while the board is gone
};

}  // namespace rl
