// Engine v1 assets: text-art sprites and tile maps, decoded once when a cart loads.
#include <string.h>
#include <math.h>
#include "engine.h"
#include "gfx.h"
#include "platform.h"
#include "lauxlib.h"

#define MAX_SPRITES 256
#define MAX_MAPS    16
#define MAX_LAYERS  4
#define SPRITE_MAX  64
#define MAP_MAX     256
#define TILE_MAX    64
#define ASSET_CAP   (512 * 1024)
#define NO_SPRITE   0xFFFF

typedef struct { int w, h; uint8_t *px; } sprite_t;
typedef struct {
    int tw, th, cols, rows, nlayers;
    uint16_t legend[128];        // tile character -> sprite index
    char *art[MAX_LAYERS];       // original layers, rows * cols characters each
    char name[32];
} mapdef_t;

static sprite_t spr[MAX_SPRITES];
static int nspr;
static mapdef_t maps[MAX_MAPS];
static int nmaps;
static int active = -1;
static char *live[MAX_LAYERS];   // the active map's layers as the game changes them
static unsigned asset_bytes;
static int loaded;               // assets decode once: on first use, or right after the cart file runs

static const char *REG_SPR = "tt_sprites", *REG_MAP = "tt_maps";

// ---------- memory ----------
static void *amalloc(lua_State *L, size_t n)
{
    if (asset_bytes + n > ASSET_CAP) luaL_error(L, "your sprites and maps need more than %d KB", ASSET_CAP / 1024);
    void *p = plat_asset_alloc(n);
    if (!p) luaL_error(L, "no memory left for sprites and maps");
    asset_bytes += n;
    return p;
}

void engine_reset(void)
{
    for (int i = 0; i < nspr; i++) plat_asset_free(spr[i].px);
    for (int i = 0; i < nmaps; i++)
        for (int l = 0; l < MAX_LAYERS; l++) plat_asset_free(maps[i].art[l]);
    for (int l = 0; l < MAX_LAYERS; l++) { plat_asset_free(live[l]); live[l] = NULL; }
    memset(spr, 0, sizeof spr);
    memset(maps, 0, sizeof maps);
    nspr = nmaps = 0;
    active = -1;
    asset_bytes = 0;
    loaded = 0;
}

unsigned engine_asset_bytes(void) { return asset_bytes; }

// ---------- text art ----------
// Splits art into trimmed, non-empty rows. Returns the row count; every row must have the same length.
static int split_rows(lua_State *L, const char *art, const char **row, int max_rows, int *width,
                      const char *what, const char *name)
{
    int n = 0;
    *width = -1;
    while (*art) {
        const char *e = strchr(art, '\n');
        if (!e) e = art + strlen(art);
        const char *a = art, *b = e;
        while (a < b && (*a == ' ' || *a == '\t' || *a == '\r')) a++;
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) b--;
        if (b > a) {
            int len = (int)(b - a);
            if (n == max_rows) luaL_error(L, "%s '%s' has more than %d rows", what, name, max_rows);
            if (*width < 0) *width = len;
            else if (len != *width)
                luaL_error(L, "%s '%s' row %d is %d wide, but row 1 is %d wide", what, name, n + 1, len, *width);
            row[n++] = a;
        }
        art = *e ? e + 1 : e;
    }
    if (n == 0) luaL_error(L, "%s '%s' is empty", what, name);
    return n;
}

static int pixel_value(char c)
{
    if (c == '.') return GFX_CLEAR;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'v') return c - 'a' + 10;   // a-v are colours 10-31
    if (c >= 'A' && c <= 'V') return c - 'A' + 10;
    return -1;
}

static void decode_sprite(lua_State *L, const char *name, const char *art)
{
    if (nspr == MAX_SPRITES) luaL_error(L, "a game can have at most %d sprites", MAX_SPRITES);
    const char *row[SPRITE_MAX];
    int w, h = split_rows(L, art, row, SPRITE_MAX, &w, "sprite", name);
    if (w > SPRITE_MAX) luaL_error(L, "sprite '%s' is %d wide; the most is %d", name, w, SPRITE_MAX);
    uint8_t *px = amalloc(L, (size_t)w * h);
    spr[nspr] = (sprite_t){ w, h, px };   // registered first so engine_reset frees it on an error below
    nspr++;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int v = pixel_value(row[y][x]);
            if (v < 0) luaL_error(L, "sprite '%s' row %d has '%c'; use . for see-through or 0-9 and a-v for colours",
                                  name, y + 1, row[y][x]);
            px[y * w + x] = (uint8_t)v;
        }
}

