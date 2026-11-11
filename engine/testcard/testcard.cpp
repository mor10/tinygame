#include "testcard.h"

#include <cstdio>

#include "../gfx/art.h"
#include "../gfx/font.h"
#include "sample_art.h"

namespace tg::testcard {

namespace {

using gfx::rgb;
using namespace sample;

// ---- Candidate palettes --------------------------------------------------------

constexpr gfx::Palette PALETTES[] = {
    {"POCKET",  // v1's dark "Pocket Arcade" colours
     {0, rgb(0x0B0E1A), rgb(0xF4F6FF), rgb(0xB8C4D9), rgb(0xFF4F64), rgb(0xFF9F1C),
      rgb(0xFFD166), rgb(0x5BE36B), rgb(0x1F8A5B), rgb(0x3A86FF), rgb(0x1E2A56),
      rgb(0x7B2D6B), rgb(0xFF6FD8), rgb(0x9A5A3C), rgb(0xFFB38A), rgb(0x56607A)},
     rgb(0x161B2E), rgb(0xF4F6FF)},
    {"BRIGHT",  // softer colours on a light sky
     {0, rgb(0x2B2D42), rgb(0xFFFFFF), rgb(0xC9D1E0), rgb(0xF25C54), rgb(0xF7934C),
      rgb(0xFFD23F), rgb(0x7BD389), rgb(0x3E9E6C), rgb(0x5BC0EB), rgb(0x3A6EA5),
      rgb(0x8E6BBF), rgb(0xF49AC2), rgb(0xA9714B), rgb(0xF2D0A4), rgb(0x5C6378)},
     rgb(0xBFE6F5), rgb(0x2B2D42)},
};
constexpr int PALETTE_COUNT = sizeof(PALETTES) / sizeof(PALETTES[0]);

// Scene positions in blocks. Columns are world columns; rows count up from the ground.
constexpr int HERO_COL = 2;
constexpr int PLATFORM_FIRST_COL = 5;
constexpr int PLATFORM_LAST_COL = 8;
constexpr int PLATFORM_ROWS_UP = 3;
constexpr int SLIME_COL = 10;
constexpr int SHADOW_AMOUNT = 80;

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

}  // namespace

int paletteCount() { return PALETTE_COUNT; }

const gfx::Palette& palette(int index) {
    return PALETTES[index < 0 || index >= PALETTE_COUNT ? 0 : index];
}

void render(gfx::Canvas& canvas, const Options& options) {
    const gfx::Palette& pal = palette(options.palette);
    const int scale = options.scale < 1 ? 1 : options.scale;
    const int block = ART_BLOCK * scale;
    const int width = canvas.width();
    const int height = canvas.height();
    const int ground_y = height - block;  // the ground is the bottom row

    gfx::DrawOptions solid;
    solid.scale = scale;
    solid.bevel = options.bevel;
    gfx::DrawOptions actor;
    actor.scale = scale;

    canvas.fill(pal.background);

    // World column -> screen x. The scroll is in screen pixels, so the scene
    // moves smoothly even though the art is coarse.
    const auto screenX = [&](int column) { return column * block - options.scroll_px; };
    const int first_col = floorDiv(options.scroll_px, block);
    const int last_col = floorDiv(options.scroll_px + width - 1, block);

    for (int col = first_col; col <= last_col; ++col) {
        gfx::drawArt(canvas, GROUND, pal, screenX(col), ground_y, solid);
    }
    for (int col = PLATFORM_FIRST_COL; col <= PLATFORM_LAST_COL; ++col) {
        gfx::drawArt(canvas, BRICK, pal, screenX(col), ground_y - PLATFORM_ROWS_UP * block, solid);
        gfx::drawArt(canvas, COIN, pal, screenX(col), ground_y - (PLATFORM_ROWS_UP + 1) * block, actor);
    }

    // Soft shadows on the ground, then the actors standing on them.
    canvas.shade(screenX(HERO_COL) + 2 * scale, ground_y, 12 * scale, scale, SHADOW_AMOUNT);
    canvas.shade(screenX(SLIME_COL) + scale, ground_y, 6 * scale, scale, SHADOW_AMOUNT);
    gfx::drawArt(canvas, HERO, pal, screenX(HERO_COL), ground_y - 2 * block, actor);
    gfx::DrawOptions facing_left = actor;
    facing_left.flip_x = true;
    gfx::drawArt(canvas, SLIME, pal, screenX(SLIME_COL), ground_y - block, facing_left);

    // Labels at three text sizes (screen resolution, not block resolution).
    char line[64];
    std::snprintf(line, sizeof(line), "S%d %s %s", scale, options.bevel ? "BEVEL" : "FLAT", pal.name);
    gfx::drawText(canvas, 3, 3, line, pal.ink, 2);
    std::snprintf(line, sizeof(line), "BLOCK %dPX  %dX%d BLOCKS", block, width / block, height / block);
    gfx::drawText(canvas, 3, 16, line, pal.ink, 1);
    gfx::drawText(canvas, 3, 25, "42", pal.ink, 3);

    // Palette swatches, top right.
    constexpr int SWATCH = 8;
    for (int i = 1; i < 16; ++i) {
        canvas.rect(width - (16 - i) * SWATCH - 2, 2, SWATCH, SWATCH, pal.color[i]);
    }
}

}  // namespace tg::testcard
