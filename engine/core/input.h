#pragma once

// What a game can read from the player on one tick. Portable copy of the v1
// input model: two knobs (80 transitions per turn, clockwise positive), a
// click on either knob as the action, and tilt.

#include <cstdint>

namespace tg {

struct KnobInput {
    std::int32_t delta = 0;  // signed transitions this tick
    bool down = false;
};

struct Input {
    KnobInput main;   // right knob, or left when the player is left-handed
    KnobInput left;
    KnobInput right;
    bool down = false;      // either knob held
    bool pressed = false;   // went down this tick
    bool released = false;  // came up this tick
    bool click = false;     // short press completed this tick
    float tilt_x = 0.0f;    // -1..1, the way a marble would roll
    float tilt_y = 0.0f;
};

}  // namespace tg
