#pragma once

#include <cstdint>

#include "tg_board.h"

namespace board {

// Physical panel
inline constexpr std::int16_t LCD_PHYSICAL_WIDTH = tg::board::LCD_169 ? 240 : 172;
inline constexpr std::int16_t LCD_PHYSICAL_HEIGHT = tg::board::LCD_169 ? 280 : 320;

// Board-specific display connections verified in the reviewed board-support work.
inline constexpr int PIN_LCD_MOSI = tg::board::LCD_169 ? 7 : 45;
inline constexpr int PIN_LCD_SCLK = tg::board::LCD_169 ? 6 : 40;
inline constexpr int PIN_LCD_CS = tg::board::LCD_169 ? 5 : 42;
inline constexpr int PIN_LCD_DC = tg::board::LCD_169 ? 4 : 41;
inline constexpr int PIN_LCD_RESET = tg::board::LCD_169 ? 8 : 39;
inline constexpr int PIN_LCD_BACKLIGHT = tg::board::LCD_169 ? 15 : 46;

// GPIO38 is not an RGB LED output on the 1.69 board.
inline constexpr int PIN_RGB_LED = tg::board::HAS_RGB_LED ? 38 : -1;

// Passive EC11 contacts and normally open switches, all pulled up to 3.3 V.
// Left/right refer to physical positions on the assembled console.
inline constexpr int PIN_LEFT_A = tg::board::LCD_169 ? 43 : 9;
inline constexpr int PIN_LEFT_B = tg::board::LCD_169 ? 2 : 8;
inline constexpr int PIN_LEFT_BUTTON = tg::board::LCD_169 ? 3 : 7;
inline constexpr int PIN_RIGHT_A = tg::board::LCD_169 ? 16 : 4;
inline constexpr int PIN_RIGHT_B = tg::board::LCD_169 ? 17 : 5;
inline constexpr int PIN_RIGHT_BUTTON = tg::board::LCD_169 ? 18 : 6;
inline constexpr int LEFT_DIRECTION = tg::board::LCD_169 ? -1 : 1;
inline constexpr int RIGHT_DIRECTION = tg::board::LCD_169 ? -1 : 1;

// QMI8658 6-axis IMU on I2C.
inline constexpr int PIN_IMU_SDA = tg::board::LCD_169 ? 11 : 48;
inline constexpr int PIN_IMU_SCL = tg::board::LCD_169 ? 10 : 47;
inline constexpr std::uint8_t IMU_I2C_ADDRESS = 0x6B;

struct AxisMap {
    std::uint8_t source;
    float sign;
};

inline constexpr AxisMap SCREEN_X =
    tg::board::LCD_169 ? AxisMap{0, 1.0f} : AxisMap{1, 1.0f};
inline constexpr AxisMap SCREEN_Y =
    tg::board::LCD_169 ? AxisMap{1, 1.0f} : AxisMap{0, 1.0f};
inline constexpr AxisMap SCREEN_Z{2, 1.0f};

inline constexpr float axis(const float chip[3], AxisMap map) {
    return chip[map.source] * map.sign;
}

static_assert(PIN_LEFT_BUTTON <= 21 && PIN_RIGHT_BUTTON <= 21,
              "ESP32-S3 ext1 wake switches must be RTC GPIOs");
static_assert(PIN_LEFT_A != PIN_IMU_SDA && PIN_LEFT_B != PIN_IMU_SDA,
              "Encoder contacts must not share the I2C bus");

}  // namespace board
