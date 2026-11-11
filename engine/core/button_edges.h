#pragma once

// Turns "which knobs are held right now" into the press, release and click
// edges of tg::Input. Used by the flat C interface (engine/engine_c.cpp).

#include <cstdint>

#include "input.h"

namespace tg {

class ButtonEdges {
public:
    // buttons: bit 0 = left knob held, bit 1 = right knob held. The main knob
    // is the right one, or the left one when left_handed. A click is a release.
    Input step(std::int32_t left_delta, std::int32_t right_delta, int buttons, float tilt_x,
               float tilt_y, bool left_handed) {
        Input in;
        const bool l = (buttons & 1) != 0;
        const bool r = (buttons & 2) != 0;
        in.left = {left_delta, l};
        in.right = {right_delta, r};
        in.main = left_handed ? in.left : in.right;
        in.down = l || r;
        in.pressed = in.down && !was_down_;
        in.released = !in.down && was_down_;
        in.click = in.released;
        in.tilt_x = tilt_x;
        in.tilt_y = tilt_y;
        was_down_ = in.down;
        return in;
    }
    void reset() { was_down_ = false; }

private:
    bool was_down_ = false;
};

}  // namespace tg
