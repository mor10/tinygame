// 8-bit indexed rasterizer: clipped primitives and the 5x7 font, drawing into the back buffer.
#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "platform.h"
#include "font.h"
#include "font_aa.h"

// Game palette (0-15), flat colours from the reference designs, then the OS "Sunrise" colours (16-25).
static const uint32_t PALETTE[] = {
    0x000000, 0x1E2340, 0x3A3F4B, 0x8A8F99, 0xFFFFFF, 0xF7F0DC, 0xE8604C, 0xC0392B,
    0xF2B632, 0xF08A3C, 0x5FB04B, 0x3FA89A, 0x9FD8CB, 0x3B6FD1, 0xF29BB0, 0x8A5A3C,
    0x3A0B4D, 0xFFFFFF, 0xE8225F, 0x7A3CFF, 0xFFD23F, 0x00B8A9, 0x2D7BFF, 0xFF8A00, 0xA6516B, 0xFFEDE0,
};
static void set_color(int i, uint32_t rgb);
static uint32_t pal_rgb_of(int i);
#define NSKY 7
static const uint32_t SKY[NSKY] = { 0xFF2E7E, 0xFF4A62, 0xFF6552, 0xFF8140, 0xFF9E34, 0xFFB835, 0xFFD23F };
static const uint32_t CUBES[3] = { 0xE8225F, 0x2D7BFF, 0x00B8A9 };   // top, left, right block

uint32_t gfx_palette_rgb(int index)
{
    if (index >= OS_SKY) return index < 256 ? pal_rgb_of(index) : 0;
    return index >= 0 && index < (int)(sizeof PALETTE / sizeof PALETTE[0]) ? PALETTE[index] : 0;
}

void gfx_init_palette(void)
{
    for (int i = 0; i < (int)(sizeof PALETTE / sizeof PALETTE[0]); i++) set_color(i, PALETTE[i]);
    for (int i = 0; i < NSKY; i++) set_color(OS_SKY + i, SKY[i]);
    for (int b = 0; b < 3; b++) {   // faces: lit top (30% white), the colour, shaded right (28% plum)
        uint32_t c = CUBES[b], top = 0, side = 0;
        for (int sh = 0; sh <= 16; sh += 8) {
            uint32_t v = (c >> sh) & 0xFF, p = (0x3A0B4Du >> sh) & 0xFF;
            top |= (v * 7 / 10 + 255 * 3 / 10) << sh;
            side |= (v * 72 / 100 + p * 28 / 100) << sh;
        }
        set_color(OS_CUBE + b * 3, top);
        set_color(OS_CUBE + b * 3 + 1, c);
        set_color(OS_CUBE + b * 3 + 2, side);
    }
    for (int i = 0; i < 16; i++) {   // dimmed copies: 30% of the colour over the OS plum
        uint32_t c = PALETTE[i], n = 0x3A0B4D, out = 0;
        for (int sh = 0; sh <= 16; sh += 8) {
            uint32_t a = (c >> sh) & 0xFF, b = (n >> sh) & 0xFF;
            out |= ((a * 3 + b * 7) / 10) << sh;
        }
        set_color(OS_DIM + i, out);
    }
}

// ---------- palette mirror and blended slots (for anti-aliased text) ----------
static uint32_t pal_rgb[256];
static uint32_t pal_rgb_of(int i) { return pal_rgb[i]; }
#define BLEND_FIRST 72
static int blend_next = BLEND_FIRST;

static void set_color(int i, uint32_t rgb) { pal_rgb[i] = rgb; plat_set_color(i, rgb); }

static int blend_slot(uint32_t rgb)
{
    for (int i = BLEND_FIRST; i < blend_next; i++) if (pal_rgb[i] == rgb) return i;
    if (blend_next >= 256) return -1;
    set_color(blend_next, rgb);
    return blend_next++;
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

void gfx_circle(int cx, int cy, int r, int c, int filled)
{
    if (r < 0) return;
    int x = r, y = 0, err = 1 - r;   // midpoint circle
    while (x >= y) {
        if (filled) {
            gfx_fill_rect(cx - x, cy + y, 2 * x + 1, 1, c);
            gfx_fill_rect(cx - x, cy - y, 2 * x + 1, 1, c);
            gfx_fill_rect(cx - y, cy + x, 2 * y + 1, 1, c);
            gfx_fill_rect(cx - y, cy - x, 2 * y + 1, 1, c);
        } else {
            gfx_pixel(cx + x, cy + y, c); gfx_pixel(cx - x, cy + y, c);
            gfx_pixel(cx + x, cy - y, c); gfx_pixel(cx - x, cy - y, c);
            gfx_pixel(cx + y, cy + x, c); gfx_pixel(cx - y, cy + x, c);
            gfx_pixel(cx + y, cy - x, c); gfx_pixel(cx - y, cy - x, c);
        }
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
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

// ---------- anti-aliased text ----------
int gfx_aa_cap(int font) { return FONT_AA[font].cap; }

static const aa_glyph_t *aa_glyph(const aa_font_t *f, char ch)
{
    unsigned c = (unsigned char)ch;
    return &f->glyphs[c >= 32 && c < 127 ? c - 32 : '?' - 32];
}

int gfx_aa_width(const char *s, int font)
{
    const aa_font_t *f = &FONT_AA[font];
    int w = 0;
    for (; *s; s++) w += aa_glyph(f, *s)->adv;
    return w;
}

int gfx_aa_text(const char *s, int x, int y, int c, int font)
{
    const aa_font_t *f = &FONT_AA[font];
    int x0 = x;
    uint32_t fg = pal_rgb[c];
    for (; *s; s++) {
        const aa_glyph_t *g = aa_glyph(f, *s);
        const uint8_t *row = f->bits + g->off;
        int stride = (g->w + 3) / 4;
        for (int r = 0; r < g->h; r++, row += stride) {
            int py = y + g->yoff + r;
            if ((unsigned)py >= SCREEN_H) continue;
            for (int k = 0; k < g->w; k++) {
                int a = (row[k >> 2] >> (6 - 2 * (k & 3))) & 3;
                int px = x + g->xoff + k;
                if (!a || (unsigned)px >= SCREEN_W) continue;
                uint8_t *p = fb() + py * SCREEN_W + px;
                if (a == 3) { *p = (uint8_t)c; continue; }
                uint32_t bg = pal_rgb[*p], out = 0;
                for (int sh = 0; sh <= 16; sh += 8) {
                    int bv = (bg >> sh) & 0xFF, fv = (fg >> sh) & 0xFF;
                    out |= (uint32_t)(bv + (fv - bv) * a / 3) << sh;
                }
                int slot = out == bg ? *p : out == fg ? c : blend_slot(out);
                *p = (uint8_t)(slot >= 0 ? slot : a >= 2 ? c : *p);
            }
        }
        x += g->adv;
    }
    return x - x0;
}
