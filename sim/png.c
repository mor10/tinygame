// Minimal PNG writer: RGB, 8 bits, zlib "stored" blocks (no compression). Enough for screenshots.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"

static uint32_t crc_table[256];

static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    c ^= 0xFFFFFFFFu;
    while (n--) c = crc_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t h[8];
    put32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    uint32_t c = crc(crc(0, (const uint8_t *)type, 4), data, n);
    put32(h, c);
    fwrite(h, 1, 4, f);
}

int png_write(const char *path, const uint8_t *fb, const uint32_t *palette, int w, int h, int scale)
{
    int W = w * scale, H = h * scale;
    size_t row = 1 + (size_t)W * 3, raw_n = row * H;
    uint8_t *raw = malloc(raw_n);
    if (!raw) return -1;
    for (int y = 0; y < H; y++) {
        uint8_t *o = raw + y * row;
        *o++ = 0;   // filter: none
        for (int x = 0; x < W; x++) {
            uint32_t c = palette[fb[(y / scale) * w + x / scale]];
            *o++ = c >> 16; *o++ = c >> 8; *o++ = c;
        }
    }
    size_t blocks = (raw_n + 65534) / 65535;
    size_t z_n = 2 + raw_n + blocks * 5 + 4;
    uint8_t *z = malloc(z_n), *o = z;
    if (!z) { free(raw); return -1; }
    *o++ = 0x78; *o++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw_n; off += 65535) {
        size_t n = raw_n - off < 65535 ? raw_n - off : 65535;
        *o++ = off + n == raw_n;
        *o++ = n & 0xFF; *o++ = n >> 8; *o++ = ~n & 0xFF; *o++ = (~n >> 8) & 0xFF;
        memcpy(o, raw + off, n);
        o += n;
    }
    put32(o, (b << 16) | a);
    o += 4;

    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); free(z); return -1; }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    uint8_t ihdr[13];
    put32(ihdr, W); put32(ihdr + 4, H);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)(o - z));
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw); free(z);
    return 0;
}
