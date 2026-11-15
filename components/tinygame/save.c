// save.set / save.get / save.clear: a small per-cart key/value store.
// Values are numbers, strings and booleans; the whole save is at most 4 KB once written out.
// Writes are batched: at most one per second while the game runs, plus one when it stops.
//
// File format (text, so a save can be read by eye):
//   TGS1
//   <key>\t<n|s|b>\t<value>      one line per key; strings escape \\ \t \n
//   #<crc32 of everything above, 8 hex digits>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "save.h"
#include "platform.h"
#include "lauxlib.h"

#define SAVE_MAX   4096
#define KEY_MAX    32
static volatile int64_t flush_us = 1000000;   // tests can shorten it with save_set_flush_interval

static const char *REG_SAVE = "tt_save";
static char save_id[32];
static int dirty;
static int64_t last_flush;

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; }
    return ~c;
}

// Serializes the save table into buf. Returns the length, or -1 if it doesn't fit in max.
static int serialize(lua_State *L, char *buf, int max)
{
    int n = snprintf(buf, max, "TGS1\n");
    lua_rawgetp(L, LUA_REGISTRYINDEX, REG_SAVE);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        const char *k = lua_tostring(L, -2);
        char type;
        char num[32];
        const char *v;
        size_t vl;
        if (lua_type(L, -1) == LUA_TBOOLEAN) { type = 'b'; v = lua_toboolean(L, -1) ? "1" : "0"; vl = 1; }
        else if (lua_type(L, -1) == LUA_TNUMBER) {
            type = 'n';
            if (lua_isinteger(L, -1)) snprintf(num, sizeof num, LUA_INTEGER_FMT, lua_tointeger(L, -1));
            else snprintf(num, sizeof num, "%.9g", (double)lua_tonumber(L, -1));
            v = num; vl = strlen(num);
        } else { type = 's'; v = lua_tolstring(L, -1, &vl); }
        if (n + (int)strlen(k) + 4 >= max) { lua_pop(L, 3); return -1; }
        n += snprintf(buf + n, max - n, "%s\t%c\t", k, type);
        for (size_t i = 0; i < vl; i++) {
            const char *esc = v[i] == '\\' ? "\\\\" : v[i] == '\t' ? "\\t" : v[i] == '\n' ? "\\n" : NULL;
            int need = esc ? 2 : 1;
            if (n + need + 12 >= max) { lua_pop(L, 3); return -1; }
            if (esc) { buf[n++] = esc[0]; buf[n++] = esc[1]; } else buf[n++] = v[i];
        }
        buf[n++] = '\n';
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (n + 11 >= max) return -1;
    n += snprintf(buf + n, max - n, "#%08x\n", (unsigned)crc32((const uint8_t *)buf, n));
    return n;
}

static void unescape(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '\\' && s[1]) { s++; *o++ = *s == 't' ? '\t' : *s == 'n' ? '\n' : *s; }
        else *o++ = *s;
    }
    *o = 0;
}

