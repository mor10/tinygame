#include <vector>

#include "../testcard/testcard.h"
#include "tg_test.h"

using namespace tg;

namespace {

constexpr gfx::Color GUARD = 0xDEAD;
constexpr int PAD = 8;

struct Screen {
    int width, height, stride;
    std::vector<gfx::Color> memory;
    Screen(int w, int h)
        : width(w), height(h), stride(w + PAD),
          memory(static_cast<std::size_t>(stride) * (h + 2 * PAD), GUARD) {}
    gfx::Surface surface() { return {memory.data() + PAD * stride, width, height, stride}; }
    bool intact() const {
        for (int y = 0; y < height + 2 * PAD; ++y)
            for (int x = 0; x < stride; ++x) {
                const bool visible = y >= PAD && y < height + PAD && x < width;
                if (!visible && memory[y * stride + x] != GUARD) return false;
            }
        return true;
    }
    bool fullyDrawn() const {
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (memory[(y + PAD) * stride + x] == GUARD) return false;
        return true;
    }
};

struct Size {
    int width, height;
};
constexpr Size BOARDS[] = {{320, 172}, {280, 240}};

}  // namespace

TG_TEST(every_variant_fills_the_screen_and_stays_inside_it) {
    for (const Size& board : BOARDS) {
        for (int pal = 0; pal < testcard::paletteCount(); ++pal) {
            for (int scale = 1; scale <= 3; ++scale) {
                for (int scroll : {0, 5, -7, 1000}) {
                    Screen screen(board.width, board.height);
                    gfx::Canvas canvas(screen.surface());
                    testcard::Options options;
                    options.scale = scale;
                    options.palette = pal;
                    options.scroll_px = scroll;
                    testcard::render(canvas, options);
                    CHECK(screen.intact());
                    CHECK(screen.fullyDrawn());
                }
            }
        }
    }
}

TG_TEST(rendering_is_deterministic) {
    Screen first(320, 172), second(320, 172);
    gfx::Canvas a(first.surface()), b(second.surface());
    testcard::render(a, {});
    testcard::render(b, {});
    CHECK(first.memory == second.memory);
}

TG_TEST(ground_is_the_bottom_row_on_both_boards) {
    for (const Size& board : BOARDS) {
        Screen screen(board.width, board.height);
        gfx::Canvas canvas(screen.surface());
        testcard::Options options;
        options.bevel = false;
        testcard::render(canvas, options);
        const gfx::Palette& pal = testcard::palette(0);
        // Bottom-left art pixel of the ground tile is brown (index d).
        CHECK_EQ(canvas.at(0, board.height - 1), pal.color[13]);
        // One block above the ground, far right, is open sky.
        CHECK_EQ(canvas.at(board.width - 1, board.height - 16 - 8), pal.background);
    }
}

TG_TEST(scrolling_moves_the_scene_by_single_screen_pixels) {
    Screen still(320, 172), moved(320, 172);
    gfx::Canvas a(still.surface()), b(moved.surface());
    testcard::Options options;
    options.bevel = false;
    testcard::render(a, options);
    options.scroll_px = 1;
    testcard::render(b, options);
    // The brick platform starts at world column 5 (x = 80 at scale 2).
    const int y = 172 - 16 - 3 * 16 + 1;
    CHECK(a.at(80, y) != a.at(79, y));
    CHECK_EQ(b.at(79, y), a.at(80, y));
}

TG_TEST(out_of_range_options_are_made_safe) {
    Screen screen(280, 240);
    gfx::Canvas canvas(screen.surface());
    testcard::Options options;
    options.scale = 0;
    options.palette = 99;
    testcard::render(canvas, options);
    CHECK(screen.intact());
    CHECK(screen.fullyDrawn());
}

int main() { return tg::test::run(); }
