// WebAssembly entry points for the browser simulator. The page calls these through cwrap;
// JSON lines from the engine arrive through Module.print.
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include "vm.h"
#include "gfx.h"
#include "sim.h"

extern const char prelude_lua[];
extern const unsigned prelude_lua_len;

static uint8_t rgba[SCREEN_W * SCREEN_H * 4];
static int running;

EMSCRIPTEN_KEEPALIVE int tg_load(const char *src, int len, int deterministic, unsigned seed)
{
    static int palette_ready;
    if (!palette_ready) { gfx_init_palette(); palette_ready = 1; }
    if (running) vm_stop();
    vm_opts_t opts = { .prelude = prelude_lua, .prelude_len = prelude_lua_len,
                       .deterministic = deterministic, .seed = seed };
    running = 1;
    if (vm_start(src, (size_t)len, "browser", &opts) != 0) {
        vm_draw_error_screen();
        vm_stop();
        running = 0;
        return -1;
    }
    return 0;
}

// Turn deltas are in quadrature transitions (4 per notch). Bits: 1 = pressed, 2 = held, 4 = released.
EMSCRIPTEN_KEEPALIVE int tg_frame(int delta_l, int delta_r, int bits_l, int bits_r)
{
    if (!running) return -1;
    static int32_t pos[2];
    vm_input_t in;
    memset(&in, 0, sizeof in);
    int d[2] = { delta_l, delta_r }, b[2] = { bits_l, bits_r };
    for (int i = 0; i < 2; i++) {
        pos[i] += d[i];
        in.pos[i] = pos[i];
        in.delta[i] = d[i];
        in.pressed[i] = b[i] & 1;
        in.down[i] = (b[i] >> 1) & 1;
        in.released[i] = (b[i] >> 2) & 1;
    }
    if (vm_frame(&in) != 0) {
        vm_draw_error_screen();
        vm_stop();
        running = 0;
        return -1;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE unsigned tg_hash(void) { return vm_frame_hash(); }
EMSCRIPTEN_KEEPALIVE int tg_frame_count(void) { return (int)vm_frame_count(); }
EMSCRIPTEN_KEEPALIVE int tg_fps(void) { return vm_fps(); }

EMSCRIPTEN_KEEPALIVE uint8_t *tg_rgba(void)
{
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint32_t c = sim_palette[sim_fb[i]];
        rgba[i * 4] = c >> 16; rgba[i * 4 + 1] = c >> 8; rgba[i * 4 + 2] = c; rgba[i * 4 + 3] = 255;
    }
    return rgba;
}
