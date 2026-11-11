// Golden images: the scrolling demo must draw exactly the frames whose
// fingerprints are recorded in golden_scroll_demo.h. If a change to the
// renderer is intended, look at `tg.py demo` and run `tg.py golden --update`.

#include <cstring>
#include <vector>

#include "../core/hash.h"
#include "../demo/scroll_cases.h"
#include "../demo/scroll_demo.h"
#include "golden_scroll_demo.h"
#include "tg_test.h"

using namespace tg;

TG_TEST(scroll_demo_matches_golden_frames) {
    const int cases = sizeof(demo::SCROLL_CASES) / sizeof(demo::SCROLL_CASES[0]);
    const int goldens = sizeof(golden::SCROLL_DEMO) / sizeof(golden::SCROLL_DEMO[0]);
    CHECK_EQ(cases, goldens);
    for (int i = 0; i < cases && i < goldens; ++i) {
        const demo::ScrollCase& c = demo::SCROLL_CASES[i];
        CHECK(std::strcmp(c.name, golden::SCROLL_DEMO[i].name) == 0);
        std::vector<gfx::Color> pixels(static_cast<std::size_t>(c.width) * c.height);
        gfx::Canvas canvas({pixels.data(), c.width, c.height, c.width});
        demo::ScrollDemo scene;
        CHECK(scene.init(c.width, c.height, c.view, 1));
        for (int t = 0; t < c.tick; ++t) scene.tick();
        scene.draw(canvas);
        const std::uint64_t hash = hashPixels(pixels.data(), pixels.size());
        if (hash != golden::SCROLL_DEMO[i].hash) {
            tg::test::fail(__FILE__, __LINE__, std::string("frame differs from golden: ") + c.name);
        }
    }
}

TG_TEST(scroll_demo_camera_follows_and_stays_in_the_level) {
    demo::ScrollDemo scene;
    CHECK(scene.init(320, 172, View::Standard, 0));
    CHECK_EQ(scene.camera().x, 0);
    const int max_camera = demo::ScrollDemo::LEVEL_COLS * 16 - 320;
    bool moved = false, reached_end = false;
    for (int t = 0; t < 1500; ++t) {
        scene.tick();
        CHECK(scene.camera().x >= 0 && scene.camera().x <= max_camera);
        moved = moved || scene.camera().x > 0;
        reached_end = reached_end || scene.camera().x == max_camera;
    }
    CHECK(moved);
    CHECK(reached_end);
    CHECK_EQ(scene.ticks(), 1500u);
}

int main() { return tg::test::run(); }
