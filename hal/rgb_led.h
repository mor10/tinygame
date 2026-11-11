#pragma once

// Carried over from v1.0 src/hal/rgb_led.h.
// Unchanged apart from this comment.

#include <cstdint>

// Onboard RGB status LED driver.
namespace rgb_led {

// Turns the LED off.
void initialize();
// Sets the colour; writes to the LED only when the colour changes.
void set(std::uint8_t r, std::uint8_t g, std::uint8_t b);

}  // namespace rgb_led
