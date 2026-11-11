#pragma once

// Fixed 60 Hz game clock. Feed it elapsed real time; it says how many game
// ticks to run. Games therefore behave the same at any frame rate, and a
// scripted run is exactly repeatable.

#include <cstdint>

namespace tg {

class FixedStep {
public:
    static constexpr int HZ = 60;
    // After a long stall, catch up by at most this many ticks and drop the rest.
    static constexpr int MAX_TICKS = 5;

    int advanceMicros(std::uint32_t elapsed_us) {
        // Accumulate in units of 1/60 microsecond so 60 Hz is exact.
        accumulated_ += static_cast<std::uint64_t>(elapsed_us) * HZ;
        int ticks = 0;
        while (accumulated_ >= ONE_SECOND && ticks < MAX_TICKS) {
            accumulated_ -= ONE_SECOND;
            ++ticks;
        }
        if (accumulated_ >= ONE_SECOND) accumulated_ = 0;
        return ticks;
    }

private:
    static constexpr std::uint64_t ONE_SECOND = 1'000'000;
    std::uint64_t accumulated_ = 0;
};

}  // namespace tg
