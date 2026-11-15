#pragma once
// Anti-aliased OS fonts (Space Grotesk, generated). Pixels are 2-bit coverage, packed row by row,
// each row padded to a byte. Only the OS screens use these; carts keep the 5x7 font.
#include <stdint.h>

typedef struct { uint8_t w, h; int8_t xoff, yoff; uint8_t adv; uint32_t off; } aa_glyph_t;
typedef struct { uint8_t cap, line; const aa_glyph_t *glyphs; const uint8_t *bits; } aa_font_t;

enum { FONT_S, FONT_M, FONT_L, FONT_XL };   // em 13, 16, 24, 32 px
extern const aa_font_t FONT_AA[4];
