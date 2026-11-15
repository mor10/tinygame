// Portable Lua host: sandboxed Lua 5.4 state, memory cap, instruction-count watchdog, error
// screens, and the cart API (cart table, init/update/draw, gfx, knob, sys, map, helpers).
// Plain C11 plus Lua; the platform supplies platform.h and drives vm_start/vm_frame/vm_stop.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vm.h"
#include "gfx.h"
#include "font_aa.h"
#include "engine.h"
#include "save.h"
#include "platform.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define MEM_CAP          (1024 * 1024)   // per cart, all heaps
#define MEM_FAST_CAP     (64 * 1024)     // small blocks go to fast (internal) RAM up to this much
#define SMALL_BLOCK      256
#define WATCHDOG_US      250000          // one callback may run this long
#define HOOK_COUNT       1000            // instructions between watchdog checks

static lua_State *L_;
static char cur_name[32];
static int det;                          // deterministic mode
static int cart_fps_v = 30;

// ---------- allocator with a cap ----------
static size_t mem_used, mem_int, mem_peak;   // mem_int counts the fast heap

static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    if (!ptr) osize = 0;   // when ptr is NULL, osize carries a type tag, not a size
    if (nsize == 0) {
        if (ptr) {
            if (plat_is_fast(ptr)) mem_int -= osize;
            mem_used -= osize;
            plat_lua_free(ptr);
        }
        return NULL;
    }
    if (mem_used - osize + nsize > MEM_CAP) return NULL;   // Lua runs a full GC and retries once
    int fast_before = ptr && plat_is_fast(ptr);
    int want_fast = nsize <= SMALL_BLOCK && mem_int + nsize <= MEM_FAST_CAP;
    void *np = plat_lua_realloc(ptr, nsize, want_fast);
    if (!np) return NULL;
    if (fast_before) mem_int -= osize;
    mem_used -= osize;
    if (plat_is_fast(np)) mem_int += nsize;
    mem_used += nsize;
    if (mem_used > mem_peak) mem_peak = mem_used;
    return np;
}

// ---------- watchdog ----------
static int64_t cb_start;
static volatile int stuck;
static int stuck_line;

static void hook(lua_State *L, lua_Debug *ar)
{
    if (plat_time_us() - cb_start < WATCHDOG_US) return;
    if (!stuck) {
        stuck = 1;
        stuck_line = lua_getinfo(L, "l", ar) ? ar->currentline : -1;
    }
    lua_pushfstring(L, "Your game got stuck (maybe a loop that never ends?) at line %d", stuck_line);
    lua_error(L);
}

// pcall/xpcall that cannot swallow the watchdog or an out-of-memory error.
static int fatal_status(int st) { return st != LUA_OK && (stuck || st == LUA_ERRMEM); }

static int safe_pcall(lua_State *L)
{
    luaL_checkany(L, 1);
    int st = lua_pcall(L, lua_gettop(L) - 1, LUA_MULTRET, 0);
    if (fatal_status(st)) return lua_error(L);
    lua_pushboolean(L, st == LUA_OK);
    lua_insert(L, 1);
    return lua_gettop(L);
}

static int safe_xpcall(lua_State *L)
{
    int n = lua_gettop(L);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_replace(L, 1);   // msgh at 1
    lua_replace(L, 2);   // function at 2
    int st = lua_pcall(L, n - 2, LUA_MULTRET, 1);
    if (fatal_status(st)) return lua_error(L);
    lua_pushboolean(L, st == LUA_OK);
    lua_replace(L, 1);
    return lua_gettop(L);
}

// ---------- API: gfx ----------
static int arg_int(lua_State *L, int i) { return (int)floorf((float)luaL_checknumber(L, i)); }

static int arg_color(lua_State *L, int i)
{
    lua_Integer c = luaL_checkinteger(L, i);
    if (c < 0 || c > 31) luaL_error(L, "colour must be 0-31, got %d", (int)c);
    return (int)c;
}

