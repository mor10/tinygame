// tinygame-sim: runs a cart headless with the console's own engine and prints the same JSON lines.
//
//   tinygame-sim CART.lua [--frames N] [--seed S] [--png FILE] [--scale K] [--press SIDE@FRAME ...]
//                         [--turn SIDE@FRAME:NOTCHES ...] [--storage DIR] [--runs R]
//   tinygame-sim --shell CART.lua... [same options] [--chord pause|exit@FRAME]
//   tinygame-sim --check CART.lua [--frames N] [--png FILE]
//
// --check lints the source (other consoles' words, other languages' operators), checks the cart
// table (title, tagline, 16x16 icon), then runs the cart N frames (default 300) twice in strict mode
// (names from other consoles, like btn or spr, are errors): once with no input, once with busy knob
// and tilt input. It prints
// {"t":"check"} lines and a final {"t":"check_done","ok":...}; exit status 0 = passed, 1 = errors.
//
// --storage DIR keeps carts and saves in DIR/carts and DIR/saves (otherwise saves last one run).
// --runs R plays the cart R times in a row (only the last prints hashes) to test saves across runs.
// --tilt X,Y@FRAME holds the tilt at X,Y (each -1..1) from that frame on.
//
// Runs are deterministic: math.random is seeded with S (default 0), sys.time() follows the frame
// count, and input comes only from --press/--turn. Frame hashes (FNV-1a of the 8-bit framebuffer)
// are printed for frame 1, every 30th frame and the last frame, exactly as the board's
// `hash <n> [seed]` command prints them, so the two can be compared line by line.
// Exit status: 0 if the cart ran all frames, 1 if it stopped with an error, 2 for usage problems.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "gfx.h"
#include "sim.h"
#include "os.h"
#include "lint.h"
#include <math.h>

extern const char *sim_storage_dir;
extern const char prelude_lua[];
extern const unsigned prelude_lua_len;

#define MAX_EVENTS 256
typedef struct { int frame, side, kind, notches; float tx, ty; } event_t;   // kind 0 press, 1 turn, 2 chord, 3 tilt
static event_t events[MAX_EVENTS];
static int nevents;

static int usage(void)
{
    fprintf(stderr, "usage: tinygame-sim CART.lua [--frames N] [--seed S] [--png FILE] [--scale K]\n"
                    "                    [--press left|right@FRAME] [--turn left|right@FRAME:NOTCHES]\n"
                    "       tinygame-sim --shell CART.lua... [same options] [--chord pause|exit@FRAME]\n"
                    "       tinygame-sim --check CART.lua [--frames N] [--png FILE]\n"
                    "       options: [--storage DIR] [--runs R] [--tilt X,Y@FRAME]\n");
    return 2;
}

static int parse_tilt(const char *arg)
{
    float x, y;
    int frame;
    if (sscanf(arg, "%f,%f@%d", &x, &y, &frame) != 3 || frame < 1 || nevents == MAX_EVENTS) return -1;
    events[nevents++] = (event_t){ frame, 0, 3, 0, x, y };
    return 0;
}

static float tilt_now[2];   // tilt holds from its --tilt frame until the next one

static void apply_events(int frame, vm_input_t *in, uint8_t *chord_pause, uint8_t *chord_exit)
{
    for (int e = 0; e < nevents; e++) {
        if (events[e].frame != frame) continue;
        if (events[e].kind == 0) { in->pressed[events[e].side] = 1; in->down[events[e].side] = 1; }
        else if (events[e].kind == 1) in->delta[events[e].side] += events[e].notches * 4;
        else if (events[e].kind == 2) { if (events[e].side == 0) *chord_pause = 1; else *chord_exit = 1; }
        else { tilt_now[0] = events[e].tx; tilt_now[1] = events[e].ty; }
    }
    in->tilt[0] = tilt_now[0];
    in->tilt[1] = tilt_now[1];
    static int32_t pos[2];   // knob.pos is the running total of every turn, as on the board
    static float speed[2];   // knob.speed: transitions per second, smoothed like the board's driver
    for (int i = 0; i < 2; i++) {
        pos[i] += in->delta[i]; in->pos[i] = pos[i];
        speed[i] += (in->delta[i] * 30.f - speed[i]) * 0.3f; in->speed[i] = speed[i];
    }
}

