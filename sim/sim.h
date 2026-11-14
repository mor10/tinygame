#pragma once
#include <stdint.h>
#include "platform.h"

extern uint8_t  sim_fb[SCREEN_W * SCREEN_H];
extern uint32_t sim_palette[256];

// Writes the framebuffer through the palette as an RGB PNG. Returns 0 on success.
int png_write(const char *path, const uint8_t *fb, const uint32_t *palette, int w, int h, int scale);
