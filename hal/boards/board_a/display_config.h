#pragma once

#include <cstdint>

#include "board_config.h"

namespace display_config {

inline constexpr bool HORIZONTAL = false;
inline constexpr std::int16_t WIDTH =
    HORIZONTAL ? board::LCD_PHYSICAL_WIDTH : board::LCD_PHYSICAL_HEIGHT;
inline constexpr std::int16_t HEIGHT =
    HORIZONTAL ? board::LCD_PHYSICAL_HEIGHT : board::LCD_PHYSICAL_WIDTH;

inline constexpr std::uint16_t OFFSET_X = 34;
inline constexpr std::uint16_t OFFSET_Y = 0;
inline constexpr std::uint32_t SPI_FREQUENCY_HZ = 80'000'000;

inline constexpr std::uint8_t DRAW_BUFFER_LINES = 16;

}  // namespace display_config
