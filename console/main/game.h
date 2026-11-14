#pragma once
// Board runner: the OS shell on core 1, plus requests from the serial console.
#include <stddef.h>
#include <stdint.h>

void game_start(int safe_mode);   // scans the built-in carts and starts the shell
// Queue an uploaded cart: it replaces the "Uploaded" entry and starts playing. The source is copied.
void game_load(const char *src, size_t len, const char *name);
void game_restart(void);   // relaunch the last game
// Relaunch the last game deterministically (fixed seed, frame-based time, no input) and print
// {"t":"hash"} lines for frame 1, every 30th frame and the last one. Compare with the simulator.
void game_hash(int frames, uint32_t seed);
