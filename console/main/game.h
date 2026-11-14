#pragma once
// Lua host: runs one cart at a time in a sandboxed Lua 5.4 state on core 1.
#include <stddef.h>

void game_start(void);
// Queue a cart to run next. The source is copied; `name` is used in logs only.
void game_load(const char *src, size_t len, const char *name);
void game_restart(void);   // rerun the current cart from its source
