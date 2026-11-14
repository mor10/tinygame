// platform.h for the simulator: a plain framebuffer, malloc, and the host clock.
#include <stdlib.h>
#include <time.h>
#include "platform.h"
#include "sim.h"

uint8_t  sim_fb[SCREEN_W * SCREEN_H];
uint32_t sim_palette[256];

int64_t plat_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

uint8_t *plat_framebuffer(void) { return sim_fb; }
void     plat_set_color(int index, uint32_t rgb) { if (index >= 0 && index < 256) sim_palette[index] = rgb; }

void *plat_lua_realloc(void *ptr, size_t nsize, int prefer_fast) { (void)prefer_fast; return realloc(ptr, nsize); }
void  plat_lua_free(void *ptr) { free(ptr); }
int   plat_is_fast(const void *ptr) { (void)ptr; return 0; }

void *plat_asset_alloc(size_t n) { return malloc(n); }
void  plat_asset_free(void *ptr) { free(ptr); }

int  sim_backlight = 100;
void plat_backlight(int percent) { sim_backlight = percent; }
int  plat_settings_load(void *buf, size_t n) { (void)buf; (void)n; return -1; }
void plat_settings_save(const void *buf, size_t n) { (void)buf; (void)n; }
