#pragma once

// Adapted from v1.0 src/os/input_adapter.h.
// Same job: turn the debounced hardware state into what a game reads. Changes:
//  - the output is the engine's tg::Input (engine/core/input.h), which has no
//    held_ms, accelerometer or gyroscope fields, so those are not filled in;
//  - the serial dev-command overrides and the Diagnostics "evidence" counters
//    are left out until the dev protocol and shell return.
// The press / release / click rules per knob are v1's, unchanged.

#include "../engine/core/input.h"
#include "input.h"

namespace input_adapter {

class Adapter {
public:
    tg::Input read(const input::State& hw, float tilt_x, float tilt_y,
                   bool left_handed = false) {
        const Edges left = edges(hw.left, left_);
        const Edges right = edges(hw.right, right_);
        tg::Input in;
        in.left = {hw.left.delta, left.down};
        in.right = {hw.right.delta, right.down};
        // Main knob: right by default, left when left handed. The action
        // button fields merge both switches.
        in.main = left_handed ? in.left : in.right;
        in.down = left.down || right.down;
        in.pressed = left.pressed || right.pressed;
        in.released = left.released || right.released;
        in.click = left.click || right.click;
        in.tilt_x = tilt_x;
        in.tilt_y = tilt_y;
        return in;
    }

private:
    struct Edges {
        bool down = false;
        bool pressed = false;
        bool released = false;
        bool click = false;
    };
    static Edges edges(const input::Encoder& hw, bool& was_down) {
        Edges out;
        const auto events = hw.button.events;
        out.down = hw.button.down;
        out.pressed = out.down && !was_down;
        out.released = !out.down && (was_down || (events & input::EVENT_RELEASED));
        out.click = (events & input::EVENT_SHORT_PRESS) != 0;
        was_down = out.down;
        return out;
    }
    bool left_ = false;
    bool right_ = false;
};

// While the both-knobs sleep gesture owns the switches, a game must not see
// them (v1 did the same in os.cpp). Turning still gets through.
inline void hideButtons(tg::Input& in) {
    in.left.down = in.right.down = in.main.down = false;
    in.down = in.pressed = in.released = in.click = false;
}

}  // namespace input_adapter