static int parse_chord(const char *arg)
{
    char what[8];
    int frame = 0;
    if (sscanf(arg, "%7[a-z]@%d", what, &frame) != 2 || frame < 1 || nevents == MAX_EVENTS) return -1;
    int s = !strcmp(what, "pause") ? 0 : !strcmp(what, "exit") ? 1 : -1;
    if (s < 0) return -1;
    events[nevents++] = (event_t){ frame, s, 2, 0 };
    return 0;
}

static int parse_event(const char *arg, int kind)
{
    char side[8];
    int frame = 0, notches = 0;
    int n = kind ? sscanf(arg, "%7[a-z]@%d:%d", side, &frame, &notches) : sscanf(arg, "%7[a-z]@%d", side, &frame);
    if (n != (kind ? 3 : 2) || frame < 1 || nevents == MAX_EVENTS) return -1;
    int s = !strcmp(side, "left") ? VM_LEFT : !strcmp(side, "right") ? VM_RIGHT : -1;
    if (s < 0) return -1;
    events[nevents++] = (event_t){ frame, s, kind, notches };
    return 0;
}

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    if (buf && fread(buf, 1, n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) { buf[n] = 0; *len = (size_t)n; }
    return buf;
}

// --check: static lint, the cart table, then two strict runs (no input, then busy input).
static int check_errors, check_warnings;

static void check_report(void *ctx, int level, int line, const char *msg)
{
    (void)ctx;
    if (level) check_errors++; else check_warnings++;
    printf("{\"t\":\"check\",\"level\":\"%s\",\"line\":%d,\"msg\":\"", level ? "error" : "warning", line);
    for (const char *p = msg; *p; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p == '\n' ? ' ' : *p);
    }
    printf("\"}\n");
}

static int smoke(const char *src, size_t len, const char *name, int frames, int busy)
{
    vm_opts_t o = { .prelude = prelude_lua, .prelude_len = prelude_lua_len, .deterministic = 1, .seed = busy ? 99 : 0,
                    .save_id = NULL, .strict = 1 };
    int ok = vm_start(src, len, name, &o) == 0;
    int32_t pos[2] = { 0, 0 };
    float speed[2] = { 0, 0 };
    for (int f = 1; ok && f <= frames; f++) {
        vm_input_t in;
        memset(&in, 0, sizeof in);
        if (busy) {   // knobs turning both ways, clicks, holds and a slow tilt circle
            in.delta[0] = (f % 40 < 20 ? 4 : -4) * (f % 3 == 0);
            in.delta[1] = (f % 50 < 25 ? -4 : 4) * (f % 4 == 0);
            in.pressed[f % 2] = f % 15 == 0;
            in.down[0] = in.down[1] = (f % 60) < 10;
            in.released[f % 2] = f % 15 == 5;
            in.long_press[1] = f % 90 == 45;
            in.tilt[0] = (float)sin(f / 20.0);
            in.tilt[1] = (float)cos(f / 25.0);
            in.shake = f % 120 == 60;
            for (int i = 0; i < 2; i++) {
                pos[i] += in.delta[i]; in.pos[i] = pos[i];
                speed[i] += (in.delta[i] * 30.f - speed[i]) * 0.3f; in.speed[i] = speed[i];
            }
        }
        ok = vm_frame(&in) == 0;
    }
    if (!ok) {
        int line;
        const char *msg = vm_error_message(&line);
        char m[256];
        snprintf(m, sizeof m, "%s while running%s: %s", vm_error() == VM_ERR_STUCK ? "Stuck" :
                 vm_error() == VM_ERR_MEMORY ? "Out of memory" : "Error", busy ? " with knob and tilt input" : "", msg);
        check_report(NULL, 1, line, m);
        vm_draw_error_screen();
    }
    vm_stop();
    return ok;
}

