// Board runner: drives the OS shell (components/tinygame/os.c) on core 1 at os_fps(), takes
// requests from the serial console, and prints stats while a game runs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "os.h"
#include "vm.h"
#include "display.h"
#include "input.h"
#include "platform.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern const char prelude_lua_start[] asm("_binary_prelude_lua_start");
extern const char prelude_lua_end[] asm("_binary_prelude_lua_end");
extern const char brick_lua_start[] asm("_binary_brick_lua_start");
extern const char brick_lua_end[] asm("_binary_brick_lua_end");
extern const char hello_lua_start[] asm("_binary_hello_lua_start");
extern const char hello_lua_end[] asm("_binary_hello_lua_end");

// ---------- requests from the console task ----------
static SemaphoreHandle_t req_lock;
static char *upload_src;
static size_t upload_len;
static char upload_id[32], play_id[32], delete_id[32];
static volatile int list_req;
static volatile int restart_req, hash_frames_req;
static volatile uint32_t hash_seed_req;

void game_load(const char *src, size_t len, const char *id)
{
    char *copy = heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) { printf("{\"t\":\"error\",\"kind\":\"load\",\"msg\":\"no memory for cart\"}\n"); return; }
    memcpy(copy, src, len);
    copy[len] = 0;
    xSemaphoreTake(req_lock, portMAX_DELAY);
    free(upload_src);
    upload_src = copy;
    upload_len = len;
    snprintf(upload_id, sizeof upload_id, "%s", id);
    xSemaphoreGive(req_lock);
}

void game_play(const char *id)
{
    xSemaphoreTake(req_lock, portMAX_DELAY);
    snprintf(play_id, sizeof play_id, "%s", id);
    xSemaphoreGive(req_lock);
}

void game_delete(const char *id)
{
    xSemaphoreTake(req_lock, portMAX_DELAY);
    snprintf(delete_id, sizeof delete_id, "%s", id);
    xSemaphoreGive(req_lock);
}

void game_list(void) { list_req = 1; }

void game_restart(void) { restart_req = 1; }

void game_hash(int frames, uint32_t seed)
{
    hash_seed_req = seed;
    hash_frames_req = frames;
}

static void handle_requests(void)
{
    char *src = NULL;
    size_t len = 0;
    char up[32], play[32], del[32];
    xSemaphoreTake(req_lock, portMAX_DELAY);
    src = upload_src; len = upload_len; upload_src = NULL;
    memcpy(up, upload_id, 32); memcpy(play, play_id, 32); memcpy(del, delete_id, 32);
    play_id[0] = delete_id[0] = 0;
    xSemaphoreGive(req_lock);
    if (src) {
        if (os_run_source(src, len, up) == 0) printf("{\"t\":\"stored\",\"id\":\"%s\",\"bytes\":%u}\n", up, (unsigned)len);
        free(src);
    }
    if (play[0] && os_play(play) != 0) printf("{\"t\":\"error\",\"kind\":\"command\",\"msg\":\"no cart named %s\"}\n", play);
    if (del[0]) printf("{\"t\":\"deleted\",\"id\":\"%s\",\"ok\":%d}\n", del, os_delete(del) == 0);
    if (list_req) { list_req = 0; os_list(); }
    if (restart_req) { restart_req = 0; os_restart_game(); }
    if (hash_frames_req) { int n = hash_frames_req; hash_frames_req = 0; os_hash_game(n, hash_seed_req); }
}

// ---------- loop ----------
static void to_os_input(const input_state_t *s, os_input_t *o)
{
    memset(o, 0, sizeof *o);
    for (int i = 0; i < 2; i++) {
        o->in.pos[i] = s->pos[i]; o->in.delta[i] = s->delta[i]; o->in.speed[i] = s->speed[i];
        o->in.down[i] = s->down[i]; o->in.pressed[i] = s->pressed[i];
        o->in.released[i] = s->released[i]; o->in.long_press[i] = s->long_press[i];
    }
    o->chord_pause = s->chord_pause;
    o->chord_exit = s->chord_exit;
}

#define HIST_MS 64

static void wait_until(int64_t t)
{
    while (t - esp_timer_get_time() > 1500) vTaskDelay(1);
    while (esp_timer_get_time() < t) { }
}

static int p99_ms(const uint32_t *hist)
{
    uint32_t total = 0, acc = 0;
    for (int i = 0; i < HIST_MS; i++) total += hist[i];
    for (int i = 0; i < HIST_MS; i++) { acc += hist[i]; if (acc * 100 >= total * 99) return i + 1; }
    return HIST_MS;
}

