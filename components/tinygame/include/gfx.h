#pragma once
// 8-bit indexed rasterizer for the back buffer. Colours 0-31 are the game palette (0-15 the original
// set, 16-31 brighter and purer ones). Everything from 32 up belongs to the OS screens, so a cart can
// never make an error screen unreadable.
#include <stdint.h>

enum {
    C_BLACK, C_NAVY, C_DGREY, C_GREY, C_WHITE, C_CREAM, C_CORAL, C_RED,
    C_YELLOW, C_ORANGE, C_GREEN, C_TEAL, C_MINT, C_BLUE, C_PINK, C_BROWN,
    C_BRIGHT_RED, C_BRIGHT_ORANGE, C_LEMON, C_LIME, C_BRIGHT_GREEN, C_TURQUOISE, C_SKY, C_BRIGHT_BLUE,
    C_DEEP_BLUE, C_VIOLET, C_MAGENTA, C_HOT_PINK, C_BUBBLEGUM, C_PALE_PINK, C_PEACH, C_PLUM,
    OS_DIM = 32,   // 32-63: game colours 0-31 darkened, for the pause screen
    // OS "Deep Toy Blue" colours, only ever used by the OS screens: navy ink and drop-edge, white, red,
    // purple, toy yellow, green, the electric blue field, orange, a muted blue for disabled text, a pale
    // blue, and a bright royal blue for dark stickers.
    OS_INK = 64, OS_WHITE, OS_RED, OS_PURPLE, OS_YELLOW, OS_GREEN, OS_BLUE, OS_ORANGE, OS_MUTED, OS_PALE, OS_ROYAL,
    OS_CUBE = 80,   // 80-88: three tones (top, left, right) for each of the three hero blocks
    OS_TINT = 90,   // 90-94: each cartridge colour 25% brighter, for the icon window
    OS_TINT2 = 95,  // 95-99: each cartridge colour 55% brighter, a lighter window for dark icons
    OS_SHADE = 100, // 100-105: drop-shadow tones, 20% darker than the red, yellow, green, purple and orange bodies, then the pale blue
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

int  gfx_color(uint32_t rgb888);   // a palette slot showing exactly this colour (allocated on demand; 0 if the palette is full)
