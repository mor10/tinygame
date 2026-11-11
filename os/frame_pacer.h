#pragma once

// New in v2. Decides when a frame is due so the screen is refreshed at most
// 30 times a second, however fast the loop spins. Pure arithmetic on a 32-bit
// microsecond clock (which wraps after about 71 minutes; subtraction is safe).

#include <cstdint>

namespace frame_pacer {

inline constexpr std::uint32_t FRAME_INTERVAL_US = 33'333;  // 30 per second

class Pacer {
public:
    // True when a frame should be drawn and presented now. Frames stay on a
    // steady beat; after a long stall the beat restarts instead of bursting.
    bool due(std::uint32_t now_us) {
        if (!started_) {
            started_ = true;
            last_us_ = now_us;
            return true;
        }
        const std::uint32_t elapsed = now_us - last_us_;
        if (elapsed < FRAME_INTERVAL_US) return false;
        last_us_ = elapsed < 2 * FRAME_INTERVAL_US ? last_us_ + FRAME_INTERVAL_US : now_us;
        return true;
    }

    // Microseconds until the next frame is due (0 if due now).
    std::uint32_t untilDue(std::uint32_t now_us) const {
        if (!started_) return 0;
        const std::uint32_t elapsed = now_us - last_us_;
        return elapsed >= FRAME_INTERVAL_US ? 0 : FRAME_INTERVAL_US - elapsed;
    }

private:
    bool started_ = false;
    std::uint32_t last_us_ = 0;
};

}  // namespace frame_pacer
