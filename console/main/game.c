// Cart runner on the board: takes carts from the console, runs them through the portable vm
// on core 1 at the cart's frame rate, prints stats once a second, and shows error screens.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "vm.h"
#include "display.h"
#include "input.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern const char prelude_lua_start[] asm("_binary_prelude_lua_start");
extern const char prelude_lua_end[] asm("_binary_prelude_lua_end");

// ---------- cart handoff ----------
static SemaphoreHandle_t cart_lock;
static char *pending_src, *cur_src;
static size_t pending_len, cur_len;
static char pending_name[32], cur_name[32];
static volatile int restart_req;
static volatile int hash_frames_req;   // >0: rerun deterministically and print frame hashes
static volatile uint32_t hash_seed_req;

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

void game_hash(int frames, uint32_t seed)
{
    hash_seed_req = seed;
    hash_frames_req = frames;
    restart_req = 1;
}

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

// ---------- running ----------
static void to_vm_input(const input_state_t *s, vm_input_t *o)
{
    for (int i = 0; i < 2; i++) {
        o->pos[i] = s->pos[i]; o->delta[i] = s->delta[i]; o->speed[i] = s->speed[i];
        o->down[i] = s->down[i]; o->pressed[i] = s->pressed[i];
        o->released[i] = s->released[i]; o->long_press[i] = s->long_press[i];
    }
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

// Runs the current cart until it errors, is stopped, or another cart or a restart arrives.
static void run_cart(void)
{
    int hash_frames = hash_frames_req;
    hash_frames_req = 0;
    vm_opts_t opts = {
        .prelude = prelude_lua_start, .prelude_len = (size_t)(prelude_lua_end - prelude_lua_start) - 1,
        .deterministic = hash_frames > 0, .seed = hash_seed_req,
    };
    input_state_t raw;
    input_snapshot(&raw);   // drop events from before the cart started
    if (vm_start(cur_src, cur_len, cur_name, &opts) != 0) { vm_stop(); return; }

    static uint32_t hist[HIST_MS];
    memset(hist, 0, sizeof hist);
    int64_t period = 1000000 / vm_fps(), next = esp_timer_get_time(), window = next;
    int frames = 0, late = 0;
    uint32_t upd_max = 0, draw_max = 0, gc_max = 0;
    vm_input_t in;
    while (!has_pending() && !restart_req) {
        input_snapshot(&raw);
        if (raw.chord_exit) { vm_stopped_by_user(); break; }
        to_vm_input(&raw, &in);
        if (hash_frames) memset(&in, 0, sizeof in);   // hash runs use no input, like the simulator
        int64_t t0 = esp_timer_get_time();
        if (vm_frame(&in) != 0) break;
        vm_stats_t s;
        vm_get_stats(&s);
        if (s.update_us > upd_max) upd_max = s.update_us;
        if (s.draw_us > draw_max) draw_max = s.draw_us;
        if (s.gc_us > gc_max) gc_max = s.gc_us;
        uint32_t n = vm_frame_count();
        if (hash_frames && (n % 30 == 0 || n == 1 || n == (uint32_t)hash_frames)) {
            printf("{\"t\":\"hash\",\"frame\":%u,\"fnv\":\"%08x\"}\n", (unsigned)n, (unsigned)vm_frame_hash());
            if (n == (uint32_t)hash_frames) hash_frames = 0;   // done; keep running normally
        }
        display_present();
        int work_ms = (int)((esp_timer_get_time() - t0) / 1000);   // game work plus waiting for the flush
        hist[work_ms < HIST_MS ? work_ms : HIST_MS - 1]++;
        frames++;

        int64_t now = esp_timer_get_time();
        if (now - window >= 1000000) {
            display_stats_t ds;
            display_take_stats(&ds);
            float fps = frames / ((now - window) / 1e6f);
            vm_set_measured_fps(fps);
            printf("{\"t\":\"stats\",\"cart\":\"%s\",\"fps\":%.1f,\"late\":%d,\"update_us_max\":%u,\"draw_us_max\":%u,"
                   "\"gc_us_max\":%u,\"flush_us_avg\":%u,\"flush_us_max\":%u,\"kb_per_frame\":%.1f,\"lua_kb\":%u,"
                   "\"lua_int_kb\":%u,\"lua_peak_kb\":%u,\"assets_kb\":%u,\"frame_ms_p99\":%d,\"heap_int\":%u}\n",
                   cur_name, fps, late, (unsigned)upd_max, (unsigned)draw_max, (unsigned)gc_max,
                   ds.flushes ? (unsigned)(ds.flush_us_sum / ds.flushes) : 0, (unsigned)ds.flush_us_max,
                   ds.flushes ? ds.bytes_sum / (float)ds.flushes / 1024.f : 0.f,
                   (unsigned)(s.lua_bytes / 1024), (unsigned)(s.lua_fast_bytes / 1024), (unsigned)(s.lua_peak_bytes / 1024),
                   s.asset_bytes / 1024, p99_ms(hist), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            window = now; frames = 0; late = 0; upd_max = draw_max = gc_max = 0;
        }
        next += period;
        now = esp_timer_get_time();
        if (hash_frames) next = now;   // hash runs go as fast as the flush allows
        else if (now > next) { late++; next = now; vTaskDelay(1); }
        else wait_until(next);
    }
    vm_stop();
}

static void game_task(void *arg)
{
    for (;;) {
        while (!take_pending() && !cur_src) vTaskDelay(pdMS_TO_TICKS(20));
        restart_req = 0;
        run_cart();
        if (vm_error() == VM_ERR_NONE) continue;   // replaced by a new cart or restarted
        vm_draw_error_screen();
        display_present();
        for (;;) {   // wait for a restart click, a restart command or a new cart
            vTaskDelay(pdMS_TO_TICKS(33));
            input_state_t raw;
            input_snapshot(&raw);
            if (raw.pressed[KNOB_RIGHT] || restart_req || has_pending()) break;
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
