#include "sprite.h"

namespace tg::gfx {

namespace {
// The same edge strengths as drawArt, so a tile looks the same from either form.
constexpr int BEVEL_LIGHT = 90;
constexpr int BEVEL_SHADE = 90;
}  // namespace

bool spriteValid(const SpriteData& sprite) {
    if (sprite.width <= 0 || sprite.height <= 0) return false;
    if (sprite.pixels.size() != static_cast<std::size_t>(sprite.width) * sprite.height) return false;
    for (const std::uint8_t index : sprite.pixels) {
        if (index > 15) return false;
    }
    return true;
}

void drawSprite(Canvas& canvas, const SpriteData& sprite, const Palette& palette, int x, int y,
                const DrawOptions& options) {
    if (sprite.width <= 0 || sprite.height <= 0) return;
    if (sprite.pixels.size() != static_cast<std::size_t>(sprite.width) * sprite.height) return;
    const int scale = options.scale < 1 ? 1 : options.scale;
    const int out_w = sprite.width * scale;
    const int out_h = sprite.height * scale;
    if (x >= canvas.width() || y >= canvas.height() || x + out_w <= 0 || y + out_h <= 0) return;
    const std::uint8_t* pixels = sprite.pixels.data();
    for (int ay = 0; ay < sprite.height; ++ay) {
        for (int ax = 0; ax < sprite.width; ++ax) {
            const int source_x = options.flip_x ? sprite.width - 1 - ax : ax;
            const int index = pixels[ay * sprite.width + source_x] & 15;
            if (index == 0) continue;
            const Color color = palette.color[index];
            const int px = x + ax * scale;
            const int py = y + ay * scale;
            canvas.rect(px, py, scale, scale, color);
            if (!options.bevel) continue;
            if (ay == 0) canvas.rect(px, py, scale, 1, lighten(color, BEVEL_LIGHT));
            if (ax == 0) canvas.rect(px, py, 1, scale, lighten(color, BEVEL_LIGHT));
            if (ay == sprite.height - 1)
                canvas.rect(px, py + scale - 1, scale, 1, darken(color, BEVEL_SHADE));
            if (ax == sprite.width - 1)
                canvas.rect(px + scale - 1, py, 1, scale, darken(color, BEVEL_SHADE));
        }
    }
}

}  // namespace tg::gfx
