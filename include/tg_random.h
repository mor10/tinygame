#pragma once

#include <cstdint>

namespace tg {

// Keep the seed in the game instance; equal seeds produce equal sequences.
inline std::uint32_t randomNext(std::uint32_t& seed) {
    if (seed == 0) seed = 1;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

}  // namespace tg
