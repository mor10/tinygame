#pragma once

// Drawing for sprites that came out of a bundle: one palette index per pixel
// (0 is transparent). The text form in art.h is for built-in art; this is what
// a loaded game uses. Both take the same DrawOptions and draw the same pixels.

#include "../game/game_data.h"
#include "art.h"
#include "canvas.h"
#include "palette.h"

namespace tg::gfx {

// True when the sprite has a size and exactly width * height pixels, all 0-15.
bool spriteValid(const SpriteData& sprite);

// Draws a sprite with its top-left corner at screen position (x, y), inside
// the canvas clip. An invalid sprite draws nothing.
void drawSprite(Canvas& canvas, const SpriteData& sprite, const Palette& palette, int x, int y,
                const DrawOptions& options);

}  // namespace tg::gfx