static int l_clear(lua_State *L) { gfx_clear(lua_isnoneornil(L, 1) ? 0 : arg_color(L, 1)); return 0; }
static int l_pixel(lua_State *L) { gfx_pixel(arg_int(L, 1), arg_int(L, 2), arg_color(L, 3)); return 0; }
static int l_line(lua_State *L) { gfx_line(arg_int(L, 1), arg_int(L, 2), arg_int(L, 3), arg_int(L, 4), arg_color(L, 5)); return 0; }
static int l_rect(lua_State *L) { gfx_rect(arg_int(L, 1), arg_int(L, 2), arg_int(L, 3), arg_int(L, 4), arg_color(L, 5)); return 0; }
static int l_circle(lua_State *L) { gfx_circle(arg_int(L, 1), arg_int(L, 2), arg_int(L, 3), arg_color(L, 4), 0); return 0; }
static int l_fill_circle(lua_State *L) { gfx_circle(arg_int(L, 1), arg_int(L, 2), arg_int(L, 3), arg_color(L, 4), 1); return 0; }
static int l_fill_rect(lua_State *L) { gfx_fill_rect(arg_int(L, 1), arg_int(L, 2), arg_int(L, 3), arg_int(L, 4), arg_color(L, 5)); return 0; }

static int l_text(lua_State *L)
{
    // Read every argument before luaL_tolstring pushes a string, which would land in a missing
    // argument's slot (lua_toboolean(L, 5) would then see that string and turn big text on).
    int x = arg_int(L, 2), y = arg_int(L, 3), c = arg_color(L, 4), scale = lua_toboolean(L, 5) ? 2 : 1;
    const char *s = luaL_tolstring(L, 1, NULL);
    lua_pushinteger(L, gfx_text(s, x, y, c, scale));
    return 1;
}

static int l_text_width(lua_State *L)
{
    int scale = lua_toboolean(L, 2) ? 2 : 1;
    lua_pushinteger(L, gfx_text_width(luaL_tolstring(L, 1, NULL), scale));
    return 1;
}

// gfx.label(s, x, y, c, [size]): smooth OS-style text. size 1-4 (small, medium, large, extra large);
// y is the top of the capital letters. Returns the width.
static int label_size(lua_State *L, int i)
{
    int n = lua_isnoneornil(L, i) ? 2 : arg_int(L, i);
    return n < 1 ? FONT_S : n > 4 ? FONT_XL : n - 1;
}

static int l_label(lua_State *L)
{
    int x = arg_int(L, 2), y = arg_int(L, 3), c = arg_color(L, 4), size = label_size(L, 5);
    const char *s = luaL_tolstring(L, 1, NULL);
    lua_pushinteger(L, gfx_aa_text(s, x, y, c, size));
    return 1;
}

static int l_label_width(lua_State *L)
{
    int size = label_size(L, 2);
    lua_pushinteger(L, gfx_aa_width(luaL_tolstring(L, 1, NULL), size));
    return 1;
}

// ---------- API: knob ----------
static vm_input_t in;             // input for the current frame
static int notch_acc[2], notches[2];

static int arg_side(lua_State *L, int i)
{
    const char *s = luaL_checkstring(L, i);
    if (!strcmp(s, "left")) return VM_LEFT;
    if (!strcmp(s, "right")) return VM_RIGHT;
    return luaL_error(L, "knob side must be \"left\" or \"right\", got \"%s\"", s);
}

static int l_turn(lua_State *L)     { lua_pushinteger(L, notches[arg_side(L, 1)]); return 1; }
static int l_pos(lua_State *L)      { lua_pushinteger(L, in.pos[arg_side(L, 1)]); return 1; }
static int l_speed(lua_State *L)    { lua_pushnumber(L, in.speed[arg_side(L, 1)]); return 1; }
static int l_pressed(lua_State *L)  { lua_pushboolean(L, in.pressed[arg_side(L, 1)]); return 1; }
static int l_held(lua_State *L)     { lua_pushboolean(L, in.down[arg_side(L, 1)]); return 1; }
static int l_released(lua_State *L) { lua_pushboolean(L, in.released[arg_side(L, 1)]); return 1; }
static int l_long(lua_State *L)     { lua_pushboolean(L, in.long_press[arg_side(L, 1)]); return 1; }

