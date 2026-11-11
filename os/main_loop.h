#pragma once

// New in v2. The first-cut firmware loop: engine scrolling demo on Board A.
// Replaces v1's src/os/os.cpp (LVGL shell, games, dev protocol), which is not
// part of this cut.

namespace main_loop {

// Brings up serial, power, input, LED, IMU and display, allocates the frame
// buffer and starts the watchdog. If something essential fails it prints why
// and loop() then only feeds the watchdog and idles.
void setup();

// One pass of the main loop.
void loop();

}  // namespace main_loop
