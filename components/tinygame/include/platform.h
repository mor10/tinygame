#pragma once
// The platform contract for the portable engine (vm, gfx, engine). Everything in
// components/tinygame is plain C11 plus Lua; each target supplies these functions:
// console/main/platform_esp.c on the board, sim/platform_host.c in the simulator.
#include <stdint.h>
#include <stddef.h>

#define SCREEN_W 320
#define SCREEN_H 172

int64_t  plat_time_us(void);                       // monotonic microseconds
uint8_t *plat_framebuffer(void);                   // the 8-bit back buffer for this frame
void     plat_set_color(int index, uint32_t rgb);  // palette entry, 0xRRGGBB

// Lua heap. realloc semantics with nsize > 0; prefer_fast asks for internal RAM where it exists.
void *plat_lua_realloc(void *ptr, size_t nsize, int prefer_fast);
void  plat_lua_free(void *ptr);
int   plat_is_fast(const void *ptr);               // 1 if ptr is in the fast (internal) heap

// Decoded sprites and maps.
void *plat_asset_alloc(size_t n);
void  plat_asset_free(void *ptr);

// Shell services.
void plat_backlight(int percent);
void plat_set_gamma(float r, float g, float b);   // per-channel panel correction exponents (1.0 = none); board only
void plat_get_gamma(float *r, float *g, float *b);
int  plat_settings_load(void *buf, size_t n);        // 0 when n bytes were read
void plat_settings_save(const void *buf, size_t n);

void  plat_storage_init(void);   // mount the volumes (board) or pick a directory (simulator)

// Stored carts (the carts volume). An id is a file name without ".lua": 1-31 of a-z 0-9 _ -.
int   plat_cart_list(char ids[][32], int max);                 // returns how many, sorted by id
char *plat_cart_load(const char *id, size_t *len);             // NUL-terminated; free with plat_asset_free
int   plat_cart_store(const char *id, const char *src, size_t len);   // 0 on success; replaces atomically
int   plat_cart_delete(const char *id);

// Saves (the saves volume): one blob per cart id, replaced atomically so a power cut leaves
// either the old save or the new one, never a mix.
int   plat_save_read(const char *id, void *buf, size_t max);   // bytes read, or -1 if there's none
int   plat_save_write(const char *id, const void *buf, size_t n);   // 0 on success
