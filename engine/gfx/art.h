#pragma once

// Text art: one character per art pixel, '.' is transparent and a hex digit is
// a palette index. This is the form kids and agents write. (Packed 4-bit data
// produced by `tg pack` replaces the text form on the console later.)

#include "canvas.h"
#include "palette.h"

namespace tg::gfx {

struct Art {
    int width;
    int height;
    const char* pixels;  // width * height characters, row by row
};

// Palette index for one art character, or -1 if the character is not allowed.
int artIndex(char c);
// True when the art has exactly width * height valid characters.
bool artValid(const Art& art);

struct DrawOptions {
    int scale = 2;        // screen pixels per art pixel
    bool flip_x = false;  // face the other way
    bool bevel = false;   // one-screen-pixel light and shade edge (solid blocks)
};

// Draws art with its top-left corner at screen position (x, y).
void drawArt(Canvas& canvas, const Art& art, const Palette& palette, int x, int y,
             const DrawOptions& options);

}  // namespace tg::gfx
