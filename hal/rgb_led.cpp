// Carried over from v1.0 src/hal/rgb_led.cpp.
// Changed: includes "board.h" instead of "board_config.h", and does nothing on a
// board without an RGB LED (board::has_rgb_led).

#include "rgb_led.h"

#include <Arduino.h>

#include "board.h"

namespace rgb_led {

namespace {

std::uint8_t current_r = 0;
std::uint8_t current_g = 0;
std::uint8_t current_b = 0;

}  // namespace

void initialize() {
    if (!board::has_rgb_led) return;
    // Same call as the Waveshare RGB_lamp demo; the core handles GRB ordering.
    neopixelWrite(board::PIN_RGB_LED, 0, 0, 0);
}

void set(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (r == current_r && g == current_g && b == current_b) {
        return;
    }
    current_r = r;
    current_g = g;
    current_b = b;
    if (!board::has_rgb_led) return;
    neopixelWrite(board::PIN_RGB_LED, r, g, b);
}

}  // namespace rgb_led
