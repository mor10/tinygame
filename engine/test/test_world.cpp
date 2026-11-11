#include <vector>

#include "../core/fixed_step.h"
#include "../core/random.h"
#include "../testcard/sample_art.h"
#include "../world/camera.h"
#include "../world/stage.h"
#include "../world/tilemap.h"
#include "tg_test.h"

using namespace tg;

namespace {

constexpr gfx::Color GUARD = 0xDEAD;
constexpr gfx::Color RED = gfx::rgb(0xFF0000);
constexpr gfx::Color BLUE = gfx::rgb(0x0000FF);
constexpr gfx::Color BACK = 0x1234;

struct Screen {
    static constexpr int PAD = 4;
    int width, height, stride;
    std::vector<gfx::Color> memory;
    Screen(int w, int h)
        : width(w), height(h), stride(w + PAD),
          memory(static_cast<std::size_t>(stride) * (h + 2 * PAD), GUARD) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) memory[(y + PAD) * stride + x] = BACK;
    }
    gfx::Surface surface() { return {memory.data() + PAD * stride, width, height, stride}; }
    bool intact() const {
        for (int y = 0; y < height + 2 * PAD; ++y)
            for (int x = 0; x < stride; ++x) {
                const bool visible = y >= PAD && y < height + PAD && x < width;
                if (!visible && memory[y * stride + x] != GUARD) return false;
            }
        return true;
    }
};

gfx::Palette twoColours() {
    gfx::Palette p{"TEST", {}, 0, 0};
    p.color[1] = RED;
    p.color[2] = BLUE;
    return p;
}

// Tile 1 is solid red, tile 2 is blue with a transparent left half.
constexpr TileDef TILES[] = {{{2, 2, "1111"}, true}, {{2, 2, ".2.2"}, false}};

}  // namespace

TG_TEST(fixed_step_is_exactly_sixty_per_second) {
    FixedStep clock;
    int ticks = 0;
    for (int i = 0; i < 1000; ++i) ticks += clock.advanceMicros(1000);  // one second in 1 ms frames
    CHECK_EQ(ticks, 60);
    FixedStep slow;
    CHECK_EQ(slow.advanceMicros(16000), 0);   // not yet a full tick
    CHECK_EQ(slow.advanceMicros(1000), 1);
    FixedStep stalled;
    CHECK_EQ(stalled.advanceMicros(2'000'000), FixedStep::MAX_TICKS);  // catches up, bounded
    CHECK_EQ(stalled.advanceMicros(0), 0);                              // the backlog was dropped
}

TG_TEST(random_repeats_for_a_seed) {
    Random a(42), b(42), c(43);
    bool differs = false;
    for (int i = 0; i < 20; ++i) {
        const auto value = a.next();
        CHECK_EQ(value, b.next());
        differs = differs || value != c.next();
    }
    CHECK(differs);
    Random zero(0);
    CHECK(zero.next() != 0u);
    CHECK_EQ(zero.below(0), 0u);
    for (int i = 0; i < 50; ++i) CHECK(zero.below(6) < 6u);
}

TG_TEST(stage_layout_board_a) {
    const StageLayout l = layoutStage(320, 172, 16, {});
    CHECK_EQ(l.stage.y, 12);
    CHECK_EQ(l.stage.h, 160);
    CHECK_EQ(l.stage.w, 320);
    CHECK_EQ(l.status.h, 12);
    CHECK(!l.status_overlays);
    CHECK(l.stage.w / l.block >= MIN_STAGE_COLS);
    StageRequest fill;
    fill.view = View::Fill;
    const StageLayout f = layoutStage(320, 172, 16, fill);  // no room for an extra row
    CHECK_EQ(f.stage.h, 160);
    CHECK(!f.status_overlays);
}

TG_TEST(stage_layout_board_b) {
    const StageLayout l = layoutStage(280, 240, 16, {});
    CHECK_EQ(l.status.h, 80);
    CHECK_EQ(l.stage.y, 80);
    CHECK_EQ(l.stage.h, 160);
    CHECK(l.stage.w / l.block >= MIN_STAGE_COLS);
    StageRequest fill;
    fill.view = View::Fill;
    const StageLayout f = layoutStage(280, 240, 16, fill);
    CHECK_EQ(f.stage.y, 0);
    CHECK_EQ(f.stage.h, 240);
    CHECK(f.status_overlays);
    CHECK_EQ(f.status.h, STATUS_STRIP_PX);
}

