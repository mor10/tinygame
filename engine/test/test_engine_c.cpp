// The flat C interface: button edges, handedness, and a short run.

#include <cstring>

#include "../core/button_edges.h"
#include "../engine_c.h"
#include "bundle_builder.h"
#include "tg_test.h"

using namespace tg;

TG_TEST(press_and_release_edges_come_from_the_button_bits) {
    ButtonEdges e;
    Input in = e.step(0, 0, 0, 0, 0, false);
    CHECK(!in.pressed && !in.released && !in.click && !in.down);
    in = e.step(0, 0, 2, 0, 0, false);
    CHECK(in.pressed && in.down && !in.released && !in.click);
    in = e.step(0, 0, 2, 0, 0, false);  // still held: no new edge
    CHECK(!in.pressed && in.down && !in.released);
    in = e.step(0, 0, 0, 0, 0, false);
    CHECK(in.released && in.click && !in.down && !in.pressed);
    in = e.step(0, 0, 0, 0, 0, false);
    CHECK(!in.released && !in.click);
}

TG_TEST(the_main_knob_is_right_or_left_when_left_handed) {
    ButtonEdges e;
    Input in = e.step(-3, 5, 2, 0.5f, -0.25f, false);
    CHECK_EQ(in.main.delta, 5);
    CHECK(in.main.down);
    CHECK_EQ(in.left.delta, -3);
    CHECK(!in.left.down && in.right.down);
    CHECK_EQ(in.tilt_x, 0.5f);
    CHECK_EQ(in.tilt_y, -0.25f);
    in = e.step(-3, 5, 1, 0, 0, true);
    CHECK_EQ(in.main.delta, -3);
    CHECK(in.main.down);
    CHECK(!in.pressed);  // a knob was already held; another one changing is no new press
}

TG_TEST(c_interface_loads_ticks_and_draws) {
    const test::Bytes b = test::caveHopBundle();
    CHECK(tg_engine_load(b.data(), static_cast<uint32_t>(b.size()), 320, 172) == 1);
    CHECK_EQ(std::string(tg_engine_error()), "");
    for (int i = 0; i < 20; ++i) tg_engine_tick(0, 0, 0, 1.0f, 0, 0);
    CHECK(tg_engine_draw() != nullptr);
    CHECK(std::strncmp(tg_engine_probe(), "tick 20\n", 8) == 0);
    const uint8_t junk[4] = {1, 2, 3, 4};
    CHECK(tg_engine_load(junk, 4, 320, 172) == 0);
    CHECK(std::strlen(tg_engine_error()) > 0);
    CHECK(tg_engine_draw() == nullptr);
}

int main() { return tg::test::run(); }
