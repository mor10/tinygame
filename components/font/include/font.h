#pragma once
#include <stdint.h>
// 5x7 bitmap font, column-major, LSB = top row. Characters it doesn't have come back as '?'.
const uint8_t *font_glyph(char c);