TG_TEST(stage_playfield_is_centred) {
    StageRequest request;
    request.playfield_cols = 12;
    request.playfield_rows = 8;
    const StageLayout l = layoutStage(320, 172, 16, request);
    CHECK_EQ(l.playfield.w, 192);
    CHECK_EQ(l.playfield.h, 128);
    CHECK_EQ(l.playfield.x, 64);
    CHECK_EQ(l.playfield.y, 12 + 16);
    const StageLayout whole = layoutStage(320, 172, 16, {});
    CHECK_EQ(whole.playfield.w, whole.stage.w);
    CHECK_EQ(whole.playfield.y, whole.stage.y);
}

TG_TEST(camera_dead_zone_follow_and_clamp) {
    FollowRules rules;
    rules.view_w = 100; rules.view_h = 50;
    rules.world_w = 400; rules.world_h = 50;
    rules.dead_w = 20; rules.dead_h = 10;
    Camera camera;
    camera.x = 100;
    follow(camera, 150, 25, rules);       // centre of the view: inside the dead zone
    CHECK_EQ(camera.x, 100);
    follow(camera, 159, 25, rules);       // still inside (dead zone is 140..160)
    CHECK_EQ(camera.x, 100);
    follow(camera, 165, 25, rules);       // 5 px past the edge
    CHECK_EQ(camera.x, 105);
    follow(camera, 120, 25, rules);       // back the other way
    CHECK_EQ(camera.x, 80);            // dead zone was 145..165: 25 px short
    follow(camera, -500, 25, rules);
    CHECK_EQ(camera.x, 0);
    follow(camera, 5000, 25, rules);
    CHECK_EQ(camera.x, 300);
    CHECK_EQ(camera.y, 0);                // world as tall as the view
}

TG_TEST(camera_centres_a_small_world) {
    FollowRules rules;
    rules.view_w = 100; rules.view_h = 240;
    rules.world_w = 60; rules.world_h = 160;
    Camera camera;
    follow(camera, 30, 80, rules);
    CHECK_EQ(camera.x, -20);
    CHECK_EQ(camera.y, -40);
}

TG_TEST(tile_cache_bakes_scale_bevel_and_mask) {
    const gfx::Palette pal = twoColours();
    TileCache cache;
    CHECK(cache.build(TILES, 2, pal, 4, true));
    CHECK_EQ(cache.block(), 8);
    CHECK_EQ(cache.count(), 2);
    CHECK(cache.mask(0) == nullptr);                       // fully solid
    CHECK(cache.mask(1) != nullptr);
    CHECK_EQ(cache.pixels(0)[0], gfx::lighten(RED, 90));   // bevel baked in
    CHECK_EQ(cache.pixels(0)[3 * 8 + 3], RED);
    CHECK_EQ(cache.pixels(1)[7], BLUE);                    // not solid: no bevel
    CHECK_EQ(cache.mask(1)[0], 0);
    CHECK_EQ(cache.mask(1)[4], 1);

    const TileDef uneven[] = {{{2, 2, "1111"}, true}, {{1, 1, "1"}, true}};
    CHECK(!cache.build(uneven, 2, pal, 4, true));
    CHECK_EQ(cache.count(), 0);
    const TileDef bad[] = {{{2, 2, "11x1"}, true}};
    CHECK(!cache.build(bad, 1, pal, 4, true));
    CHECK(!cache.build(TILES, 2, pal, 0, true));
}

