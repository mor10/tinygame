// Lua host: sandboxed Lua 5.4 state, memory cap, instruction-count watchdog, error screens,
// and the Phase 3 API (cart table, init/update/draw, gfx, knob, sys).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "gfx.h"
#include "display.h"
#include "input.h"
#include "engine.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define MEM_CAP          (1024 * 1024)   // per cart, all heaps
#define MEM_INTERNAL_CAP (64 * 1024)     // small blocks go to internal RAM up to this much
#define SMALL_BLOCK      256
#define WATCHDOG_US      250000          // one callback may run this long
#define HOOK_COUNT       1000            // instructions between watchdog checks
#define CART_MAX         (128 * 1024)

// ---------- cart handoff ----------
static SemaphoreHandle_t cart_lock;
static char *pending_src, *cur_src;
static size_t pending_len, cur_len;
static char pending_name[32], cur_name[32];
static volatile int restart_req;

void game_load(const char *src, size_t len, const char *name)
{
    char *copy = heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) { printf("{\"t\":\"error\",\"kind\":\"load\",\"msg\":\"no memory for cart\"}\n"); return; }
    memcpy(copy, src, len);
    copy[len] = 0;
    xSemaphoreTake(cart_lock, portMAX_DELAY);
    free(pending_src);
    pending_src = copy;
    pending_len = len;
    strlcpy(pending_name, name, sizeof pending_name);
    xSemaphoreGive(cart_lock);
}

void game_restart(void) { restart_req = 1; }

static int take_pending(void)
{
    int got = 0;
    xSemaphoreTake(cart_lock, portMAX_DELAY);
    if (pending_src) {
        free(cur_src);
        cur_src = pending_src; cur_len = pending_len;
        strlcpy(cur_name, pending_name, sizeof cur_name);
        pending_src = NULL;
        got = 1;
    }
    xSemaphoreGive(cart_lock);
    return got;
}

static int has_pending(void) { return pending_src != NULL; }

// ---------- allocator with a cap ----------
static size_t mem_used, mem_int, mem_peak;

static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    if (!ptr) osize = 0;   // when ptr is NULL, osize carries a type tag, not a size
    if (nsize == 0) {
        if (ptr) {
            if (esp_ptr_internal(ptr)) mem_int -= osize;
            mem_used -= osize;
            heap_caps_free(ptr);
        }
        return NULL;
    }
    if (mem_used - osize + nsize > MEM_CAP) return NULL;   // Lua runs a full GC and retries once
    int want_int = nsize <= SMALL_BLOCK && mem_int + nsize <= MEM_INTERNAL_CAP;
    uint32_t caps = (want_int ? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM) | MALLOC_CAP_8BIT;
    void *np = ptr ? heap_caps_realloc(ptr, nsize, caps) : heap_caps_malloc(nsize, caps);
    if (!np && want_int) {
        caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        np = ptr ? heap_caps_realloc(ptr, nsize, caps) : heap_caps_malloc(nsize, caps);
    }
    if (!np) return NULL;
    if (ptr && esp_ptr_internal(ptr)) mem_int -= osize;
    mem_used -= osize;
    if (esp_ptr_internal(np)) mem_int += nsize;
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
    if (esp_timer_get_time() - cb_start < WATCHDOG_US) return;
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
    if (c < 0 || c > 15) luaL_error(L, "colour must be 0-15, got %d", (int)c);
    return (int)c;
}

static int l_clear(lua_State *L) { gfx_clear(lua_isnoneornil(L, 1) ? 0 : arg_color(L, 1)); return 0; }
static int l_pixel(lua_State *L) { gfx_pixel(arg_int(L, 1), arg_int(L, 2), arg_color(L, 3)); return 0; }
static int l_line(lua_State *L) { gfx_line(arg_int(L, 1), arg_int(L, 2), arg_int(L, 3), arg_int(L, 4), arg_color(L, 5)); return 0; }
static int l_rect(lua_State *L) { gfx_rect(arg_int(L, 1), arg_int(L, 2), arg_int(L, 3), arg_int(L, 4), arg_color(L, 5)); return 0; }
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

// ---------- API: knob ----------
static input_state_t in;          // snapshot for the current frame
static int notch_acc[2], notches[2];

