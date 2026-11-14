#pragma once
#include <stdint.h>
// 5x7 bitmap font, column-major, LSB = top row. Returns NULL for characters it doesn't have.
const uint8_t *font_glyph(char c);
