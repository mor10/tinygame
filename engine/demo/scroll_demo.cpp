#include "scroll_demo.h"

#include <cstdio>

#include "../gfx/art.h"
#include "../gfx/font.h"
#include "../testcard/sample_art.h"
#include "../testcard/testcard.h"

namespace tg::demo {

namespace {

enum Tile : std::uint8_t { EMPTY = 0, GROUND = 1, BRICK = 2, CLOUD = 3 };
constexpr TileDef TILES[] = {{sample::GROUND, true}, {sample::BRICK, true}, {sample::CLOUD, false}};

constexpr int HERO_SPEED_X16 = 26;  // 1.625 px per tick, about 6 blocks per second
constexpr int HERO_BLOCKS = 2;      // the hero is 2 x 2 blocks
constexpr int SHADOW_AMOUNT = 80;

}  // namespace

bool ScrollDemo::init(int screen_w, int screen_h, View view, int palette) {
    palette_ = palette;
    if (!tiles_.build(TILES, 3, testcard::palette(palette), SCALE, true)) return false;
    StageRequest request;
    request.view = view;
    layout_ = layoutStage(screen_w, screen_h, tiles_.block(), request);

    ground_.assign(LEVEL_COLS * LEVEL_ROWS, EMPTY);
    sky_.assign(LEVEL_COLS * LEVEL_ROWS, EMPTY);
    for (int col = 0; col < LEVEL_COLS; ++col) {
        ground_[(LEVEL_ROWS - 1) * LEVEL_COLS + col] = GROUND;
        if (col % 12 >= 4 && col % 12 <= 7) ground_[6 * LEVEL_COLS + col] = BRICK;
        if (col % 16 >= 10 && col % 16 <= 12) ground_[3 * LEVEL_COLS + col] = BRICK;
        if (col % 7 == 2) sky_[1 * LEVEL_COLS + col] = CLOUD;
        if (col % 9 == 5) sky_[2 * LEVEL_COLS + col] = CLOUD;
    }

    hero_x16_ = 2 * tiles_.block() * 16;
    hero_dir_ = 1;
    ticks_ = 0;
    camera_ = Camera{};
    updateCamera();
    return true;
}

void ScrollDemo::updateCamera() {
    const int block = tiles_.block();
    FollowRules rules;
    rules.view_w = layout_.stage.w;
    rules.view_h = layout_.stage.h;
    rules.world_w = LEVEL_COLS * block;
    rules.world_h = LEVEL_ROWS * block;
    rules.dead_w = layout_.stage.w / 4;
    rules.dead_h = layout_.stage.h / 2;
    follow(camera_, heroX() + block, (LEVEL_ROWS - 2) * block, rules);
}

void ScrollDemo::tick() {
    const int block = tiles_.block();
    const int min_x16 = block * 16;
    const int max_x16 = (LEVEL_COLS - HERO_BLOCKS - 1) * block * 16;
    hero_x16_ += hero_dir_ * HERO_SPEED_X16;
    if (hero_x16_ >= max_x16) { hero_x16_ = max_x16; hero_dir_ = -1; }
    if (hero_x16_ <= min_x16) { hero_x16_ = min_x16; hero_dir_ = 1; }
    ++ticks_;
    updateCamera();
}

void ScrollDemo::draw(gfx::Canvas& canvas) const {
    const gfx::Palette& pal = testcard::palette(palette_);
    const int block = tiles_.block();
    const gfx::Rect& stage = layout_.stage;
    canvas.resetClip();
    canvas.fill(pal.background);

    const Layer sky{LEVEL_COLS, LEVEL_ROWS, sky_.data(), 128};
    const Layer ground{LEVEL_COLS, LEVEL_ROWS, ground_.data(), 256};
    drawLayer(canvas, tiles_, sky, stage, camera_.x, camera_.y);
    drawLayer(canvas, tiles_, ground, stage, camera_.x, camera_.y);

    canvas.setClip(stage);
    const int hero_x = stage.x + heroX() - camera_.x;
    const int ground_y = stage.y + (LEVEL_ROWS - 1) * block - camera_.y;
    canvas.shade(hero_x + 2 * SCALE, ground_y, 12 * SCALE, SCALE, SHADOW_AMOUNT);
    gfx::DrawOptions hero;
    hero.scale = SCALE;
    hero.flip_x = hero_dir_ < 0;
    gfx::drawArt(canvas, sample::HERO, pal, hero_x, ground_y - HERO_BLOCKS * block, hero);
    canvas.resetClip();

    // Status area: a darker band with the tick and camera position.
    canvas.shade(layout_.status.x, layout_.status.y, layout_.status.w, layout_.status.h, 70);
    char line[64];
    std::snprintf(line, sizeof(line), "TICK %lu  CAM %d", static_cast<unsigned long>(ticks_),
                  camera_.x);
    gfx::drawText(canvas, layout_.status.x + 3, layout_.status.y + 3, line, pal.ink, 1);
}

}  // namespace tg::demo