static int safe_boot;

static void game_task(void *arg)
{
    // Scanning carts runs Lua, so it happens here on the game task's big stack, not in app_main.
    vm_opts_t opts = {
        .prelude = prelude_lua_start, .prelude_len = (size_t)(prelude_lua_end - prelude_lua_start) - 1,
    };
    plat_storage_init();
    os_init(&opts, safe_boot);
    // -1: embedded text carries a trailing NUL
    os_add_cart(brick_lua_start, (size_t)(brick_lua_end - brick_lua_start) - 1, "brick");
    os_add_cart(hello_lua_start, (size_t)(hello_lua_end - hello_lua_start) - 1, "hello");
    if (!safe_boot) os_scan_storage();   // safe mode shows only the built-in carts

    static uint32_t hist[HIST_MS];
    int64_t next = esp_timer_get_time(), window = next;
    int frames = 0, late = 0;
    uint32_t upd_max = 0, draw_max = 0, gc_max = 0;
    os_mode_t prev_mode = os_mode();
    for (;;) {
        handle_requests();
        input_state_t raw;
        input_snapshot(&raw);
        os_input_t in;
        to_os_input(&raw, &in);

        int hashing = os_hash_frames_left() > 0;
        int64_t t0 = esp_timer_get_time();
        os_frame(&in);
        os_mode_t m = os_mode();
        if (m != prev_mode) { memset(hist, 0, sizeof hist); prev_mode = m; }
        if (hashing && m == OS_GAME) {
            uint32_t n = vm_frame_count();
            if (n % 30 == 0 || n == 1 || os_hash_frames_left() == 0)
                printf("{\"t\":\"hash\",\"frame\":%u,\"fnv\":\"%08x\"}\n", (unsigned)n, (unsigned)vm_frame_hash());
        }
        vm_stats_t s;
        vm_get_stats(&s);
        if (m == OS_GAME) {
            if (s.update_us > upd_max) upd_max = s.update_us;
            if (s.draw_us > draw_max) draw_max = s.draw_us;
            if (s.gc_us > gc_max) gc_max = s.gc_us;
        }
        display_present();
        int work_ms = (int)((esp_timer_get_time() - t0) / 1000);
        hist[work_ms < HIST_MS ? work_ms : HIST_MS - 1]++;
        frames++;

        int64_t now = esp_timer_get_time();
        if (now - window >= 1000000) {
            display_stats_t ds;
            display_take_stats(&ds);
            float fps = frames / ((now - window) / 1e6f);
            vm_set_measured_fps(fps);
            static const char *modes[] = { "welcome", "gallery", "card", "settings", "level", "game", "paused", "error" };
            printf("{\"t\":\"stats\",\"mode\":\"%s\",\"cart\":\"%s\",\"fps\":%.1f,\"late\":%d,\"update_us_max\":%u,"
                   "\"draw_us_max\":%u,\"gc_us_max\":%u,\"flush_us_avg\":%u,\"flush_us_max\":%u,\"kb_per_frame\":%.1f,"
                   "\"lua_kb\":%u,\"lua_int_kb\":%u,\"lua_peak_kb\":%u,\"assets_kb\":%u,\"frame_ms_p99\":%d,\"heap_int\":%u}\n",
                   modes[m], os_game_name(), fps, late, (unsigned)upd_max, (unsigned)draw_max, (unsigned)gc_max,
                   ds.flushes ? (unsigned)(ds.flush_us_sum / ds.flushes) : 0, (unsigned)ds.flush_us_max,
                   ds.flushes ? ds.bytes_sum / (float)ds.flushes / 1024.f : 0.f,
                   (unsigned)(s.lua_bytes / 1024), (unsigned)(s.lua_fast_bytes / 1024), (unsigned)(s.lua_peak_bytes / 1024),
                   s.asset_bytes / 1024, p99_ms(hist), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            window = now; frames = 0; late = 0; upd_max = draw_max = gc_max = 0;
        }
        next += 1000000 / os_fps();
        now = esp_timer_get_time();
        if (os_hash_frames_left() > 0) next = now;   // hash runs go as fast as the flush allows
        else if (now > next) { late++; next = now; vTaskDelay(1); }
        else wait_until(next);
    }
}

void game_start(int safe_mode)
{
    req_lock = xSemaphoreCreateMutex();
    safe_boot = safe_mode;
    xTaskCreatePinnedToCore(game_task, "game", 16384, NULL, 5, NULL, 1);
}
