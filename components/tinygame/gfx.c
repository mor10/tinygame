// 8-bit indexed rasterizer: clipped primitives and the 5x7 font, drawing into the back buffer.
#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "platform.h"
#include "font.h"

// Game palette (0-15), flat colours from the reference designs, then the OS copies (16-25).
static const uint32_t PALETTE[] = {
    0x000000, 0x1E2340, 0x3A3F4B, 0x8A8F99, 0xFFFFFF, 0xF7F0DC, 0xE8604C, 0xC0392B,
    0xF2B632, 0xF08A3C, 0x5FB04B, 0x3FA89A, 0x9FD8CB, 0x3B6FD1, 0xF29BB0, 0x8A5A3C,
    0x1E2340, 0xF7F0DC, 0xE8604C, 0xFFFFFF, 0xF2B632, 0x8A8F99, 0x3FA89A,
    0xC0392B, 0x9FD8CB, 0x3A3F4B,
};

uint32_t gfx_palette_rgb(int index)
{
    return index >= 0 && index < (int)(sizeof PALETTE / sizeof PALETTE[0]) ? PALETTE[index] : 0;
}

void gfx_init_palette(void)
{
    for (int i = 0; i < (int)(sizeof PALETTE / sizeof PALETTE[0]); i++) plat_set_color(i, PALETTE[i]);
    for (int i = 0; i < 16; i++) {   // dimmed copies: 30% of the colour over the OS navy
        uint32_t c = PALETTE[i], n = 0x1E2340, out = 0;
        for (int sh = 0; sh <= 16; sh += 8) {
            uint32_t a = (c >> sh) & 0xFF, b = (n >> sh) & 0xFF;
            out |= ((a * 3 + b * 7) / 10) << sh;
        }
        plat_set_color(OS_DIM + i, out);
    }
}

static inline uint8_t *fb(void) { return plat_framebuffer(); }

void gfx_clear(int c) { memset(fb(), c, SCREEN_W * SCREEN_H); }

void gfx_pixel(int x, int y, int c)
{
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) fb()[y * SCREEN_W + x] = (uint8_t)c;
}

void gfx_fill_rect(int x, int y, int w, int h, int c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;
    uint8_t *p = fb() + y * SCREEN_W + x;
    for (int r = 0; r < h; r++, p += SCREEN_W) memset(p, c, w);
}

void gfx_rect(int x, int y, int w, int h, int c)
{
    if (w <= 0 || h <= 0) return;
    gfx_fill_rect(x, y, w, 1, c);
    gfx_fill_rect(x, y + h - 1, w, 1, c);
    gfx_fill_rect(x, y, 1, h, c);
    gfx_fill_rect(x + w - 1, y, 1, h, c);
}

void gfx_line(int x0, int y0, int x1, int y1, int c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 4096; guard++) {   // longest on-screen line is well under this
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int gfx_text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? n * 6 * scale - scale : 0;
}

int gfx_text(const char *s, int x, int y, int c, int scale)
{
    int x0 = x;
    for (; *s; s++, x += 6 * scale) {
        const uint8_t *g = font_glyph(*s);
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if ((g[col] >> row) & 1) gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, c);
    }
    return x - x0 - (x > x0 ? scale : 0);
}

void gfx_blit(const uint8_t *px, int w, int h, int x, int y, int flip_x, int flip_y, int scale)
{
    uint8_t *f = fb();
    int dw = w * scale, dh = h * scale;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + dw > SCREEN_W ? SCREEN_W : x + dw, y1 = y + dh > SCREEN_H ? SCREEN_H : y + dh;
    for (int dy = y0; dy < y1; dy++) {
        int sy = (dy - y) / scale;
        if (flip_y) sy = h - 1 - sy;
        const uint8_t *row = px + sy * w;
        uint8_t *out = f + dy * SCREEN_W;
        for (int dx = x0; dx < x1; dx++) {
            int sx = (dx - x) / scale;
            uint8_t c = row[flip_x ? w - 1 - sx : sx];
            if (c != GFX_CLEAR) out[dx] = c;
        }
    }
}
