#pragma once
// 8-bit indexed rasterizer for the back buffer. Colours 0-15 are the game palette; 16-31 are
// reserved for the OS screens so a cart can never make an error screen unreadable.
#include <stdint.h>

enum {
    C_BLACK, C_NAVY, C_DGREY, C_GREY, C_WHITE, C_CREAM, C_CORAL, C_RED,
    C_YELLOW, C_ORANGE, C_GREEN, C_TEAL, C_MINT, C_BLUE, C_PINK, C_BROWN,
    // OS "Sunrise" colours, only ever used by the OS screens: plum ink, white, hot pink, purple,
    // sun yellow, teal, blue, orange, a muted rose for disabled items, and a warm blush.
    OS_INK = 16, OS_WHITE, OS_PINK, OS_PURPLE, OS_YELLOW, OS_TEAL, OS_BLUE, OS_ORANGE, OS_ROSE, OS_BLUSH,
    OS_DIM = 32,   // 32-47: game colours 0-15 darkened, for the pause screen
    OS_SKY = 48,   // 48-54: seven bands of the sky, hot pink at the top to sun yellow at the bottom
    OS_CUBE = 56,  // 56-64: three tones (top, left, right) for each of the three hero blocks
};

void gfx_init_palette(void);
uint32_t gfx_palette_rgb(int index);   // the built-in colour for index, 0xRRGGBB
void gfx_clear(int c);
void gfx_pixel(int x, int y, int c);
void gfx_line(int x0, int y0, int x1, int y1, int c);
void gfx_rect(int x, int y, int w, int h, int c);
void gfx_fill_rect(int x, int y, int w, int h, int c);
void gfx_circle(int cx, int cy, int r, int c, int filled);
int  gfx_text(const char *s, int x, int y, int c, int scale);   // returns the width drawn
int  gfx_text_width(const char *s, int scale);
#define GFX_CLEAR 0xFF   // see-through pixel in sprite data
void gfx_blit(const uint8_t *px, int w, int h, int x, int y, int flip_x, int flip_y, int scale);

// Anti-aliased OS text (Space Grotesk, see font_aa.h). y is the top of the capital letters. Edges are
// blended with whatever is already drawn, using palette slots 64+ that are allocated on demand.
int  gfx_aa_text(const char *s, int x, int y, int c, int font);   // returns the width drawn
int  gfx_aa_width(const char *s, int font);
int  gfx_aa_cap(int font);   // height of a capital letter
