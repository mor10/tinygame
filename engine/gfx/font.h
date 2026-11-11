#pragma once

// Tiny 3x5 pixel font (carried over from v1 TiltBlock). Upper case, digits and
// a little punctuation; lower case is drawn as upper case. A fuller font is a
// later work item; this one exists so the test card can label itself.

#include <cstdint>

#include "canvas.h"

namespace tg::gfx {

inline constexpr int GLYPH_W = 3;
inline constexpr int GLYPH_H = 5;
inline constexpr int GLYPH_ADVANCE = 4;

// Five rows of three bits; bit (row * 3 + 2 - col) is set when the pixel is on.
std::uint16_t glyph(char c);
// Width in screen pixels of `text` at `scale`.
int textWidth(const char* text, int scale);
void drawText(Canvas& canvas, int x, int y, const char* text, Color color, int scale);

}  // namespace tg::gfx
