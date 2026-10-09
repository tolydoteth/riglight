#include "core/settings.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <vector>

namespace rl {

static std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.remove_prefix(1);
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.remove_suffix(1);
    return s;
}

static std::string lowerStr(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

static std::optional<int> parseInt(std::string_view s) {
    s = trim(s);
    int v = 0;
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || p != s.data() + s.size()) return std::nullopt;
    return v;
}

static int clampLead(int ms) { return std::clamp(ms, 0, 2000); }

std::string serializeSettings(const LightState& s, int ramLeadMs) {
    std::string out = "[lights]\n";
    out += std::string("on=") + (s.on ? "1" : "0") + "\n";
    out += "color=" + colorHex(s.color) + "\n";
    out += std::string("effect=") + effectName(s.effect) + "\n";
    out += "brightness=" + std::to_string(clampBrightness(s.brightness)) + "\n";
    if (clampLead(ramLeadMs) != kDefaultRamLeadMs) out += "ram_lead_ms=" + std::to_string(clampLead(ramLeadMs)) + "\n";
    return out;
}

// Calls fn(lowercased key, trimmed value) for every key=value line.
template <class Fn>
static void forEachKey(std::string_view text, Fn fn) {
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view line = trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        fn(lowerStr(trim(line.substr(0, eq))), trim(line.substr(eq + 1)));
    }
}

int parseRamLeadMs(std::string_view text) {
    int lead = kDefaultRamLeadMs;
    forEachKey(text, [&](const std::string& key, std::string_view val) {
        if (key == "ram_lead_ms")
            if (auto v = parseInt(val)) lead = clampLead(*v);
    });
    return lead;
}

LightState parseSettings(std::string_view text) {
    LightState s;
    forEachKey(text, [&](const std::string& key, std::string_view val) {
        if (key == "on") {
            if (val == "1") s.on = true;
            else if (val == "0") s.on = false;
        } else if (key == "color") {
            if (auto c = parseColorHex(val)) s.color = *c;
        } else if (key == "effect") {
            if (auto e = parseEffect(val)) s.effect = *e;
        } else if (key == "brightness") {
            if (auto b = parseInt(val)) s.brightness = clampBrightness(*b);
        }
    });
    return s;
}

static std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> parts;
    size_t pos = 0;
    while (true) {
        const size_t end = s.find(sep, pos);
        parts.push_back(s.substr(pos, end == std::string_view::npos ? s.size() - pos : end - pos));
        if (end == std::string_view::npos) break;
        pos = end + 1;
    }
    return parts;
}

std::optional<LightState> parseStateSpec(std::string_view spec, const LightState& base) {
    const auto parts = split(trim(spec), ':');
    const std::string head = lowerStr(parts[0]);
    LightState s = base;
    if (head == "off" && parts.size() == 1) {
        s.on = false;
        return s;
    }
    if (head == "on" && parts.size() == 1) {
        s.on = true;
        return s;
    }
    s.on = true;
    if (head == "rainbow") {
        if (parts.size() > 2) return std::nullopt;
        s.effect = Effect::Rainbow;
        if (parts.size() == 2) {
            auto b = parseInt(parts[1]);
            if (!b) return std::nullopt;
            s.brightness = clampBrightness(*b);
        }
        return s;
    }
    if (head == "static" || head == "breathing") {
        if (parts.size() < 2 || parts.size() > 3) return std::nullopt;
        auto c = parseColorHex(parts[1]);
        if (!c) return std::nullopt;
        s.effect = head == "static" ? Effect::Static : Effect::Breathing;
        s.color = *c;
        if (parts.size() == 3) {
            auto b = parseInt(parts[2]);
            if (!b) return std::nullopt;
            s.brightness = clampBrightness(*b);
        }
        return s;
    }
    return std::nullopt;
}

}  // namespace rl