static int run_check(const char *path, int frames, const char *png, int scale)
{
    size_t len;
    char *src = read_file(path, &len);
    if (!src) { fprintf(stderr, "can't read %s\n", path); return 2; }
    const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    gfx_init_palette();
    plat_storage_init();
    if (len > 128 * 1024) check_report(NULL, 1, -1, "The cart is bigger than 128 KB");
    lint_source(src, len, check_report, NULL);

    vm_opts_t o = { .prelude = prelude_lua, .prelude_len = prelude_lua_len };
    vm_meta_t *m = calloc(1, sizeof *m);
    if (vm_scan(src, len, &o, m) == 0) {
        if (!m->title[0]) check_report(NULL, 0, -1, "cart.title is missing: the gallery will show the file name");
        else if (strlen(m->title) > 23) check_report(NULL, 0, -1, "cart.title is longer than 23 characters and will be cut off");
        if (!m->tagline[0]) check_report(NULL, 0, -1, "cart.tagline is missing: the game card will have no description");
        lint_icon(m->icon, check_report, NULL);
    }
    free(m);

    int ok = smoke(src, len, name, frames, 0);   // quiet run first: its last frame is the screenshot
    if (png) png_write(png, sim_fb, sim_palette, SCREEN_W, SCREEN_H, scale);
    if (ok) smoke(src, len, name, frames, 1);
    printf("{\"t\":\"check_done\",\"ok\":%d,\"errors\":%d,\"warnings\":%d,\"frames\":%d}\n",
           check_errors == 0, check_errors, check_warnings, frames);
    free(src);
    return check_errors ? 1 : 0;
}