static int arg_side(lua_State *L, int i)
{
    const char *s = luaL_checkstring(L, i);
    if (!strcmp(s, "left")) return KNOB_LEFT;
    if (!strcmp(s, "right")) return KNOB_RIGHT;
    return luaL_error(L, "knob side must be \"left\" or \"right\", got \"%s\"", s);
}

static int l_turn(lua_State *L)     { lua_pushinteger(L, notches[arg_side(L, 1)]); return 1; }
static int l_pos(lua_State *L)      { lua_pushinteger(L, in.pos[arg_side(L, 1)]); return 1; }
static int l_speed(lua_State *L)    { lua_pushnumber(L, in.speed[arg_side(L, 1)]); return 1; }
static int l_pressed(lua_State *L)  { lua_pushboolean(L, in.pressed[arg_side(L, 1)]); return 1; }
static int l_held(lua_State *L)     { lua_pushboolean(L, in.down[arg_side(L, 1)]); return 1; }
static int l_released(lua_State *L) { lua_pushboolean(L, in.released[arg_side(L, 1)]); return 1; }
static int l_long(lua_State *L)     { lua_pushboolean(L, in.long_press[arg_side(L, 1)]); return 1; }

// ---------- API: sys ----------
static uint32_t frame_no;
static int64_t cart_t0;
static float fps_now;

static int l_time(lua_State *L)  { lua_pushinteger(L, (lua_Integer)((esp_timer_get_time() - cart_t0) / 1000)); return 1; }
static int l_frame(lua_State *L) { lua_pushinteger(L, (lua_Integer)frame_no); return 1; }
static int l_fps(lua_State *L)   { lua_pushnumber(L, fps_now); return 1; }

static void json_str(const char *s)   // prints s as a JSON string body
{
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { putchar('\\'); putchar(*s); }
        else if (*s == '\n') fputs("\\n", stdout);
        else if ((unsigned char)*s < 0x20) putchar(' ');
        else putchar(*s);
    }
}

static int l_log(lua_State *L)
{
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

static void open_sandbox(lua_State *L)
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
        { "fill_rect", l_fill_rect }, { "text", l_text }, { "text_width", l_text_width }, { NULL, NULL },
    };
    static const luaL_Reg knob_fns[] = {
        { "turn", l_turn }, { "pos", l_pos }, { "speed", l_speed }, { "pressed", l_pressed },
        { "held", l_held }, { "released", l_released }, { "long", l_long }, { NULL, NULL },
    };
    static const luaL_Reg sys_fns[] = {
        { "time", l_time }, { "frame", l_frame }, { "fps", l_fps }, { "log", l_log }, { NULL, NULL },
    };
    luaL_newlib(L, gfx_fns);
    lua_pushinteger(L, SCREEN_W); lua_setfield(L, -2, "W");
    lua_pushinteger(L, SCREEN_H); lua_setfield(L, -2, "H");
    engine_open(L, -1);                     // adds gfx.sprite/gfx.map, pushes the map table
    make_readonly(L, "map");  lua_setglobal(L, "map");
    make_readonly(L, "gfx");  lua_setglobal(L, "gfx");
    luaL_newlib(L, knob_fns); make_readonly(L, "knob"); lua_setglobal(L, "knob");
    luaL_newlib(L, sys_fns);  make_readonly(L, "sys");  lua_setglobal(L, "sys");
}

// ---------- error screens ----------
typedef enum { ERR_NONE, ERR_BUG, ERR_STUCK, ERR_MEMORY, ERR_STOPPED } err_kind_t;
static err_kind_t err_kind;
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
    err_kind = stuck ? ERR_STUCK : st == LUA_ERRMEM ? ERR_MEMORY : ERR_BUG;
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
    if (err_kind == ERR_MEMORY) strlcpy(err_msg, "Your game ran out of memory", sizeof err_msg);

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

