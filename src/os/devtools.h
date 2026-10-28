#pragma once

#include <cstdint>

// Line commands on the USB serial port so coding agents (via tools/tg.py)
// can see the screen and press buttons. Type `help` in a serial monitor.
// Every command answers with `[DEV] ok` or `[DEV] error: ...` (except `shot`,
// which sends `SHOT <w> <h> RGB565LE`, base64 rows, then `END`).
namespace devtools {

// Fake input set by commands; the OS applies it on top of the real controls.
struct EncoderOverride {
    std::int32_t delta = 0;
    bool down = false;
    std::uint8_t events = 0;
};

struct Overrides {
    bool tilt_active = false;
    float tilt_x = 0.0f;
    float tilt_y = 0.0f;
    EncoderOverride left;
    EncoderOverride right;
    int launch = -1;             // registry index requested by `launch`
    bool input_log = false;      // `log on`: print input once per second

    void clearFrame() {
        left.delta = right.delta = 0;
        left.events = right.events = 0;
        launch = -1;
    }
};

// Reads and runs one pending command per call. Call once per loop, before
// input is used.
void poll();
Overrides& overrides();
// Call at the end of each loop: clears one-frame events and requests.
void endFrame();

}  // namespace devtools
