#pragma once
// 8-bit indexed rasterizer for the back buffer. Colours 0-15 are the game palette; 16-31 are
// reserved for the OS screens so a cart can never make an error screen unreadable.
#include <stdint.h>

enum {
    C_BLACK, C_NAVY, C_DGREY, C_GREY, C_WHITE, C_CREAM, C_CORAL, C_RED,
    C_YELLOW, C_ORANGE, C_GREEN, C_TEAL, C_MINT, C_BLUE, C_PINK, C_BROWN,
    OS_NAVY = 16, OS_CREAM, OS_CORAL, OS_WHITE, OS_YELLOW, OS_GREY, OS_TEAL,
};

void gfx_init_palette(void);
void gfx_clear(int c);
void gfx_pixel(int x, int y, int c);
void gfx_line(int x0, int y0, int x1, int y1, int c);
void gfx_rect(int x, int y, int w, int h, int c);
void gfx_fill_rect(int x, int y, int w, int h, int c);
int  gfx_text(const char *s, int x, int y, int c, int scale);   // returns the width drawn
int  gfx_text_width(const char *s, int scale);