static void draw_error_screen(void)
{
    gfx_clear(OS_CREAM);
    const char *title = err_kind == ERR_STUCK ? "YOUR GAME GOT STUCK"
                      : err_kind == ERR_MEMORY ? "OUT OF MEMORY"
                      : err_kind == ERR_STOPPED ? "GAME STOPPED"
                      : "OOPS! YOUR GAME HIT A BUG.";
    gfx_fill_rect(0, 0, SCREEN_W, 26, err_kind == ERR_STOPPED ? OS_TEAL : OS_CORAL);
    gfx_text(title, 8, 6, OS_WHITE, 2);
    if (err_kind == ERR_STUCK) wrap_text("Maybe a loop that never ends?", 10, 40, 50, OS_NAVY);
    else if (err_kind == ERR_STOPPED) wrap_text("Both knobs were held for 3 seconds.", 10, 40, 50, OS_NAVY);
    else wrap_text(err_msg, 10, 40, 50, OS_NAVY);
    if (err_line > 0) {
        char b[24];
        snprintf(b, sizeof b, "LINE %d", err_line);
        gfx_text(b, 10, 100, OS_CORAL, 2);
    }
    gfx_text("CLICK RIGHT KNOB TO RESTART", 10, SCREEN_H - 16, OS_GREY, 1);
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
    cb_start = esp_timer_get_time();
    int st = lua_pcall(L, 0, 0, -2);
    if (st == LUA_OK) lua_pop(L, 1);
    else lua_remove(L, -2);   // leave just the message
    return st;
}

static int pcall_c(lua_State *L, lua_CFunction f)
{
    lua_pushcfunction(L, msgh);
    lua_pushcfunction(L, f);
    cb_start = esp_timer_get_time();
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
    cb_start = esp_timer_get_time();
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
    if (api != 1) { lua_pushfstring(L, "cart.api %d is not supported; this console runs api 1", api); return LUA_ERRRUN; }
    return LUA_OK;
}

#define HIST_MS 64
static uint32_t frame_hist[HIST_MS];   // frame work time in 1 ms buckets since the cart started
static uint32_t gc_max;

static int frame_p99_ms(void)
{
    uint32_t total = 0, acc = 0;
    for (int i = 0; i < HIST_MS; i++) total += frame_hist[i];
    for (int i = 0; i < HIST_MS; i++) { acc += frame_hist[i]; if (acc * 100 >= total * 99) return i + 1; }
    return HIST_MS;
}

