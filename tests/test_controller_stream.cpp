#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>

#include "controller_stream.h"
#include "input_logic.h"

int main() {
    using namespace controller_stream;
    State state;
    Sample sample{INT32_MIN, true, INT32_MAX, false, -0.5f, 0.25f};
    char line[128];
    assert(format(line, sizeof(line), sample));
    assert(std::strcmp(line, "IN -2147483648 1 2147483647 0 -0.500000 0.250000\n") == 0);
    assert(!format(line, 3, sample));
    assert(!format(line, 0, sample));
    sample.tilt_x = std::numeric_limits<float>::quiet_NaN();
    assert(!format(line, sizeof(line), sample));
    sample.tilt_x = std::numeric_limits<float>::infinity();
    assert(!format(line, sizeof(line), sample));
    sample.tilt_x = -0.5f;

    assert(!state.due(100, sample));
    assert(!state.ping(100));
    state.start(100);
    assert(state.due(100, sample));
    state.sent(100, sample);
    sample.left_total++;
    assert(!state.due(119, sample));
    assert(state.due(120, sample));
    sample.left_down = false;
    assert(state.due(101, sample));  // button edges bypass the periodic interval
    state.sent(101, sample);
    sample.right_down = true;
    assert(state.due(102, sample));
    assert(state.check(3099, true));
    assert(!state.check(3100, true));
    assert(!state.due(3100, sample));
    state.start(5000);
    assert(state.ping(7999));
    assert(state.check(10998, true));
    assert(!state.check(10999, true));
    state.start(12000);
    assert(!state.check(12001, false));

    state.start(UINT32_MAX - 10);
    state.sent(UINT32_MAX - 10, sample);
    assert(!state.due(8, sample));
    assert(state.due(9, sample));
    assert(state.check(2988, true));
    assert(!state.check(2989, true));
    state.start(0);
    state.stop();
    assert(!state.active());

    input::Button physical;
    physical.initialize(false, 0);
    sample.left_down = false;
    state.start(0);
    state.sent(0, sample);
    physical.update(true, 1);
    assert(!physical.down);  // the physical HAL's debounce has not completed
    sample.left_down = true;  // stream raw press immediately, not physical.down
    assert(state.due(1, sample));
    state.sent(1, sample);
    physical.update(false, 2);
    sample.left_down = false;
    assert(state.due(2, sample));  // even raw bounce edges reach the simulator
    state.sent(2, sample);
    physical.update(true, 3);
    sample.left_down = true;
    assert(state.due(3, sample));
    std::puts("controller stream tests passed");
}
