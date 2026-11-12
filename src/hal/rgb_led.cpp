#include "rgb_led.h"

#include <Arduino.h>

#include "board_config.h"
#include "tg_board.h"

namespace rgb_led {

namespace {

std::uint8_t current_r = 0;
std::uint8_t current_g = 0;
std::uint8_t current_b = 0;

}  // namespace

void initialize() {
    if constexpr (!tg::board::HAS_RGB_LED) {
        Serial.println("[RGB] Not supported on this board");
        return;
    }
    // Same call as the Waveshare RGB_lamp demo; the core handles GRB ordering.
    neopixelWrite(board::PIN_RGB_LED, 0, 0, 0);
}

void set(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if constexpr (!tg::board::HAS_RGB_LED) return;
    if (r == current_r && g == current_g && b == current_b) {
        return;
    }
    current_r = r;
    current_g = g;
    current_b = b;
    neopixelWrite(board::PIN_RGB_LED, r, g, b);
}

}  // namespace rgb_led
