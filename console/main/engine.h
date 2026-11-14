#pragma once
// Engine v1 assets: sprites and tile maps decoded from a cart's text art, plus their Lua API.
// See docs/engine-v1.md for the format.
#include "lua.h"

// Adds gfx.sprite and gfx.map to the table at gfx_index, defines map_get/map_set/map_hit as
// globals, and pushes the `map` library table (the caller makes it read-only and names it).
void engine_open(lua_State *L, int gfx_index);
int  engine_load_assets(lua_State *L);   // lua_CFunction: decodes the cart's `sprites` and `maps`
void engine_reset(void);                 // frees everything decoded for the previous cart
unsigned engine_asset_bytes(void);
