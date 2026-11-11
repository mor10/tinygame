// New in v2: tests for os/tick_input.h.
#include "../../engine/test/tg_test.h"

#include "tick_input.h"

namespace {
tg::Input sample(int main_delta, bool click = false, float tilt = 0.0f) {
    tg::Input in;
    in.main.delta = main_delta;
    in.right.delta = main_delta;
    in.click = click;
    in.pressed = click;
    in.tilt_x = tilt;
    return in;
}
}  // namespace

TG_TEST(turns_between_ticks_are_summed_and_handed_out_once) {
    tick_input::Latch latch;
    latch.add(sample(3));
    latch.add(sample(-1));
    CHECK_EQ(latch.take().main.delta, 2);
    CHECK_EQ(latch.take().main.delta, 0);
}

TG_TEST(a_click_is_delivered_to_exactly_one_tick) {
    tick_input::Latch latch;
    latch.add(sample(0, true));
    latch.add(sample(0, false));  // a later pass must not erase the click
    CHECK(latch.take().click);
    CHECK(!latch.take().click);
}

TG_TEST(tilt_and_held_state_follow_the_newest_reading) {
    tick_input::Latch latch;
    latch.add(sample(0, false, 0.2f));
    tg::Input held = sample(0, false, 0.5f);
    held.down = held.right.down = true;
    latch.add(held);
    const tg::Input out = latch.take();
    CHECK(out.tilt_x == 0.5f);
    CHECK(out.down && out.right.down);
    CHECK(latch.take().down);  // still held on the next tick
}

TG_TEST(turns_saturate_instead_of_wrapping) {
    tick_input::Latch latch;
    latch.add(sample(INT32_MAX));
    latch.add(sample(10));
    CHECK_EQ(latch.take().main.delta, INT32_MAX);
}

int main() { return tg::test::run(); }