// ---------- API: tilt ----------
static float tilt_zero[2];   // tilt.zero() makes the current tilt the new level

static float tilt_axis(int i)
{
    float v = in.tilt[i] - tilt_zero[i];
    return v < -1.f ? -1.f : v > 1.f ? 1.f : v;
}

static int l_tilt_x(lua_State *L)     { lua_pushnumber(L, tilt_axis(0)); return 1; }
static int l_tilt_y(lua_State *L)     { lua_pushnumber(L, tilt_axis(1)); return 1; }
static int l_tilt_shake(lua_State *L) { lua_pushboolean(L, in.shake); return 1; }
static int l_tilt_zero(lua_State *L)  { tilt_zero[0] = in.tilt[0]; tilt_zero[1] = in.tilt[1]; (void)L; return 0; }

// ---------- API: sys ----------
static uint32_t frame_no;
static int64_t cart_t0;
static float fps_now;

static int l_time(lua_State *L)
{
    lua_Integer ms = det ? (lua_Integer)frame_no * 1000 / cart_fps_v : (lua_Integer)((plat_time_us() - cart_t0) / 1000);
    lua_pushinteger(L, ms);
    return 1;
}
static int left_handed;
void vm_set_left_handed(int left) { left_handed = left != 0; }
static int l_hands(lua_State *L) { lua_pushstring(L, left_handed ? "left" : "right"); return 1; }
static int l_frame(lua_State *L) { lua_pushinteger(L, (lua_Integer)frame_no); return 1; }
static int l_fps(lua_State *L)   { lua_pushnumber(L, det ? (lua_Number)cart_fps_v : fps_now); return 1; }

static void json_str(const char *s)   // prints s as a JSON string body
{
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { putchar('\\'); putchar(*s); }
        else if (*s == '\n') fputs("\\n", stdout);
        else if ((unsigned char)*s < 0x20) putchar(' ');
        else putchar(*s);
    }
}

static int scanning;   // vm_scan runs a cart's top level only to read its cart table: stay quiet

static int l_log(lua_State *L)
{
    if (scanning) return 0;
    int n = lua_gettop(L);
    fputs("{\"t\":\"log\",\"msg\":\"", stdout);
    for (int i = 1; i <= n; i++) { if (i > 1) putchar(' '); json_str(luaL_tolstring(L, i, NULL)); lua_pop(L, 1); }
    fputs("\"}\n", stdout);
    return 0;
}

// ---------- sandbox ----------
static int readonly_newindex(lua_State *L)
{
    return luaL_error(L, "the %s table can't be changed", lua_tostring(L, lua_upvalueindex(1)));
}

// Leaves a read-only proxy for the table on top of the stack in place of the table.
static void make_readonly(lua_State *L, const char *name)
{
    lua_newtable(L);                       // proxy
    lua_newtable(L);                       // metatable
    lua_pushvalue(L, -3);
    lua_setfield(L, -2, "__index");
    lua_pushstring(L, name);
    lua_pushcclosure(L, readonly_newindex, 1);
    lua_setfield(L, -2, "__newindex");
    lua_pushliteral(L, "locked");
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, -2);
    lua_remove(L, -2);                     // drop the real table; the proxy keeps it via __index
}

