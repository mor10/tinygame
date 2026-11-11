// New in v2: tests for os/input_adapter.h (adapted from v1 src/os/input_adapter.h).
#include "../../engine/test/tg_test.h"

#include "input_adapter.h"

namespace {
input::State hardware(int left_delta, int right_delta, bool left_down, bool right_down,
                      std::uint8_t left_events = 0, std::uint8_t right_events = 0) {
    input::State s;
    s.left.delta = left_delta;
    s.right.delta = right_delta;
    s.left.button.down = left_down;
    s.right.button.down = right_down;
    s.left.button.events = left_events;
    s.right.button.events = right_events;
    return s;
}
}  // namespace

TG_TEST(main_knob_is_right_unless_left_handed) {
    input_adapter::Adapter a;
    const tg::Input right = a.read(hardware(5, -2, false, false), 0.1f, -0.2f);
    CHECK_EQ(right.main.delta, -2);
    CHECK_EQ(right.left.delta, 5);
    CHECK(right.tilt_x == 0.1f && right.tilt_y == -0.2f);
    const tg::Input left = a.read(hardware(5, -2, false, false), 0, 0, true);
    CHECK_EQ(left.main.delta, 5);
}

TG_TEST(press_release_and_click_merge_across_knobs) {
    input_adapter::Adapter a;
    tg::Input in = a.read(hardware(0, 0, false, true), 0, 0);
    CHECK(in.down && in.pressed && !in.released && !in.click);
    in = a.read(hardware(0, 0, false, true), 0, 0);
    CHECK(in.down && !in.pressed);
    in = a.read(hardware(0, 0, false, false, 0,
                         input::EVENT_RELEASED | input::EVENT_SHORT_PRESS), 0, 0);
    CHECK(!in.down && in.released && in.click);
    in = a.read(hardware(0, 0, false, false), 0, 0);
    CHECK(!in.released && !in.click);
}

TG_TEST(hiding_buttons_keeps_turns) {
    input_adapter::Adapter a;
    tg::Input in = a.read(hardware(3, 4, true, true), 0, 0);
    input_adapter::hideButtons(in);
    CHECK(!in.down && !in.pressed && !in.left.down && !in.main.down);
    CHECK_EQ(in.main.delta, 4);
}

int main() { return tg::test::run(); }
