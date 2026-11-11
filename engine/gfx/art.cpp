#include "art.h"

namespace tg::gfx {

namespace {
constexpr int BEVEL_LIGHT = 90;
constexpr int BEVEL_SHADE = 90;
}  // namespace

int artIndex(char c) {
    if (c == '.') return 0;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool artValid(const Art& art) {
    if (art.width <= 0 || art.height <= 0 || art.pixels == nullptr) return false;
    const int count = art.width * art.height;
    for (int i = 0; i < count; ++i) {
        if (art.pixels[i] == '\0' || artIndex(art.pixels[i]) < 0) return false;
    }
    return art.pixels[count] == '\0';
}

void drawArt(Canvas& canvas, const Art& art, const Palette& palette, int x, int y,
             const DrawOptions& options) {
    const int scale = options.scale < 1 ? 1 : options.scale;
    const int out_w = art.width * scale;
    const int out_h = art.height * scale;
    if (x >= canvas.width() || y >= canvas.height() || x + out_w <= 0 || y + out_h <= 0) return;
    for (int ay = 0; ay < art.height; ++ay) {
        for (int ax = 0; ax < art.width; ++ax) {
            const int source_x = options.flip_x ? art.width - 1 - ax : ax;
            const int index = artIndex(art.pixels[ay * art.width + source_x]);
            if (index <= 0) continue;
            const Color color = palette.color[index];
            const int px = x + ax * scale;
            const int py = y + ay * scale;
            canvas.rect(px, py, scale, scale, color);
            if (!options.bevel) continue;
            // The bevel follows the outer edge of the whole block, at screen
            // resolution, so coarse art still gets a fine edge.
            if (ay == 0) canvas.rect(px, py, scale, 1, lighten(color, BEVEL_LIGHT));
            if (ax == 0) canvas.rect(px, py, 1, scale, lighten(color, BEVEL_LIGHT));
            if (ay == art.height - 1) canvas.rect(px, py + scale - 1, scale, 1, darken(color, BEVEL_SHADE));
            if (ax == art.width - 1) canvas.rect(px + scale - 1, py, 1, scale, darken(color, BEVEL_SHADE));
        }
    }
}

}  // namespace tg::gfx
