#pragma once

#include <cstdint>

namespace brickbreaker::level {

inline constexpr std::uint8_t LEVEL_COUNT = 6;
inline constexpr std::uint8_t MAX_BRICK_HITS = 2;

// Hits needed to break the brick at (row, column): 0 = empty, 1 = normal,
// 2 = tough. `level_index` is zero-based and wraps around LEVEL_COUNT.
std::uint8_t brickHits(std::uint32_t level_index, std::uint8_t row,
                       std::uint8_t column);

}  // namespace brickbreaker::level
