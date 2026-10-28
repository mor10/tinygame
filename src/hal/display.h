#pragma once

#include <lvgl.h>

#include <cstdint>

namespace display {

bool initialize();
lv_display_t* lvDisplay();

// Sets backlight brightness, 0-100 percent.
void setBrightness(std::uint8_t percent);
// Turns off backlight and panel; allow 120 ms before entering deep sleep.
bool prepareSleep();

}  // namespace display