TG_TEST(layer_draws_only_inside_the_viewport) {
    const gfx::Palette pal = twoColours();
    TileCache cache;
    CHECK(cache.build(TILES, 2, pal, 2, false));           // 4 px tiles
    const std::uint8_t cells[] = {1, 1, 1, 1, 1, 1,
                                  1, 1, 1, 1, 1, 1,
                                  1, 1, 1, 1, 1, 1};
    const Layer layer{6, 3, cells, 256};
    Screen screen(16, 12);
    gfx::Canvas canvas(screen.surface());
    const gfx::Rect viewport{4, 2, 8, 6};
    canvas.setClip({1, 1, 14, 10});
    drawLayer(canvas, cache, layer, viewport, 3, 1);
    CHECK(screen.intact());
    CHECK_EQ(canvas.at(4, 2), RED);
    CHECK_EQ(canvas.at(11, 7), RED);
    CHECK_EQ(canvas.at(3, 2), BACK);     // left of the viewport
    CHECK_EQ(canvas.at(12, 7), BACK);    // right of it
    CHECK_EQ(canvas.at(4, 1), BACK);     // above
    CHECK_EQ(canvas.at(4, 8), BACK);     // below
    CHECK_EQ(canvas.clipRect().x, 1);    // the caller's clip is restored
    CHECK_EQ(canvas.clipRect().w, 14);
}

TG_TEST(layer_scrolls_by_pixels_skips_empty_cells_and_keeps_transparency) {
    const gfx::Palette pal = twoColours();
    TileCache cache;
    CHECK(cache.build(TILES, 2, pal, 2, false));
    const std::uint8_t cells[] = {1, 0, 2, 9};             // 9 is not a tile: ignored
    const Layer layer{4, 1, cells, 256};
    Screen screen(16, 4);
    gfx::Canvas canvas(screen.surface());
    const gfx::Rect viewport{0, 0, 16, 4};
    drawLayer(canvas, cache, layer, viewport, 0, 0);
    CHECK_EQ(canvas.at(0, 0), RED);
    CHECK_EQ(canvas.at(4, 0), BACK);     // empty cell
    CHECK_EQ(canvas.at(8, 0), BACK);     // transparent half of tile 2
    CHECK_EQ(canvas.at(10, 0), BLUE);
    CHECK_EQ(canvas.at(12, 0), BACK);    // unknown tile number
    Screen moved(16, 4);
    gfx::Canvas second(moved.surface());
    drawLayer(second, cache, layer, viewport, 1, 0);
    CHECK_EQ(second.at(2, 0), RED);
    CHECK_EQ(second.at(3, 0), BACK);
    CHECK_EQ(second.at(9, 0), BLUE);
    Screen half(16, 4);
    gfx::Canvas third(half.surface());
    const Layer far{4, 1, cells, 128};   // half-speed layer
    drawLayer(third, cache, far, viewport, 2, 0);
    CHECK_EQ(third.at(2, 0), RED);
    CHECK_EQ(third.at(3, 0), BACK);
    drawLayer(third, cache, layer, viewport, -100, -100);  // camera far outside
    drawLayer(third, cache, layer, viewport, 9000, 0);
    CHECK(half.intact());
}

TG_TEST(blit_respects_clip_and_mask) {
    Screen screen(8, 8);
    gfx::Canvas canvas(screen.surface());
    const gfx::Color source[] = {RED, BLUE, RED, BLUE};
    const std::uint8_t mask[] = {1, 0, 0, 1};
    canvas.blit(source, mask, 2, 2, 3, 3);
    CHECK_EQ(canvas.at(3, 3), RED);
    CHECK_EQ(canvas.at(4, 3), BACK);
    CHECK_EQ(canvas.at(4, 4), BLUE);
    canvas.blit(source, nullptr, 2, 2, -1, -1);            // hangs off the corner
    CHECK_EQ(canvas.at(0, 0), BLUE);
    canvas.blit(source, nullptr, 2, 2, 7, 7);
    CHECK_EQ(canvas.at(7, 7), RED);
    canvas.setClip({0, 0, 2, 2});
    canvas.rect(0, 0, 8, 8, BLUE);
    canvas.pixel(5, 5, RED);
    CHECK_EQ(canvas.at(1, 1), BLUE);
    CHECK_EQ(canvas.at(2, 2), BACK);
    CHECK_EQ(canvas.at(5, 5), BACK);
    canvas.setClip({-50, -50, 500, 500});                  // trimmed to the surface
    canvas.fill(RED);
    CHECK(screen.intact());
    canvas.setClip({6, 6, -3, -3});                        // nothing left
    canvas.fill(BLUE);
    CHECK_EQ(canvas.at(6, 6), RED);
}

int main() { return tg::test::run(); }
