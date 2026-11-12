#pragma once

#include <cstdint>

#include "board_config.h"

namespace display_config {

inline constexpr std::int16_t WIDTH = tg::board::WIDTH;
inline constexpr std::int16_t HEIGHT = tg::board::HEIGHT;

// The landscape mounting gives each controller a different address-window axis.
inline constexpr std::uint8_t MADCTL = tg::board::LCD_169 ? 0xA0 : 0x70;
inline constexpr std::uint16_t COLUMN_OFFSET = tg::board::LCD_169 ? 20 : 0;
inline constexpr std::uint16_t ROW_OFFSET = tg::board::LCD_169 ? 0 : 34;
inline constexpr std::uint32_t SPI_FREQUENCY_HZ =
    tg::board::LCD_169 ? 40'000'000 : 80'000'000;

inline constexpr std::uint8_t DRAW_BUFFER_LINES = 16;

struct Window {
    std::uint16_t x1;
    std::uint16_t y1;
    std::uint16_t x2;
    std::uint16_t y2;
};

inline constexpr Window window(int x1, int y1, int x2, int y2) {
    return {static_cast<std::uint16_t>(x1 + COLUMN_OFFSET),
            static_cast<std::uint16_t>(y1 + ROW_OFFSET),
            static_cast<std::uint16_t>(x2 + COLUMN_OFFSET),
            static_cast<std::uint16_t>(y2 + ROW_OFFSET)};
}

}  // namespace display_config
