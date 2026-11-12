#pragma once

#include <cstdint>

namespace button_gesture {

inline constexpr std::uint32_t HOLD_MS = 3000;

class Gesture {
public:
    enum class Event : std::uint8_t { None, Pause, Sleep };

    Event update(bool left, bool right, std::uint32_t now) {
        const bool both = left && right;
        blocked_ = latched_ || (left && right);
        if (both) {
            latched_ = true;
            if (!timing_) {
                since_ = now;
                timing_ = true;
            }
            if (!sent_ && now - since_ >= HOLD_MS) {
                sent_ = true;
                return Event::Sleep;
            }
        } else if (latched_ && !left && !right) {
            Event event = Event::None;
            if (!sent_) {
                event = timing_ && now - since_ >= HOLD_MS ? Event::Sleep : Event::Pause;
            }
            latched_ = sent_ = timing_ = false;
            return event;
        } else {
            timing_ = false;
        }
        return Event::None;
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

}  // namespace button_gesture