static void stats_tick(int64_t *window, int *frames, int *late, uint32_t *upd_max, uint32_t *draw_max)
{
    int64_t now = esp_timer_get_time();
    if (now - *window < 1000000) return;
    display_stats_t ds;
    display_take_stats(&ds);
    fps_now = *frames / ((now - *window) / 1e6f);
    printf("{\"t\":\"stats\",\"cart\":\"%s\",\"fps\":%.1f,\"late\":%d,\"update_us_max\":%u,\"draw_us_max\":%u,"
           "\"flush_us_avg\":%u,\"flush_us_max\":%u,\"kb_per_frame\":%.1f,\"lua_kb\":%u,\"lua_int_kb\":%u,"
           "\"lua_peak_kb\":%u,\"assets_kb\":%u,\"gc_us_max\":%u,\"frame_ms_p99\":%d,\"heap_int\":%u}\n",
           cur_name, fps_now, *late, (unsigned)*upd_max, (unsigned)*draw_max,
           ds.flushes ? (unsigned)(ds.flush_us_sum / ds.flushes) : 0, (unsigned)ds.flush_us_max,
           ds.flushes ? ds.bytes_sum / (float)ds.flushes / 1024.f : 0.f,
           (unsigned)(mem_used / 1024), (unsigned)(mem_int / 1024), (unsigned)(mem_peak / 1024),
           engine_asset_bytes() / 1024, (unsigned)gc_max, frame_p99_ms(),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    gc_max = 0;
    *window = now; *frames = 0; *late = 0; *upd_max = 0; *draw_max = 0;
}

static void wait_until(int64_t t)
{
    while (t - esp_timer_get_time() > 1500) vTaskDelay(1);
    while (esp_timer_get_time() < t) { }
}

// Runs the current cart until it errors, is stopped, or another cart arrives.
extern const char prelude_lua_start[] asm("_binary_prelude_lua_start");
extern const char prelude_lua_end[] asm("_binary_prelude_lua_end");

static void run_cart(void)
{
    stuck = 0; err_kind = ERR_NONE; mem_peak = 0;
    engine_reset();
    lua_State *L = lua_newstate(l_alloc, NULL);
    if (!L) { err_kind = ERR_MEMORY; strlcpy(err_msg, "Your game ran out of memory", sizeof err_msg); return; }
    lua_gc(L, LUA_GCGEN, 0, 0);
    lua_sethook(L, hook, LUA_MASKCOUNT, HOOK_COUNT);
    open_sandbox(L);
    printf("{\"t\":\"cart\",\"name\":\"%s\",\"bytes\":%u,\"lua_kb\":%u}\n", cur_name, (unsigned)cur_len, (unsigned)(mem_used / 1024));

    int fps = 30;
    cart_t0 = esp_timer_get_time();
    frame_no = 0;
    input_snapshot(&in);   // drop events from before the cart started
    memset(frame_hist, 0, sizeof frame_hist);
    memset(notch_acc, 0, sizeof notch_acc);

    int st = run_chunk(L, prelude_lua_start, (size_t)(prelude_lua_end - prelude_lua_start) - 1, "=prelude");
    if (st == LUA_OK) st = run_chunk(L, cur_src, cur_len, "=game");
    if (st == LUA_OK) st = pcall_c(L, engine_load_assets);
    if (st == LUA_OK) st = cart_fps(L, &fps);
    if (st == LUA_OK) st = call_cb(L, "init", 0);

    int64_t period = 1000000 / fps, next = esp_timer_get_time();
    int64_t window = next;
    int frames = 0, late = 0;
    uint32_t upd_max = 0, draw_max = 0;
    while (st == LUA_OK) {
        if (has_pending() || restart_req) break;
        input_snapshot(&in);
        if (in.chord_exit) { err_kind = ERR_STOPPED; err_line = -1; printf("{\"t\":\"error\",\"kind\":\"stopped\"}\n"); break; }
        for (int i = 0; i < 2; i++) {
            notch_acc[i] += in.delta[i];
            notches[i] = notch_acc[i] / 4;   // 4 transitions = 1 notch, 20 per turn
            notch_acc[i] -= notches[i] * 4;
        }
        int64_t t0 = esp_timer_get_time();
        st = call_cb(L, "update", 0);
        int64_t t1 = esp_timer_get_time();
        if (st == LUA_OK) st = call_cb(L, "draw", 1);
        int64_t t2 = esp_timer_get_time();
        if (st != LUA_OK) break;
        cb_start = t2;
        lua_gc(L, LUA_GCSTEP, 0);   // one young collection per frame, after draw, so pauses stay small
        int64_t t3 = esp_timer_get_time();
        if ((uint32_t)(t1 - t0) > upd_max) upd_max = (uint32_t)(t1 - t0);
        if ((uint32_t)(t2 - t1) > draw_max) draw_max = (uint32_t)(t2 - t1);
        if ((uint32_t)(t3 - t2) > gc_max) gc_max = (uint32_t)(t3 - t2);
        display_present();
        int work_ms = (int)((esp_timer_get_time() - t0) / 1000);   // game work plus waiting for the flush
        frame_hist[work_ms < HIST_MS ? work_ms : HIST_MS - 1]++;
        frame_no++; frames++;
        stats_tick(&window, &frames, &late, &upd_max, &draw_max);
        next += period;
        int64_t now = esp_timer_get_time();
        if (now > next) { late++; next = now; vTaskDelay(1); }
        else wait_until(next);
    }
    if (st != LUA_OK) record_error(L, st);
    cb_start = esp_timer_get_time();   // finalizers run inside lua_close under the same watchdog
    lua_close(L);
    engine_reset();
    stuck = 0;
}

static void game_task(void *arg)
{
    for (;;) {
        while (!take_pending() && !cur_src) vTaskDelay(pdMS_TO_TICKS(20));
        restart_req = 0;
        run_cart();
        if (err_kind == ERR_NONE) continue;   // replaced by a new cart or restarted
        draw_error_screen();
        display_present();
        for (;;) {   // wait for a restart click, a restart command or a new cart
            vTaskDelay(pdMS_TO_TICKS(33));
            input_snapshot(&in);
            if (in.pressed[KNOB_RIGHT] || restart_req || has_pending()) break;
        }
        take_pending();
        restart_req = 0;
    }
}

void game_start(void)
{
    cart_lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(game_task, "game", 16384, NULL, 5, NULL, 1);
}
