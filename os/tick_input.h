#pragma once

// New in v2. The loop reads the knobs every pass, but the game ticks at a
// fixed 60 per second: a pass may run no tick, or several. This holds what was
// read until a tick takes it, so no turn or click is lost or delivered twice.

#include "../engine/core/input.h"
#include "input_logic.h"

namespace tick_input {

class Latch {
public:
    // Once per loop pass, with what was read on that pass.
    void add(const tg::Input& sample) {
        const std::int32_t main = input::boundedAdd(held_.main.delta, sample.main.delta);
        const std::int32_t left = input::boundedAdd(held_.left.delta, sample.left.delta);
        const std::int32_t right = input::boundedAdd(held_.right.delta, sample.right.delta);
        const bool pressed = held_.pressed || sample.pressed;
        const bool released = held_.released || sample.released;
        const bool click = held_.click || sample.click;
        held_ = sample;  // held state and tilt: the newest reading wins
        held_.main.delta = main;
        held_.left.delta = left;
        held_.right.delta = right;
        held_.pressed = pressed;
        held_.released = released;
        held_.click = click;
    }

    // Once per game tick. Turns and one-tick events are handed out once; a
    // second tick in the same pass sees the knobs still held but not moving.
    tg::Input take() {
        const tg::Input out = held_;
        held_.main.delta = held_.left.delta = held_.right.delta = 0;
        held_.pressed = held_.released = held_.click = false;
        return out;
    }

private:
    tg::Input held_;
};

}  // namespace tick_input
