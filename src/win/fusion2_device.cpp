#include "win/fusion2_device.h"

#include <windows.h>

#include <cwctype>

#include "core/effects.h"
#include "win/board_id.h"
#include "win/text.h"

namespace rl {

using namespace fusion2;

Fusion2Device::Fusion2Device() : boardOk_(baseBoardProduct() == kBoardName) {}

bool Fusion2Device::run(HidDevice& dev, const std::vector<Step>& steps, std::vector<Packet>* reads) {
    for (const Step& s : steps) {
        if (isForbidden(s.bytes)) return false;  // never write flash, whatever the caller built
        if (!dev.setFeature(s.bytes)) return false;
        if (s.readAfter) {
            Packet r{};
            r[0] = kReportId;
            if (!dev.getFeature(r)) return false;
            if (reads) reads->push_back(r);
        }
        if (s.waitMs) Sleep(s.waitMs);
    }
    return true;
}

void Fusion2Device::close() {
    dev_.close();
    ready_ = false;
}

bool Fusion2Device::reinit() {
    close();
    if (!boardOk_) return false;
    const auto paths = findHidCollections(kVid, kPid, kUsagePage, kUsage);
    if (paths.empty() || !dev_.open(paths[0])) return false;
    std::vector<Packet> reads;
    if (!run(dev_, infoSteps(), &reads) || reads.empty()) {
        close();
        return false;
    }
    const Report report = parseReport(reads[0]);
    if (!report.valid || !run(dev_, initSteps(report), nullptr)) {
        close();
        return false;
    }
    ready_ = true;
    return true;
}

bool Fusion2Device::apply(const LightState& s) {
    if (!ready_ && !reinit()) return false;
    // Animated effects follow RigLight's clock (see LightController); start them at frame 0.
    const auto steps = isAnimated(s) ? frameSteps(ramColorAt(s, 0)) : applySteps(s);
    if (!run(dev_, steps, nullptr)) {
        close();
        return false;
    }
    return true;
}

bool Fusion2Device::writeFrame(Rgb color) {
    if (!ready_) return false;
    if (!run(dev_, frameSteps(color), nullptr)) {
        close();
        return false;
    }
    return true;
}

bool Fusion2Device::matchesPath(const wchar_t* path) {
    if (!path) return false;
    std::wstring p(path);
    for (wchar_t& c : p) c = (wchar_t)std::towlower(c);
    return p.find(L"vid_048d&pid_5711&mi_01&col02") != std::wstring::npos;
}

std::string Fusion2Device::probe() {
    std::string out = "== Motherboard (Fusion2) ==\n";
    const std::string board = baseBoardProduct();
    out += "BaseBoardProduct: \"" + board + "\" (" + (board == kBoardName ? "supported" : "NOT supported") + ")\n";
    const auto paths = findHidCollections(kVid, kPid, kUsagePage, kUsage);
    out += "Control collections found: " + std::to_string(paths.size()) + "\n";
    for (const auto& p : paths) out += "  " + narrow(p) + "\n";
    if (paths.empty()) return out;
    HidDevice dev;
    if (!dev.open(paths[0])) {
        out += "open failed, error " + std::to_string(GetLastError()) + "\n";
        return out;
    }
    std::vector<Packet> reads;
    const bool ok = run(dev, probeSteps(), &reads);
    out += std::string("probe script: ") + (ok ? "ok" : "FAILED, error " + std::to_string(GetLastError())) + "\n";
    if (!reads.empty()) {
        out += "CC 60 report: " + describeReport(parseReport(reads[0])) + "\n" + hexDump(reads[0]);
    }
    if (reads.size() > 1) out += "CC 61 calibration:\n" + hexDump(reads[1]);
    return out;
}

}  // namespace rl