// --shell: the OS shell with the given carts in the gallery, driven by the scripted input.
static int run_shell(const char **paths, int n, int frames, const char *png, int scale)
{
    if (frames < 1 || scale < 1 || scale > 8) return usage();
    gfx_init_palette();
    vm_opts_t opts = { .prelude = prelude_lua, .prelude_len = prelude_lua_len };
    plat_storage_init();
    os_init(&opts, 0);
    for (int i = 0; i < n; i++) {
        size_t len;
        char *src = read_file(paths[i], &len);   // kept for the whole run; the shell points into it
        if (!src) { fprintf(stderr, "can't read %s\n", paths[i]); return 2; }
        char id[32];
        const char *base = strrchr(paths[i], '/') ? strrchr(paths[i], '/') + 1 : paths[i];
        snprintf(id, sizeof id, "%.*s", (int)strcspn(base, "."), base);
        os_add_cart(src, len, id);
    }
    os_scan_storage();
    static const char *modes[] = { "welcome", "gallery", "card", "settings", "level", "game", "paused", "error", "colourcard", "diagnostics" };
    os_mode_t last = os_mode();
    for (int f = 1; f <= frames; f++) {
        os_input_t in;
        memset(&in, 0, sizeof in);
        apply_events(f, &in.in, &in.chord_pause, &in.chord_exit);
        os_frame(&in);
        if (os_mode() != last) {
            last = os_mode();
            printf("{\"t\":\"mode\",\"frame\":%d,\"mode\":\"%s\",\"game\":\"%s\"}\n", f, modes[last], os_game_name());
        }
    }
    if (png && png_write(png, sim_fb, sim_palette, SCREEN_W, SCREEN_H, scale) != 0) {
        fprintf(stderr, "can't write %s\n", png);
        return 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *cart = NULL, *png = NULL;
    const char *shell_carts[32];
    int shell = 0, nshell = 0, runs = 1, checking = 0;
    int frames = 300, scale = 2;
    unsigned long seed = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int more = i + 1 < argc;
        if (!strcmp(a, "--frames") && more) frames = atoi(argv[++i]);
        else if (!strcmp(a, "--seed") && more) seed = strtoul(argv[++i], NULL, 10);
        else if (!strcmp(a, "--png") && more) png = argv[++i];
        else if (!strcmp(a, "--scale") && more) scale = atoi(argv[++i]);
        else if (!strcmp(a, "--press") && more) { if (parse_event(argv[++i], 0)) return usage(); }
        else if (!strcmp(a, "--turn") && more) { if (parse_event(argv[++i], 1)) return usage(); }
        else if (!strcmp(a, "--shell")) shell = 1;
        else if (!strcmp(a, "--left-handed")) vm_set_left_handed(1);
        else if (!strcmp(a, "--check")) checking = 1;
        else if (!strcmp(a, "--storage") && more) sim_storage_dir = argv[++i];
        else if (!strcmp(a, "--runs") && more) runs = atoi(argv[++i]);
        else if (!strcmp(a, "--chord") && more) { if (parse_chord(argv[++i])) return usage(); }
        else if (!strcmp(a, "--tilt") && more) { if (parse_tilt(argv[++i])) return usage(); }
        else if (a[0] != '-' && shell && nshell < 32) shell_carts[nshell++] = a;
        else if (a[0] != '-' && !cart) cart = a;
        else return usage();
    }
    if (shell) return run_shell(shell_carts, nshell, frames, png, scale);
    if (checking) { if (!cart) return usage(); return run_check(cart, frames, png, scale); }
    if (!cart || frames < 1 || scale < 1 || scale > 8) return usage();

    size_t len;
    char *src = read_file(cart, &len);
    if (!src) { fprintf(stderr, "can't read %s\n", cart); return 2; }
    const char *name = strrchr(cart, '/') ? strrchr(cart, '/') + 1 : cart;

    gfx_init_palette();
    plat_storage_init();
    char save_id[32];
    snprintf(save_id, sizeof save_id, "%.*s", (int)strcspn(name, "."), name);
    vm_opts_t opts = { .prelude = prelude_lua, .prelude_len = prelude_lua_len, .deterministic = 1, .seed = (uint32_t)seed,
                       .save_id = save_id };
    for (int r = 1; r < runs; r++) {   // earlier runs: play all frames and stop, so saves carry over
        if (vm_start(src, len, name, &opts) == 0) {
            vm_input_t none;
            memset(&none, 0, sizeof none);
            for (int f = 0; f < frames && vm_frame(&none) == 0; f++) { }
        }
        vm_stop();
    }
    int ok = vm_start(src, len, name, &opts) == 0;
    int n = 0;
    while (ok && n < frames) {
        vm_input_t in;
        uint8_t cp = 0, ce = 0;
        memset(&in, 0, sizeof in);
        apply_events(n + 1, &in, &cp, &ce);
        ok = vm_frame(&in) == 0;
        if (!ok) break;
        n = (int)vm_frame_count();
        if (n % 30 == 0 || n == 1 || n == frames) printf("{\"t\":\"hash\",\"frame\":%d,\"fnv\":\"%08x\"}\n", n, vm_frame_hash());
    }
    if (!ok) vm_draw_error_screen();
    vm_stats_t st;
    vm_get_stats(&st);
    printf("{\"t\":\"done\",\"frames\":%d,\"ok\":%d,\"lua_peak_kb\":%u}\n", n, ok, (unsigned)(st.lua_peak_bytes / 1024));
    vm_stop();
    if (png && png_write(png, sim_fb, sim_palette, SCREEN_W, SCREEN_H, scale) != 0) {
        fprintf(stderr, "can't write %s\n", png);
        return 2;
    }
    free(src);
    return ok ? 0 : 1;
}
