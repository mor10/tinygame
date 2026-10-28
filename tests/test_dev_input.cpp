#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "dev_input.h"

int main() {
    devtools::Overrides state;
    using devtools::CommandResult;
    assert(devtools::inputCommand("turn", "left 80", state) == CommandResult::Ok);
    assert(state.left.delta == 80 && state.right.delta == 0);
    assert(devtools::inputCommand("turn", "right -4", state) == CommandResult::Ok);
    assert(state.right.delta == -4);
    assert(devtools::inputCommand("click", nullptr, state) == CommandResult::Ok);
    assert(state.left.events & input::EVENT_SHORT_PRESS);
    assert(devtools::inputCommand("down", "right", state) == CommandResult::Ok);
    assert(state.right.down);
    assert(devtools::inputCommand("up", "right", state) == CommandResult::Ok);
    assert(!state.right.down);
    const auto before = state.left.delta;
    for (const char* bad : {"", "left", "centre 1", "left nan", "left 1.2",
                            "left 2147483648", "left 999999999999999999999",
                            "left 1 extra", "right -1000001"}) {
        assert(devtools::inputCommand("turn", bad, state) == CommandResult::Error);
        assert(state.left.delta == before);
    }
    assert(devtools::inputCommand("click", "left extra", state) == CommandResult::Error);
    assert(devtools::inputCommand("launch", "maze", state) == CommandResult::NotInput);
    state.clearFrame();
    assert(state.left.delta == 0 && state.right.events == 0);
    state.left.delta = INT32_MAX;
    assert(devtools::inputCommand("turn", "left 1", state) == CommandResult::Error);
    assert(state.left.delta == INT32_MAX);

    // tilt takes two numbers from -1 to 1, or "off"; bad input changes nothing.
    assert(devtools::inputCommand("tilt", "0.5 -0.25", state) == CommandResult::Ok);
    assert(state.tilt_active && state.tilt_x == 0.5f && state.tilt_y == -0.25f);
    for (const char* bad : {"", "0.5", "0.5 nan", "inf 0", "1.5 0", "0 -1.01", "0 0 0",
                            "x y", "0.5 0 extra", "0.5.5", "1-1", "0x1p-1"}) {
        assert(devtools::inputCommand("tilt", bad, state) == CommandResult::Error);
        assert(state.tilt_active && state.tilt_x == 0.5f && state.tilt_y == -0.25f);
    }
    assert(devtools::inputCommand("tilt", nullptr, state) == CommandResult::Error);
    assert(devtools::inputCommand("tilt", "off", state) == CommandResult::Ok);
    assert(!state.tilt_active);
    std::puts("dev input tests passed");
}
