#include <vector>

#include "../gfx/art.h"
#include "../gfx/canvas.h"
#include "../gfx/font.h"
#include "tg_test.h"

using namespace tg::gfx;

namespace {

constexpr Color GUARD = 0xDEAD;
constexpr Color RED = rgb(0xFF0000);
constexpr Color BLUE = rgb(0x0000FF);

// A surface surrounded by guard pixels: any write outside the visible area
// (above, below, or into the stride padding) changes a guard and is caught.
struct Guarded {
    static constexpr int PAD = 4;
    int width, height, stride;
    std::vector<Color> memory;
    Guarded(int w, int h)
        : width(w), height(h), stride(w + PAD),
          memory(static_cast<std::size_t>(stride) * (h + 2 * PAD), GUARD) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) memory[(y + PAD) * stride + x] = 0;
    }
    Surface surface() { return {memory.data() + PAD * stride, width, height, stride}; }
    bool intact() const {
        for (int y = 0; y < height + 2 * PAD; ++y)
            for (int x = 0; x < stride; ++x) {
                const bool visible = y >= PAD && y < height + PAD && x < width;
                if (!visible && memory[y * stride + x] != GUARD) return false;
            }
        return true;
    }
};

Palette testPalette() {
    Palette p{"TEST", {}, 0, 0};
    p.color[1] = RED;
    p.color[2] = BLUE;
    return p;
}

}  // namespace

TG_TEST(color_packs_rgb565) {
    CHECK_EQ(rgb(0xFFFFFF), Color{0xFFFF});
    CHECK_EQ(rgb(0xFF0000), Color{0xF800});
    CHECK_EQ(rgb(0x00FF00), Color{0x07E0});
    CHECK_EQ(rgb(0x0000FF), Color{0x001F});
    CHECK_EQ(mix(RED, BLUE, 0), RED);
    CHECK_EQ(mix(RED, BLUE, 256), BLUE);
    CHECK_EQ(darken(Color{0xFFFF}, 256), Color{0});
}

TG_TEST(rect_is_clipped_on_every_side) {
    Guarded g(8, 6);
    Canvas canvas(g.surface());
    canvas.rect(-3, -3, 100, 100, RED);
    CHECK(g.intact());
    CHECK_EQ(canvas.at(0, 0), RED);
    CHECK_EQ(canvas.at(7, 5), RED);
    canvas.rect(20, 20, 5, 5, BLUE);   // fully outside
    canvas.rect(2, 2, -4, 3, BLUE);    // negative size
    canvas.pixel(-1, 0, BLUE);
    canvas.pixel(8, 0, BLUE);
    CHECK(g.intact());
    CHECK_EQ(canvas.at(2, 2), RED);
    CHECK_EQ(canvas.at(99, 99), Color{0});
}

TG_TEST(shade_darkens_only_its_area) {
    Guarded g(4, 4);
    Canvas canvas(g.surface());
    canvas.fill(Color{0xFFFF});
    canvas.shade(1, 1, 2, 2, 256);
    CHECK_EQ(canvas.at(1, 1), Color{0});
    CHECK_EQ(canvas.at(2, 2), Color{0});
    CHECK_EQ(canvas.at(0, 0), Color{0xFFFF});
    CHECK_EQ(canvas.at(3, 3), Color{0xFFFF});
    canvas.shade(-5, -5, 50, 50, 10);
    CHECK(g.intact());
}

TG_TEST(art_validation) {
    CHECK(artValid({2, 2, "1.f0"}));
    CHECK(!artValid({2, 2, "1.f"}));     // too short
    CHECK(!artValid({2, 2, "1.f00"}));   // too long
    CHECK(!artValid({2, 2, "1.g0"}));    // bad character
    CHECK(!artValid({0, 2, ""}));
    CHECK_EQ(artIndex('.'), 0);
    CHECK_EQ(artIndex('a'), 10);
    CHECK_EQ(artIndex('A'), -1);
}

TG_TEST(art_scales_flips_and_keeps_transparency) {
    Guarded g(8, 4);
    Canvas canvas(g.surface());
    const Palette pal = testPalette();
    const Art art{2, 1, "1."};
    canvas.fill(Color{0x1234});
    DrawOptions options;
    options.scale = 3;
    drawArt(canvas, art, pal, 1, 0, options);
    CHECK_EQ(canvas.at(1, 0), RED);
    CHECK_EQ(canvas.at(3, 2), RED);
    CHECK_EQ(canvas.at(4, 0), Color{0x1234});  // transparent pixel left alone
    CHECK_EQ(canvas.at(0, 0), Color{0x1234});
    canvas.fill(Color{0x1234});
    options.flip_x = true;
    drawArt(canvas, art, pal, 1, 0, options);
    CHECK_EQ(canvas.at(1, 0), Color{0x1234});
    CHECK_EQ(canvas.at(4, 0), RED);
    drawArt(canvas, art, pal, -100, -100, options);
    drawArt(canvas, art, pal, 6, 3, options);  // hangs off the corner
    CHECK(g.intact());
}

TG_TEST(bevel_touches_only_the_outer_edge) {
    Guarded g(16, 16);
    Canvas canvas(g.surface());
    const Palette pal = testPalette();
    const Art art{2, 2, "1111"};
    DrawOptions options;
    options.scale = 4;
    options.bevel = true;
    drawArt(canvas, art, pal, 0, 0, options);
    CHECK_EQ(canvas.at(3, 0), lighten(RED, 90));   // top edge
    CHECK_EQ(canvas.at(0, 3), lighten(RED, 90));   // left edge
    CHECK_EQ(canvas.at(3, 7), darken(RED, 90));    // bottom edge
    CHECK_EQ(canvas.at(7, 3), darken(RED, 90));    // right edge
    CHECK_EQ(canvas.at(3, 3), RED);                // interior
    CHECK_EQ(canvas.at(4, 4), RED);                // seam between art pixels is not bevelled
    CHECK(g.intact());
}

TG_TEST(font_draws_known_glyph) {
    Guarded g(12, 12);
    Canvas canvas(g.surface());
    drawText(canvas, 0, 0, "A", RED, 1);
    // A = .#. / #.# / ### / #.# / #.#
    CHECK_EQ(canvas.at(1, 0), RED);
    CHECK_EQ(canvas.at(0, 0), Color{0});
    CHECK_EQ(canvas.at(0, 2), RED);
    CHECK_EQ(canvas.at(1, 2), RED);
    CHECK_EQ(canvas.at(1, 3), Color{0});
    CHECK_EQ(glyph('a'), glyph('A'));
    CHECK_EQ(glyph('~'), 0);
    CHECK_EQ(textWidth("", 2), 0);
    CHECK_EQ(textWidth("AB", 2), 14);
    drawText(canvas, 9, 9, "WWWW", BLUE, 3);  // runs off the surface
    CHECK(g.intact());
}

int main() { return tg::test::run(); }