static void open_sandbox(lua_State *L, const char *save_id)
{
    static const luaL_Reg libs[] = {
        { LUA_GNAME, luaopen_base }, { LUA_STRLIBNAME, luaopen_string }, { LUA_TABLIBNAME, luaopen_table },
        { LUA_MATHLIBNAME, luaopen_math }, { LUA_UTF8LIBNAME, luaopen_utf8 }, { NULL, NULL },
    };
    for (const luaL_Reg *l = libs; l->func; l++) { luaL_requiref(L, l->name, l->func, 1); lua_pop(L, 1); }

    // Base library: no code loading from strings or files, no manual GC control.
    static const char *drop[] = { "dofile", "loadfile", "load", "collectgarbage", NULL };
    for (const char **d = drop; *d; d++) { lua_pushnil(L); lua_setglobal(L, *d); }
    lua_getglobal(L, "string"); lua_pushnil(L); lua_setfield(L, -2, "dump"); lua_pop(L, 1);
    lua_pushcfunction(L, safe_pcall);  lua_setglobal(L, "pcall");
    lua_pushcfunction(L, safe_xpcall); lua_setglobal(L, "xpcall");
    lua_pushcfunction(L, l_log);       lua_setglobal(L, "print");

    static const luaL_Reg gfx_fns[] = {
        { "clear", l_clear }, { "pixel", l_pixel }, { "line", l_line }, { "rect", l_rect },
        { "fill_rect", l_fill_rect }, { "circle", l_circle }, { "fill_circle", l_fill_circle }, { "text", l_text }, { "text_width", l_text_width }, { "label", l_label }, { "label_width", l_label_width }, { NULL, NULL },
    };
    static const luaL_Reg knob_fns[] = {
        { "turn", l_turn }, { "pos", l_pos }, { "speed", l_speed }, { "pressed", l_pressed },
        { "held", l_held }, { "released", l_released }, { "long", l_long }, { NULL, NULL },
    };
    static const luaL_Reg sys_fns[] = {
        { "time", l_time }, { "frame", l_frame }, { "hands", l_hands }, { "fps", l_fps }, { "log", l_log }, { NULL, NULL },
    };
    luaL_newlib(L, gfx_fns);
    lua_pushinteger(L, SCREEN_W); lua_setfield(L, -2, "W");
    lua_pushinteger(L, SCREEN_H); lua_setfield(L, -2, "H");
    engine_open(L, -1);                     // adds gfx.sprite/gfx.map, pushes the map table
    make_readonly(L, "map");  lua_setglobal(L, "map");
    make_readonly(L, "gfx");  lua_setglobal(L, "gfx");
    luaL_newlib(L, knob_fns); make_readonly(L, "knob"); lua_setglobal(L, "knob");
    static const luaL_Reg tilt_fns[] = {
        { "x", l_tilt_x }, { "y", l_tilt_y }, { "shake", l_tilt_shake }, { "zero", l_tilt_zero }, { NULL, NULL },
    };
    luaL_newlib(L, tilt_fns); make_readonly(L, "tilt"); lua_setglobal(L, "tilt");
    luaL_newlib(L, sys_fns);  make_readonly(L, "sys");  lua_setglobal(L, "sys");
    save_open(L, save_id, plat_time_us());
    make_readonly(L, "save"); lua_setglobal(L, "save");
}

// ---------- error screens ----------
static vm_err_t err_kind;
static int err_line;
static char err_msg[160];

static int msgh(lua_State *L)   // adds a traceback to the error message
{
    const char *m = lua_tostring(L, 1);
    luaL_traceback(L, L, m ? m : "(error object is not a string)", 1);
    return 1;
}

static void record_error(lua_State *L, int st)
{
    const char *full = lua_tostring(L, -1);
    if (!full) full = "unknown error";
    err_kind = stuck ? VM_ERR_STUCK : st == LUA_ERRMEM ? VM_ERR_MEMORY : VM_ERR_BUG;
    err_line = stuck ? stuck_line : -1;
    // First line of the message, without the "game:12: " position, which becomes err_line.
    const char *m = full;
    const char *p = strstr(full, "game:");
    if (p == full) {
        int line = atoi(p + 5);
        const char *after = strstr(p + 5, ": ");
        if (line > 0 && after) { if (err_line < 0) err_line = line; m = after + 2; }
    }
    size_t n = strcspn(m, "\n");
    if (n >= sizeof err_msg) n = sizeof err_msg - 1;
    memcpy(err_msg, m, n); err_msg[n] = 0;
    if (err_kind == VM_ERR_MEMORY) snprintf(err_msg, sizeof err_msg, "Your game ran out of memory");

    static const char *kinds[] = { "none", "bug", "stuck", "memory", "stopped" };
    printf("{\"t\":\"error\",\"kind\":\"%s\",\"line\":%d,\"msg\":\"", kinds[err_kind], err_line);
    json_str(err_msg);
    fputs("\",\"trace\":\"", stdout);
    json_str(full);
    fputs("\"}\n", stdout);
}

