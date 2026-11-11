#pragma once

// Board A panel: ST7789, 172x320, used in landscape as 320x172.
// Carried over from v1.0 src/hal/display_config.h. Values are unchanged; only
// comments were added.

#include <cstdint>

#include "board_config.h"

namespace display_config {

inline constexpr bool HORIZONTAL = false;
inline constexpr std::int16_t WIDTH =
    HORIZONTAL ? board::LCD_PHYSICAL_WIDTH : board::LCD_PHYSICAL_HEIGHT;
inline constexpr std::int16_t HEIGHT =
    HORIZONTAL ? board::LCD_PHYSICAL_HEIGHT : board::LCD_PHYSICAL_WIDTH;

// The ST7789 controller addresses 240 lines but this glass only has 172, so
// the visible area starts 34 lines in on the panel's short (172 px) axis.
// It is named OFFSET_X because that axis is x in portrait. In landscape
// (HORIZONTAL = false, MADCTL 0x70) the short axis is the ROW axis, so
// hal/display.cpp adds these 34 pixels to the row addresses, not the columns.
inline constexpr std::uint16_t OFFSET_X = 34;
inline constexpr std::uint16_t OFFSET_Y = 0;
inline constexpr std::uint32_t SPI_FREQUENCY_HZ = 80'000'000;

// Height of v1's LVGL partial draw buffer. Unused until the LVGL shell returns;
// engine frames are presented whole (display::present).
inline constexpr std::uint8_t DRAW_BUFFER_LINES = 16;

}  // namespace display_config
