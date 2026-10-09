#pragma once
#include <cstdint>

#include "core/light_state.h"

namespace rl {

// The animation's state + start time + a generation that every restart bumps. A frame computed
// from a snapshot is written only if its generation is still current, so a frame of the old
// effect can never land after apply() switched to off/static. Not thread-safe: the owner locks.
class AnimClock {
public:
    struct Frame {
        LightState state;
        uint32_t tMs = 0;
        uint64_t generation = 0;
    };
    void restart(const LightState& s, uint64_t nowMs) {
        state_ = s;
        t0_ = nowMs;
        ++generation_;
    }
    Frame snapshot(uint64_t nowMs) const { return Frame{state_, (uint32_t)(nowMs - t0_), generation_}; }
    bool isCurrent(uint64_t generation) const { return generation == generation_; }
    const LightState& state() const { return state_; }

private:
    LightState state_;
    uint64_t t0_ = 0;
    uint64_t generation_ = 0;
};

// Rate-limits expensive retries (device discovery + init script) on paths a user can trigger
// many times a second, e.g. dragging the slider while the board is unavailable.
class RetryGate {
public:
    explicit RetryGate(uint32_t minIntervalMs) : interval_(minIntervalMs) {}
    bool allow(uint64_t nowMs) {
        if (any_ && nowMs - last_ < interval_) return false;
        any_ = true;
        last_ = nowMs;
        return true;
    }
    void reset() { any_ = false; }

private:
    uint32_t interval_;
    uint64_t last_ = 0;
    bool any_ = false;
};

}  // namespace rl