static void wrap_text(const char *s, int x, int y, int max_chars, int c)
{
    char line[64];
    while (*s && y < SCREEN_H - 30) {
        int n = (int)strlen(s);
        if (n > max_chars) {
            n = max_chars;
            while (n > 0 && s[n] != ' ') n--;
            if (n == 0) n = max_chars;
        }
        memcpy(line, s, n); line[n] = 0;
        gfx_text(line, x, y, c, 1);
        s += n;
        while (*s == ' ') s++;
        y += 10;
    }
}

void vm_draw_error_screen(void)
{
    gfx_clear(OS_BLUE);
    const char *title = err_kind == VM_ERR_STUCK ? "Your game got stuck"
                      : err_kind == VM_ERR_MEMORY ? "Out of memory"
                      : err_kind == VM_ERR_STOPPED ? "Game stopped"
                      : "Oops! Your game hit a bug.";
    gfx_fill_rect(0, 0, SCREEN_W, 26, err_kind == VM_ERR_STOPPED ? OS_GREEN : OS_RED);
    gfx_aa_text(title, 8, 7, OS_WHITE, FONT_M);
    if (err_kind == VM_ERR_STUCK) wrap_text("Maybe a loop that never ends?", 10, 40, 50, OS_WHITE);
    else if (err_kind == VM_ERR_STOPPED) wrap_text("Both knobs were held for 3 seconds.", 10, 40, 50, OS_WHITE);
    else wrap_text(err_msg, 10, 40, 50, OS_WHITE);
    if (err_line > 0) {
        char b[24];
        snprintf(b, sizeof b, "Line %d", err_line);
        gfx_aa_text(b, 10, 100, OS_YELLOW, FONT_L);
    }
    gfx_aa_text("Click a knob to go back", 10, SCREEN_H - 22, OS_WHITE, FONT_M);
}

// ---------- running a cart ----------
static int call_cb(lua_State *L, const char *name, int required)
{
    lua_getglobal(L, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        if (!required) return LUA_OK;
        lua_pushfstring(L, "Your game needs a %s() function", name);
        return LUA_ERRRUN;
    }
    lua_pushcfunction(L, msgh);
    lua_insert(L, -2);
    cb_start = plat_time_us();
    int st = lua_pcall(L, 0, 0, -2);
    if (st == LUA_OK) lua_pop(L, 1);
    else lua_remove(L, -2);   // leave just the message
    return st;
}

static int pcall_c(lua_State *L, lua_CFunction f)
{
    lua_pushcfunction(L, msgh);
    lua_pushcfunction(L, f);
    cb_start = plat_time_us();
    int st = lua_pcall(L, 0, 0, -2);
    if (st == LUA_OK) lua_pop(L, 1); else lua_remove(L, -2);
    return st;
}

static int run_chunk(lua_State *L, const char *src, size_t len, const char *chunkname)
{
    // Text only ("t"): precompiled bytecode can crash the VM, so it is never accepted.
    int st = luaL_loadbufferx(L, src, len, chunkname, "t");
    if (st != LUA_OK) return st;
    lua_pushcfunction(L, msgh);
    lua_insert(L, -2);
    cb_start = plat_time_us();
    st = lua_pcall(L, 0, 0, -2);
    if (st == LUA_OK) lua_pop(L, 1); else lua_remove(L, -2);
    return st;
}

