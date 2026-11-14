#pragma once
// Portable Lua host: one sandboxed cart at a time, a 1 MB memory cap, a 250 ms watchdog per
// callback, error screens, and the cart API (gfx, knob, sys, map, helpers). The platform drives it:
// vm_start() once, vm_frame() every frame, vm_stop() at the end. Output is JSON lines on stdout.
#include <stdint.h>
#include <stddef.h>

enum { VM_LEFT = 0, VM_RIGHT = 1 };

// One frame of input. delta is in quadrature transitions (80 per turn); the others are events
// since the previous frame, except down (held now), pos and speed.
typedef struct {
    int32_t pos[2], delta[2];
    float   speed[2];
    uint8_t down[2], pressed[2], released[2], long_press[2];
} vm_input_t;

typedef struct {
    const char *prelude;    // prelude.lua source, run before the cart
    size_t      prelude_len;
    int         deterministic;   // sys.time() follows the frame count, math.random is seeded
    uint32_t    seed;
} vm_opts_t;

typedef enum { VM_ERR_NONE, VM_ERR_BUG, VM_ERR_STUCK, VM_ERR_MEMORY, VM_ERR_STOPPED } vm_err_t;

typedef struct {
    uint32_t update_us, draw_us, gc_us;   // last frame
    size_t   lua_bytes, lua_fast_bytes, lua_peak_bytes;
    unsigned asset_bytes;
} vm_stats_t;

int      vm_start(const char *src, size_t len, const char *name, const vm_opts_t *opts);  // 0 = running
int      vm_frame(const vm_input_t *in);   // runs update, draw and a GC step; 0 = still running
void     vm_stop(void);                    // closes the Lua state and frees assets
void     vm_stopped_by_user(void);         // records "game stopped" (force-exit chord)
vm_err_t vm_error(void);
void     vm_draw_error_screen(void);
int      vm_fps(void);                     // the cart's declared fps, 30 or 60
void     vm_set_measured_fps(float fps);   // what sys.fps() reports
uint32_t vm_frame_count(void);
void     vm_get_stats(vm_stats_t *out);
void     vm_reset_peak(void);
uint32_t vm_frame_hash(void);              // FNV-1a of the current back buffer
