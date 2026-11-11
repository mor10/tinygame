#pragma once

// Carried over from v1.0 src/hal/display.h.
// Changed: the LVGL handle (lvDisplay) is gone. v2's engine draws a whole
// RGB565 frame itself, so the driver gains present() and presentMicros().
// setBrightness() and prepareSleep() are as in v1.

#include <cstdint>

namespace display {

bool initialize();

// Sends one full frame to the panel and returns when it has been written.
// `frame` is display_config::WIDTH * display_config::HEIGHT RGB565 pixels,
// row by row from the top-left, exactly as the engine stores them.
void present(const std::uint16_t* frame);
// How long the most recent present() took, in microseconds (0 before the first).
std::uint32_t presentMicros();

// Sets backlight brightness, 0-100 percent.
void setBrightness(std::uint8_t percent);
// Turns off backlight and panel; allow 120 ms before entering deep sleep.
bool prepareSleep();

}  // namespace display
