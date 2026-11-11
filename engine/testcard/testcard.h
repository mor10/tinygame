#pragma once

// The test card: one small scene drawn at a chosen block size, palette and
// edge style, so people can judge on the real screen what reads well. It is
// the first user of the renderer and the basis of the M1 decision.

#include "../gfx/canvas.h"
#include "../gfx/palette.h"

namespace tg::testcard {

inline constexpr int ART_BLOCK = 8;  // art pixels per block

struct Options {
    int scale = 2;       // screen pixels per art pixel (block = 8 * scale)
    bool bevel = true;   // light and shade edge on solid blocks
    int palette = 0;     // 0 .. paletteCount() - 1
    int scroll_px = 0;   // world offset in screen pixels (shows smooth scrolling)
};

int paletteCount();
const gfx::Palette& palette(int index);
void render(gfx::Canvas& canvas, const Options& options);

}  // namespace tg::testcard