static int cart_fps(lua_State *L, int *fps)
{
    lua_getglobal(L, "cart");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_pushliteral(L, "Your game needs a cart = { ... } table at the top");
        return LUA_ERRRUN;
    }
    lua_getfield(L, -1, "fps");
    *fps = lua_isnil(L, -1) ? 30 : (int)lua_tointeger(L, -1);
    lua_getfield(L, -2, "api");
    int api = lua_isnil(L, -1) ? 1 : (int)lua_tointeger(L, -1);
    lua_pop(L, 3);
    if (*fps != 30 && *fps != 60) { lua_pushliteral(L, "cart.fps must be 30 or 60"); return LUA_ERRRUN; }
    cart_fps_v = *fps;
    if (api != 1) { lua_pushfstring(L, "cart.api %d is not supported; this console runs api 1", api); return LUA_ERRRUN; }
    return LUA_OK;
}

// ---------- strict globals (checking only) ----------
// Reading a name from another console or language (btn, spr, love, ...) is an error with a hint.
// Names from other consoles and languages, with what to use instead.
static const char *const HINTS[][2] = {
    { "btn", "use knob.pressed(\"left\"), knob.held(...) or knob.turn(...)" },
    { "btnp", "use knob.pressed(\"left\") or knob.turn(...)" },
    { "spr", "use gfx.sprite(name, x, y)" }, { "cls", "use gfx.clear(colour)" },
    { "pset", "use gfx.pixel(x, y, colour)" }, { "rectfill", "use gfx.fill_rect(x, y, w, h, colour)" },
    { "rect", "use gfx.rect(x, y, w, h, colour)" }, { "circfill", "use gfx.fill_circle(x, y, r, colour)" },
    { "circ", "use gfx.circle(x, y, r, colour)" }, { "line", "use gfx.line(x1, y1, x2, y2, colour)" },
    { "rnd_float", "use rnd(n)" }, { "random", "use rnd(n) or rnd_int(a, b)" }, { "flr", "use math.floor(x)" },
    { "abs", "use math.abs(x)" }, { "min", "use math.min(a, b)" }, { "max", "use math.max(a, b)" },
    { "sin", "use math.sin(x) (radians)" }, { "cos", "use math.cos(x) (radians)" }, { "sqrt", "use math.sqrt(x)" },
    { "love", "TinyGame isn't LOVE2D: see gfx, knob, tilt and save" }, { "os", "use sys.time() for time" },
    { "io", "use save.set/save.get to keep data" }, { "require", "put everything in one file" },
    { "screen_width", "use gfx.W" }, { "screen_height", "use gfx.H" }, { "width", "use gfx.W" }, { "height", "use gfx.H" },
};

static int strict_index(lua_State *L)
{
    lua_Debug ar;
    // Only Lua code is checked; the host and C functions read globals that may legitimately be absent.
    if (!lua_getstack(L, 1, &ar) || !lua_getinfo(L, "Sl", &ar) || ar.what[0] == 'C') return 0;
    const char *k = lua_tostring(L, 2);
    if (!k) return 0;
    for (size_t i = 0; i < sizeof HINTS / sizeof HINTS[0]; i++)
        if (!strcmp(k, HINTS[i][0])) return luaL_error(L, "unknown name '%s': %s", k, HINTS[i][1]);
    // Any other unset name reads as nil, as in normal Lua: `if not stars then stars = {} end` is a
    // common, correct pattern. Calling a misspelt function still fails ("attempt to call a nil value").
    return 0;
}

static void make_strict(lua_State *L)
{
    lua_pushglobaltable(L);
    lua_newtable(L);
    lua_pushcfunction(L, strict_index);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
}

// ---------- lifecycle ----------
static uint32_t last_update_us, last_draw_us, last_gc_us;

static int seed_random(lua_State *L)
{
    lua_getglobal(L, "math");
    lua_getfield(L, -1, "randomseed");
    lua_pushinteger(L, (lua_Integer)lua_tointeger(L, 1));
    lua_call(L, 1, 0);
    lua_pop(L, 1);
    return 0;
}

static int fail(int st)
{
    record_error(L_, st);
    return -1;
}