// Parses buf into the save table. Returns 0, or -1 if the file is damaged (the table stays empty).
static int parse(lua_State *L, char *buf, int n)
{
    buf[n] = 0;
    char *hash = strrchr(buf, '#');
    if (strncmp(buf, "TGS1\n", 5) != 0 || !hash || hash == buf || hash[-1] != '\n') return -1;
    if ((uint32_t)strtoul(hash + 1, NULL, 16) != crc32((const uint8_t *)buf, hash - buf)) return -1;
    *hash = 0;
    lua_rawgetp(L, LUA_REGISTRYINDEX, REG_SAVE);
    for (char *line = buf + 5, *next; *line; line = next) {
        next = strchr(line, '\n');
        if (!next) break;
        *next++ = 0;
        char *t1 = strchr(line, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (!t1 || !t2) continue;
        *t1 = 0; *t2 = 0;
        char type = t1[1], *v = t2 + 1;
        unescape(v);
        if (type == 'b') lua_pushboolean(L, v[0] == '1');
        else if (type == 'n') {
            if (!lua_stringtonumber(L, v)) lua_pushinteger(L, 0);
        } else lua_pushstring(L, v);
        lua_setfield(L, -2, line);
    }
    lua_pop(L, 1);
    return 0;
}

static int l_set(lua_State *L)
{
    lua_settop(L, 2);   // a missing value is nil (removes the key)
    size_t kl;
    const char *k = luaL_checklstring(L, 1, &kl);
    if (kl == 0 || kl > KEY_MAX) return luaL_error(L, "save keys must be 1-%d characters", KEY_MAX);
    for (size_t i = 0; i < kl; i++)
        if (k[i] == '\t' || k[i] == '\n' || k[i] == '#') return luaL_error(L, "save keys can't contain tabs, newlines or #");
    int t = lua_type(L, 2);
    if (t != LUA_TNUMBER && t != LUA_TSTRING && t != LUA_TBOOLEAN && t != LUA_TNIL)
        return luaL_error(L, "save.set can store numbers, strings and true/false, not a %s", lua_typename(L, t));
    lua_rawgetp(L, LUA_REGISTRYINDEX, REG_SAVE);
    lua_pushvalue(L, 1);
    lua_getfield(L, -2, k);                  // old value, to put back if the new one doesn't fit
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_rawset(L, -5);
    char *buf = plat_asset_alloc(SAVE_MAX);
    int n = buf ? serialize(L, buf, SAVE_MAX) : -1;
    plat_asset_free(buf);
    if (n < 0) {
        lua_rawset(L, -3);                   // restore: key and old value are on top
        return luaL_error(L, "Your game's save is full (%d KB). Store less, or use save.clear()", SAVE_MAX / 1024);
    }
    lua_pop(L, 3);
    dirty = 1;
    return 0;
}

static int l_get(lua_State *L)
{
    luaL_checkstring(L, 1);
    lua_settop(L, 2);   // a missing default is nil at index 2, not whatever gets pushed next
    lua_rawgetp(L, LUA_REGISTRYINDEX, REG_SAVE);
    lua_pushvalue(L, 1);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushvalue(L, 2); }
    return 1;
}

static int l_clear(lua_State *L)
{
    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, REG_SAVE);
    dirty = 1;
    return 0;
}

void save_open(lua_State *L, const char *id, int64_t now_us)
{
    snprintf(save_id, sizeof save_id, "%s", id ? id : "");
    dirty = 0;
    last_flush = now_us;
    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, REG_SAVE);
    if (save_id[0]) {
        char *buf = plat_asset_alloc(SAVE_MAX + 1);
        int n = buf ? plat_save_read(save_id, buf, SAVE_MAX) : -1;
        if (n > 0 && parse(L, buf, n) != 0)
            printf("{\"t\":\"error\",\"kind\":\"save\",\"msg\":\"save for %s was damaged and has been reset\"}\n", save_id);
        plat_asset_free(buf);
    }
    static const luaL_Reg fns[] = { { "set", l_set }, { "get", l_get }, { "clear", l_clear }, { NULL, NULL } };
    luaL_newlib(L, fns);   // the caller makes it read-only and names it
}

void save_flush(lua_State *L, int64_t now_us, int force)
{
    if (!dirty || !save_id[0]) return;
    if (!force && now_us - last_flush < flush_us) return;
    char *buf = plat_asset_alloc(SAVE_MAX);
    int n = buf ? serialize(L, buf, SAVE_MAX) : -1;
    if (n > 0 && plat_save_write(save_id, buf, (size_t)n) == 0) dirty = 0;
    else printf("{\"t\":\"error\",\"kind\":\"save\",\"msg\":\"couldn't write the save for %s\"}\n", save_id);
    plat_asset_free(buf);
    last_flush = now_us;
}

void save_set_flush_interval(int64_t us) { flush_us = us < 0 ? 0 : us; }
