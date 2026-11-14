#pragma once
// Lua host: runs one cart at a time in a sandboxed Lua 5.4 state on core 1.
#include <stddef.h>
#include <stdint.h>

void game_start(void);
// Queue a cart to run next. The source is copied; `name` is used in logs only.
void game_load(const char *src, size_t len, const char *name);
void game_restart(void);   // rerun the current cart from its source
// Rerun the current cart deterministically (fixed seed, frame-based time, no input) and print
// {"t":"hash"} lines for frame 1, every 30th frame and the last one. Compare with the simulator.
void game_hash(int frames, uint32_t seed);