int vm_start(const char *src, size_t len, const char *name, const vm_opts_t *opts)
{
    stuck = 0; err_kind = VM_ERR_NONE; err_line = -1; err_msg[0] = 0; mem_peak = 0;
    det = opts->deterministic;
    cart_fps_v = 30;
    strncpy(cur_name, name, sizeof cur_name - 1);
    cur_name[sizeof cur_name - 1] = 0;
    engine_reset();
    gfx_clear(0);   // every cart starts on a black screen, so frame hashes match across platforms
    L_ = lua_newstate(l_alloc, NULL);
    if (!L_) {
        err_kind = VM_ERR_MEMORY;
        strcpy(err_msg, "Your game ran out of memory");
        printf("{\"t\":\"error\",\"kind\":\"memory\",\"line\":-1,\"msg\":\"Your game ran out of memory\"}\n");
        return -1;
    }
    lua_State *L = L_;
    lua_gc(L, LUA_GCGEN, 0, 0);
    lua_sethook(L, hook, LUA_MASKCOUNT, HOOK_COUNT);
    open_sandbox(L, opts->save_id);
    printf("{\"t\":\"cart\",\"name\":\"%s\",\"bytes\":%u,\"lua_kb\":%u,\"deterministic\":%d}\n",
           cur_name, (unsigned)len, (unsigned)(mem_used / 1024), det);

    cart_t0 = plat_time_us();
    frame_no = 0;
    memset(&in, 0, sizeof in);
    memset(tilt_zero, 0, sizeof tilt_zero);
    memset(notch_acc, 0, sizeof notch_acc);
    memset(notches, 0, sizeof notches);

    int fps = 30;
    int st = LUA_OK;
    if (det) {
        lua_pushcfunction(L, msgh);
        lua_pushcfunction(L, seed_random);
        lua_pushinteger(L, (lua_Integer)opts->seed);
        st = lua_pcall(L, 1, 0, -3);
        if (st == LUA_OK) lua_pop(L, 1); else lua_remove(L, -2);
    }
    if (st == LUA_OK) st = run_chunk(L, opts->prelude, opts->prelude_len, "=prelude");
    if (st == LUA_OK && opts->strict) make_strict(L);
    if (st == LUA_OK) st = run_chunk(L, src, len, "=game");
    if (st == LUA_OK) st = pcall_c(L, engine_load_assets);
    if (st == LUA_OK) st = cart_fps(L, &fps);
    if (st == LUA_OK) st = call_cb(L, "init", 0);
    return st == LUA_OK ? 0 : fail(st);
}

int vm_frame(const vm_input_t *input)
{
    lua_State *L = L_;
    if (!L || err_kind != VM_ERR_NONE) return -1;
    in = *input;
    for (int i = 0; i < 2; i++) {
        notch_acc[i] += in.delta[i];
        notches[i] = notch_acc[i] / 4;   // 4 transitions = 1 notch, 20 per turn
        notch_acc[i] -= notches[i] * 4;
    }
    int64_t t0 = plat_time_us();
    int st = call_cb(L, "update", 0);
    int64_t t1 = plat_time_us();
    if (st == LUA_OK) st = call_cb(L, "draw", 1);
    int64_t t2 = plat_time_us();
    if (st != LUA_OK) return fail(st);
    cb_start = t2;
    lua_gc(L, LUA_GCSTEP, 0);   // one young collection per frame, after draw, so pauses stay small
    int64_t t3 = plat_time_us();
    save_flush(L, t3, 0);
    last_update_us = (uint32_t)(t1 - t0);
    last_draw_us = (uint32_t)(t2 - t1);
    last_gc_us = (uint32_t)(t3 - t2);
    frame_no++;
    return 0;
}

