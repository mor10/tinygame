// tinygame-sim: runs a cart headless with the console's own engine and prints the same JSON lines.
//
//   tinygame-sim CART.lua [--frames N] [--seed S] [--png FILE] [--scale K] [--press SIDE@FRAME ...]
//                         [--turn SIDE@FRAME:NOTCHES ...]
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

extern const char prelude_lua[];
extern const unsigned prelude_lua_len;

#define MAX_EVENTS 256
typedef struct { int frame, side, kind, notches; } event_t;   // kind 0 = press, 1 = turn, 2 = chord
static event_t events[MAX_EVENTS];
static int nevents;

static int usage(void)
{
    fprintf(stderr, "usage: tinygame-sim CART.lua [--frames N] [--seed S] [--png FILE] [--scale K]\n"
                    "                    [--press left|right@FRAME] [--turn left|right@FRAME:NOTCHES]\n"
                    "       tinygame-sim --shell CART.lua... [same options] [--chord pause|exit@FRAME]\n");
    return 2;
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

// --shell: the OS shell with the given carts in the gallery, driven by the scripted input.
static int run_shell(const char **paths, int n, int frames, const char *png, int scale)
{
    if (frames < 1 || scale < 1 || scale > 8) return usage();
    gfx_init_palette();
    vm_opts_t opts = { .prelude = prelude_lua, .prelude_len = prelude_lua_len };
    os_init(&opts, 0);
    for (int i = 0; i < n; i++) {
        size_t len;
        char *src = read_file(paths[i], &len);   // kept for the whole run; the shell points into it
        if (!src) { fprintf(stderr, "can't read %s\n", paths[i]); return 2; }
        const char *name = strrchr(paths[i], '/') ? strrchr(paths[i], '/') + 1 : paths[i];
        os_add_cart(src, len, name);
    }
    static const char *modes[] = { "welcome", "gallery", "card", "settings", "level", "game", "paused", "error" };
    os_mode_t last = os_mode();
    for (int f = 1; f <= frames; f++) {
        os_input_t in;
        memset(&in, 0, sizeof in);
        for (int e = 0; e < nevents; e++) {
            if (events[e].frame != f) continue;
            if (events[e].kind == 0) { in.in.pressed[events[e].side] = 1; in.in.down[events[e].side] = 1; }
            else if (events[e].kind == 1) in.in.delta[events[e].side] += events[e].notches * 4;
            else if (events[e].side == 0) in.chord_pause = 1;
            else in.chord_exit = 1;
        }
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
    int shell = 0, nshell = 0;
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
        else if (!strcmp(a, "--chord") && more) { if (parse_chord(argv[++i])) return usage(); }
        else if (a[0] != '-' && shell && nshell < 32) shell_carts[nshell++] = a;
        else if (a[0] != '-' && !cart) cart = a;
        else return usage();
    }
    if (shell) return run_shell(shell_carts, nshell, frames, png, scale);
    if (!cart || frames < 1 || scale < 1 || scale > 8) return usage();

    size_t len;
    char *src = read_file(cart, &len);
    if (!src) { fprintf(stderr, "can't read %s\n", cart); return 2; }
    const char *name = strrchr(cart, '/') ? strrchr(cart, '/') + 1 : cart;

    gfx_init_palette();
    vm_opts_t opts = { .prelude = prelude_lua, .prelude_len = prelude_lua_len, .deterministic = 1, .seed = (uint32_t)seed };
    int ok = vm_start(src, len, name, &opts) == 0;
    int n = 0;
    while (ok && n < frames) {
        vm_input_t in;
        memset(&in, 0, sizeof in);
        for (int e = 0; e < nevents; e++) {
            if (events[e].frame != n + 1) continue;
            if (events[e].kind == 0) in.pressed[events[e].side] = 1;
            else in.delta[events[e].side] += events[e].notches * 4;
        }
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
