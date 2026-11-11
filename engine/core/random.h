#pragma once

// Seeded random numbers (xorshift32, as in v1). The same seed always gives the
// same sequence, which is what makes replays and golden images possible.

#include <cstdint>

namespace tg {

class Random {
public:
    explicit Random(std::uint32_t seed = 1) : state_(seed == 0 ? 1 : seed) {}
    std::uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    // 0 .. count-1. Returns 0 when count is 0.
    std::uint32_t below(std::uint32_t count) { return count == 0 ? 0 : next() % count; }

private:
    std::uint32_t state_;
};

}  // namespace tg