int vm_pause_stats(vm_stat_t *out, int max)
{
    lua_State *L = L_;
    if (!L || err_kind != VM_ERR_NONE) return 0;
    int top = lua_gettop(L), n = 0;
    lua_getglobal(L, "stats");
    if (lua_isfunction(L, -1)) {
        cb_start = plat_time_us();
        if (lua_pcall(L, 0, 1, 0) == LUA_OK && lua_istable(L, -1)) {
            for (; n < max; n++) {
                lua_rawgeti(L, -1, n + 1);
                if (!lua_istable(L, -1)) { lua_pop(L, 1); break; }
                lua_rawgeti(L, -1, 1);
                lua_rawgeti(L, -2, 2);
                const char *a = lua_type(L, -2) == LUA_TSTRING || lua_type(L, -2) == LUA_TNUMBER ? lua_tostring(L, -2) : "";
                const char *b = lua_type(L, -1) == LUA_TSTRING || lua_type(L, -1) == LUA_TNUMBER ? lua_tostring(L, -1) : "";
                snprintf(out[n].label, sizeof out[n].label, "%s", a);
                snprintf(out[n].value, sizeof out[n].value, "%s", b);
                lua_pop(L, 3);
            }
        }
    }
    lua_settop(L, top);
    return n;
}

static void copy_field(lua_State *L, const char *k, char *out, size_t n)
{
    lua_getfield(L, -1, k);
    const char *v = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : NULL;
    if (v) { strncpy(out, v, n - 1); out[n - 1] = 0; }
    lua_pop(L, 1);
}

int vm_scan(const char *src, size_t len, const vm_opts_t *opts, vm_meta_t *m)
{
    memset(m, 0, sizeof *m);
    if (L_) return -1;   // a cart is running; scanning shares the one Lua slot
    stuck = 0;
    lua_State *L = lua_newstate(l_alloc, NULL);
    if (!L) return -1;
    lua_gc(L, LUA_GCGEN, 0, 0);
    lua_sethook(L, hook, LUA_MASKCOUNT, HOOK_COUNT);
    L_ = L;
    scanning = 1;
    open_sandbox(L, NULL);
    int st = run_chunk(L, opts->prelude, opts->prelude_len, "=prelude");
    if (st == LUA_OK) st = run_chunk(L, src, len, "=game");
    int ok = 0;
    if (st == LUA_OK) {
        lua_getglobal(L, "cart");
        if (lua_istable(L, -1)) {
            copy_field(L, "title", m->title, sizeof m->title);
            copy_field(L, "tagline", m->tagline, sizeof m->tagline);
            copy_field(L, "icon", m->icon, sizeof m->icon);
            lua_getfield(L, -1, "diagnostic");
            m->diagnostic = lua_toboolean(L, -1);
            lua_pop(L, 1);
            ok = 1;
        }
        lua_pop(L, 1);
    }
    cb_start = plat_time_us();
    lua_close(L);
    L_ = NULL;
    scanning = 0;
    engine_reset();
    stuck = 0;
    return ok ? 0 : -1;
}

void vm_stop(void)
{
    if (L_) {
        save_flush(L_, plat_time_us(), 1);   // the last changes, even if the game stopped on an error
        cb_start = plat_time_us();   // finalizers run inside lua_close under the same watchdog
        lua_close(L_);
        L_ = NULL;
    }
    engine_reset();
    stuck = 0;
}

void vm_stopped_by_user(void)
{
    err_kind = VM_ERR_STOPPED;
    err_line = -1;
    printf("{\"t\":\"error\",\"kind\":\"stopped\"}\n");
}

vm_err_t vm_error(void) { return err_kind; }
const char *vm_error_message(int *line) { if (line) *line = err_line; return err_msg; }
int      vm_fps(void) { return cart_fps_v; }
void     vm_set_measured_fps(float f) { fps_now = f; }
uint32_t vm_frame_count(void) { return frame_no; }
void     vm_reset_peak(void) { mem_peak = mem_used; }

void vm_get_stats(vm_stats_t *o)
{
    o->update_us = last_update_us;
    o->draw_us = last_draw_us;
    o->gc_us = last_gc_us;
    o->lua_bytes = mem_used;
    o->lua_fast_bytes = mem_int;
    o->lua_peak_bytes = mem_peak;
    o->asset_bytes = engine_asset_bytes();
}

uint32_t vm_frame_hash(void)
{
    const uint8_t *p = plat_framebuffer();
    uint32_t h = 2166136261u;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
