#pragma once
// Per-cart save store behind save.set/get/clear. See save.c for the format and limits.
#include <stdint.h>
#include "lua.h"

// Loads the save for `id` (empty id: saving is off) and pushes the `save` library table.
void save_open(lua_State *L, const char *id, int64_t now_us);
// Writes the save if it changed: at most once a second, or now when force is set.
void save_flush(lua_State *L, int64_t now_us, int force);
// Developer/test hook: write saves at most every `us` microseconds (default 1 s; 0 = every frame).
void save_set_flush_interval(int64_t us);