static int get_int_field(lua_State *L, int t, const char *k, int dflt, const char *map)
{
    lua_getfield(L, t, k);
    if (!lua_isnil(L, -1) && !lua_isinteger(L, -1)) luaL_error(L, "map '%s' %s must be a whole number", map, k);
    int v = lua_isnil(L, -1) ? dflt : (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return v;
}

static void decode_map(lua_State *L, const char *name, int t)
{
    if (nmaps == MAX_MAPS) luaL_error(L, "a game can have at most %d maps", MAX_MAPS);
    mapdef_t *m = &maps[nmaps++];
    strlcpy(m->name, name, sizeof m->name);
    int tile = get_int_field(L, t, "tile", 8, name);
    m->tw = get_int_field(L, t, "tile_w", tile, name);
    m->th = get_int_field(L, t, "tile_h", tile, name);
    if (m->tw < 1 || m->tw > TILE_MAX || m->th < 1 || m->th > TILE_MAX)
        luaL_error(L, "map '%s' tiles must be 1-%d pixels each way", name, TILE_MAX);

    for (int i = 0; i < 128; i++) m->legend[i] = NO_SPRITE;
    lua_getfield(L, t, "legend");
    if (!lua_istable(L, -1)) luaL_error(L, "map '%s' needs a legend = { ... } table", name);
    lua_rawgetp(L, LUA_REGISTRYINDEX, REG_SPR);   // name -> index
    lua_pushnil(L);
    while (lua_next(L, -3)) {
        size_t kl;
        const char *k = lua_type(L, -2) == LUA_TSTRING ? lua_tolstring(L, -2, &kl) : NULL;
        if (!k || kl != 1 || (unsigned char)k[0] >= 128 || k[0] == '.' || k[0] == ' ')
            luaL_error(L, "map '%s' legend keys must be single characters (not '.')", name);
        lua_pushvalue(L, -1);
        lua_rawget(L, -4);
        if (!lua_isinteger(L, -1))
            luaL_error(L, "map '%s' legend '%c' names sprite '%s', which doesn't exist", name, k[0],
                       lua_isstring(L, -2) ? lua_tostring(L, -2) : "?");
        m->legend[(unsigned char)k[0]] = (uint16_t)lua_tointeger(L, -1);
        lua_pop(L, 2);
    }
    lua_pop(L, 2);

    lua_getfield(L, t, "layers");
    if (!lua_istable(L, -1)) luaL_error(L, "map '%s' needs layers = { [[ ... ]] }", name);
    int n = (int)lua_rawlen(L, -1);
    if (n < 1 || n > MAX_LAYERS) luaL_error(L, "map '%s' must have 1-%d layers", name, MAX_LAYERS);
    m->nlayers = n;
    for (int l = 0; l < n; l++) {
        lua_rawgeti(L, -1, l + 1);
        const char *art = lua_tostring(L, -1);
        if (!art) luaL_error(L, "map '%s' layer %d must be text art", name, l + 1);
        static const char *row[MAP_MAX];
        int cols, rows = split_rows(L, art, row, MAP_MAX, &cols, "map", name);
        if (cols > MAP_MAX) luaL_error(L, "map '%s' is %d tiles wide; the most is %d", name, cols, MAP_MAX);
        if (l == 0) { m->cols = cols; m->rows = rows; }
        else if (cols != m->cols || rows != m->rows)
            luaL_error(L, "map '%s' layer %d is %dx%d, but layer 1 is %dx%d", name, l + 1, cols, rows, m->cols, m->rows);
        m->art[l] = amalloc(L, (size_t)cols * rows);
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < cols; x++) {
                char c = row[y][x];
                if (c != '.' && ((unsigned char)c >= 128 || m->legend[(unsigned char)c] == NO_SPRITE))
                    luaL_error(L, "map '%s' layer %d row %d has '%c', which isn't in its legend", name, l + 1, y + 1, c);
                m->art[l][y * cols + x] = c;
            }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

int engine_load_assets(lua_State *L)
{
    if (loaded) return 0;
    loaded = 1;
    lua_getglobal(L, "sprites");
    if (!lua_isnil(L, -1)) {
        if (!lua_istable(L, -1)) return luaL_error(L, "sprites must be a table");
        lua_rawgetp(L, LUA_REGISTRYINDEX, REG_SPR);
        lua_pushnil(L);
        while (lua_next(L, -3)) {
            if (lua_type(L, -2) != LUA_TSTRING || !lua_isstring(L, -1))
                return luaL_error(L, "each sprite needs a name and text art: name = [[ ... ]]");
            const char *name = lua_tostring(L, -2);
            decode_sprite(L, name, lua_tostring(L, -1));
            lua_pop(L, 1);
            lua_pushvalue(L, -1);
            lua_pushinteger(L, nspr - 1);
            lua_rawset(L, -4);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getglobal(L, "maps");
    if (!lua_isnil(L, -1)) {
        if (!lua_istable(L, -1)) return luaL_error(L, "maps must be a table");
        lua_rawgetp(L, LUA_REGISTRYINDEX, REG_MAP);
        lua_pushnil(L);
        while (lua_next(L, -3)) {
            if (lua_type(L, -2) != LUA_TSTRING || !lua_istable(L, -1))
                return luaL_error(L, "each map needs a name and a table: level1 = { ... }");
            decode_map(L, lua_tostring(L, -2), lua_gettop(L));
            lua_pop(L, 1);
            lua_pushvalue(L, -1);
            lua_pushinteger(L, nmaps - 1);
            lua_rawset(L, -4);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return 0;
}

// ---------- Lua API ----------
static int arg_int(lua_State *L, int i) { return (int)floorf((float)luaL_checknumber(L, i)); }
static int opt_int(lua_State *L, int i, int d) { return lua_isnoneornil(L, i) ? d : arg_int(L, i); }
static int floordiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static int l_sprite(lua_State *L)
{
    // Read every argument before pushing anything: a pushed value would sit in the slot of a
    // missing optional argument (lua_toboolean(L, 4) would then see the sprite index as flip_x).
    luaL_checkstring(L, 1);
    int x = arg_int(L, 2), y = arg_int(L, 3), fx = lua_toboolean(L, 4), fy = lua_toboolean(L, 5);
    int scale = opt_int(L, 6, 1);
    if (scale != 1 && scale != 2) return luaL_error(L, "sprite scale must be 1 or 2");
    engine_load_assets(L);
    lua_settop(L, 1);
    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(1));
    if (!lua_isinteger(L, -1)) return luaL_error(L, "there's no sprite named '%s'", lua_tostring(L, 1));
    sprite_t *s = &spr[lua_tointeger(L, -1)];
    gfx_blit(s->px, s->w, s->h, x, y, fx, fy, scale);
    return 0;
}

static mapdef_t *need_map(lua_State *L)
{
    if (active < 0) luaL_error(L, "call map.load(\"name\") before using a map");
    return &maps[active];
}

static int arg_layer(lua_State *L, int i, mapdef_t *m)
{
    int l = (int)luaL_checkinteger(L, i);
    if (l < 1 || l > m->nlayers) luaL_error(L, "map '%s' has layers 1-%d, not %d", m->name, m->nlayers, l);
    return l - 1;
}

static int l_map_load(lua_State *L)
{
    luaL_checkstring(L, 1);
    engine_load_assets(L);
    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(1));
    if (!lua_isinteger(L, -1)) return luaL_error(L, "there's no map named '%s'", lua_tostring(L, 1));
    int idx = (int)lua_tointeger(L, -1);
    mapdef_t *m = &maps[idx];
    for (int l = 0; l < MAX_LAYERS; l++) { plat_asset_free(live[l]); live[l] = NULL; }
    for (int l = 0; l < m->nlayers; l++) {
        size_t n = (size_t)m->cols * m->rows;
        live[l] = plat_asset_alloc(n);
        if (!live[l]) return luaL_error(L, "no memory left to load map '%s'", m->name);
        memcpy(live[l], m->art[l], n);
    }
    active = idx;
    return 0;
}

static int l_map_draw(lua_State *L)
{
    mapdef_t *m = need_map(L);
    int l = arg_layer(L, 1, m);
    int ox = opt_int(L, 2, 0), oy = opt_int(L, 3, 0);
    int tx0 = floordiv(-ox, m->tw), ty0 = floordiv(-oy, m->th);
    int tx1 = floordiv(SCREEN_W - 1 - ox, m->tw), ty1 = floordiv(SCREEN_H - 1 - oy, m->th);
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 >= m->cols) tx1 = m->cols - 1;
    if (ty1 >= m->rows) ty1 = m->rows - 1;
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            char c = live[l][ty * m->cols + tx];
            if (c == '.') continue;
            sprite_t *s = &spr[m->legend[(unsigned char)c]];
            gfx_blit(s->px, s->w, s->h, ox + tx * m->tw, oy + ty * m->th, 0, 0, 1);
        }
    return 0;
}

static int l_map_get(lua_State *L)
{
    mapdef_t *m = need_map(L);
    int l = arg_layer(L, 1, m);
    int tx = arg_int(L, 2), ty = arg_int(L, 3);
    if (tx < 0 || ty < 0 || tx >= m->cols || ty >= m->rows) { lua_pushnil(L); return 1; }
    lua_pushlstring(L, &live[l][ty * m->cols + tx], 1);
    return 1;
}

static int l_map_set(lua_State *L)
{
    mapdef_t *m = need_map(L);
    int l = arg_layer(L, 1, m);
    int tx = arg_int(L, 2), ty = arg_int(L, 3);
    size_t n;
    const char *c = luaL_checklstring(L, 4, &n);
    if (n != 1) return luaL_error(L, "map_set needs a single tile character, like \".\" or \"r\"");
    if (c[0] != '.' && ((unsigned char)c[0] >= 128 || m->legend[(unsigned char)c[0]] == NO_SPRITE))
        return luaL_error(L, "'%c' isn't in the legend of map '%s'", c[0], m->name);
    if (tx < 0 || ty < 0 || tx >= m->cols || ty >= m->rows)
        return luaL_error(L, "tile %d,%d is outside map '%s' (%d by %d tiles)", tx, ty, m->name, m->cols, m->rows);
    live[l][ty * m->cols + tx] = c[0];
    return 0;
}

static int l_map_hit(lua_State *L)
{
    mapdef_t *m = need_map(L);
    int l = arg_layer(L, 1, m);
    int x = arg_int(L, 2), y = arg_int(L, 3), w = arg_int(L, 4), h = arg_int(L, 5);
    if (w < 1 || h < 1) return 0;
    int tx0 = floordiv(x, m->tw), tx1 = floordiv(x + w - 1, m->tw);
    int ty0 = floordiv(y, m->th), ty1 = floordiv(y + h - 1, m->th);
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 >= m->cols) tx1 = m->cols - 1;
    if (ty1 >= m->rows) ty1 = m->rows - 1;
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            char c = live[l][ty * m->cols + tx];
            if (c == '.') continue;
            lua_pushlstring(L, &c, 1);
            lua_pushinteger(L, tx);
            lua_pushinteger(L, ty);
            return 3;
        }
    lua_pushnil(L);
    return 1;
}

void engine_open(lua_State *L, int gfx_index)
{
    gfx_index = lua_absindex(L, gfx_index);
    lua_newtable(L); lua_rawsetp(L, LUA_REGISTRYINDEX, REG_SPR);
    lua_newtable(L); lua_rawsetp(L, LUA_REGISTRYINDEX, REG_MAP);

    lua_rawgetp(L, LUA_REGISTRYINDEX, REG_SPR);
    lua_pushcclosure(L, l_sprite, 1);
    lua_setfield(L, gfx_index, "sprite");
    lua_pushcfunction(L, l_map_draw);
    lua_setfield(L, gfx_index, "map");

    lua_pushcfunction(L, l_map_get); lua_setglobal(L, "map_get");
    lua_pushcfunction(L, l_map_set); lua_setglobal(L, "map_set");
    lua_pushcfunction(L, l_map_hit); lua_setglobal(L, "map_hit");

    lua_newtable(L);
    lua_rawgetp(L, LUA_REGISTRYINDEX, REG_MAP);
    lua_pushcclosure(L, l_map_load, 1);
    lua_setfield(L, -2, "load");
}
