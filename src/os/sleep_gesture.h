#pragma once

#include <cstdint>

namespace sleep_gesture {

inline constexpr std::uint32_t HOLD_MS = 3000;

class Gesture {
public:
    bool update(bool left, bool right, std::uint32_t now) {
        blocked_ = latched_ || (left && right);
        if (left && right) {
            latched_ = true;
            if (!timing_) {
                since_ = now;
                timing_ = true;
            }
            if (!sent_ && now - since_ >= HOLD_MS) {
                sent_ = true;
                return true;
            }
        } else {
            timing_ = false;
        }
        if (!left && !right) latched_ = sent_ = false;
        return false;
    }

    // Swallow the entire chord, including staggered releases and release clicks.
    bool blocked() const { return blocked_; }

private:
    bool latched_ = false;
    bool blocked_ = false;
    bool timing_ = false;
    bool sent_ = false;
    std::uint32_t since_ = 0;
};

}  // namespace sleep_gesture
