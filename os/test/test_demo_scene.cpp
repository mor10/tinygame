// New in v2: tests for os/demo_scene.h. Links the engine, like the engine tests.
#include "../../engine/test/tg_test.h"

#include <cstring>
#include <vector>

#include "demo_scene.h"

TG_TEST(scene_counts_input_and_draws_at_panel_size) {
    demo_scene::Scene scene;
    CHECK(scene.init(320, 172));
    tg::Input in;
    in.right.delta = in.main.delta = 4;
    in.left.delta = -1;
    in.click = true;
    scene.tick(in);
    scene.tick(tg::Input{});
    CHECK_EQ(scene.ticks(), 2u);
    char text[64];
    scene.text(text, sizeof(text));
    CHECK_EQ(std::string(text), std::string("L-1 R+4 M+4 C1 D0 T+0 +0"));

    std::vector<tg::gfx::Color> pixels(320 * 172, 0);
    tg::gfx::Canvas canvas({pixels.data(), 320, 172, 320});
    scene.draw(canvas);
    bool any = false;
    for (auto p : pixels) any = any || p != 0;
    CHECK(any);
}

int main() { return tg::test::run(); }
