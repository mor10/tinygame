#pragma once

#include <cstdint>

namespace board {

// Physical panel
inline constexpr std::int16_t LCD_PHYSICAL_WIDTH = 172;
inline constexpr std::int16_t LCD_PHYSICAL_HEIGHT = 320;

// Waveshare ESP32-S3-LCD-1.47B LCD
inline constexpr int PIN_LCD_MOSI = 45;
inline constexpr int PIN_LCD_SCLK = 40;
inline constexpr int PIN_LCD_CS = 42;
inline constexpr int PIN_LCD_DC = 41;
inline constexpr int PIN_LCD_RESET = 39;
inline constexpr int PIN_LCD_BACKLIGHT = 46;

// Onboard addressable RGB LED (WS2812-type). Source: Waveshare wiki pinout
// "RGB_Control GPIO38" and demo RGB_lamp.cpp (#define PIN_NEOPIXEL 38).
inline constexpr int PIN_RGB_LED = 38;

// Passive EC11 contacts and normally open switches, all pulled up to 3.3 V.
// Left/right refer to physical positions on the assembled console.
inline constexpr int PIN_LEFT_A = 9;
inline constexpr int PIN_LEFT_B = 8;
inline constexpr int PIN_LEFT_BUTTON = 7;
inline constexpr int PIN_RIGHT_A = 4;
inline constexpr int PIN_RIGHT_B = 5;
inline constexpr int PIN_RIGHT_BUTTON = 6;
// Verify clockwise polarity on the mounted hardware.
inline constexpr int LEFT_DIRECTION = 1;
inline constexpr int RIGHT_DIRECTION = 1;

// QMI8658 6-axis IMU on I2C. Verified on the board with an I2C scan
// (WHO_AM_I register 0x00 reads 0x05 at address 0x6B).
inline constexpr int PIN_IMU_SDA = 48;
inline constexpr int PIN_IMU_SCL = 47;
inline constexpr std::uint8_t IMU_I2C_ADDRESS = 0x6B;

}  // namespace board
