// OS shell: the screens from docs/ref-designs (Welcome, Cartridges, Game Card, Settings, Paused)
// and the game/error states around the vm. Layout follows console/carts/system/launcher_mock.lua,
// the reviewed mock.
#include <stdio.h>
#include <string.h>
#include "os.h"
#include "gfx.h"
#include "platform.h"

#define MAX_CARTS 32
#define ICON_MAX  16

typedef struct {
    char *src;              // owned copy (uploaded carts) or NULL when `csrc` is static
    const char *csrc;
    size_t len;
    char title[24], tagline[48];
    uint8_t icon[ICON_MAX * ICON_MAX];
    int icon_w, icon_h;     // 0 when the cart has no usable icon
    int uploaded;
} entry_t;

static entry_t carts[MAX_CARTS];
static int ncarts;
static vm_opts_t base;
static os_settings_t set = { 6, 8, 0, 1 };
static int safe;

static os_mode_t mode = OS_WELCOME;
static int focus = 1;                 // focused item on the current screen
static int pick;                      // gallery selection; -1 is the "<" back item
static int last_game = -1;            // index of the game playing or last played
static int settings_row = 1;
static os_mode_t settings_from = OS_WELCOME;
static int level_kind;                // 1 = sound, 2 = light
static int notch_acc;
static uint8_t *frozen;               // the game frame behind the pause menu
static int hash_left;

static const char *SETTINGS_ROWS[] = { "SOUND", "LIGHT", "HANDS", "BACK" };
#define NSETTINGS 4

// ---------- carts ----------
static int hexval(char c)
{
    if (c == '.') return GFX_CLEAR;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void decode_icon(entry_t *e, const char *art)   // same text-art rules as sprites, at most 16x16
{
    int w = -1, h = 0;
    e->icon_w = e->icon_h = 0;
    while (*art) {
        const char *end = strchr(art, '\n');
        if (!end) end = art + strlen(art);
        const char *a = art, *b = end;
        while (a < b && (*a == ' ' || *a == '\t' || *a == '\r')) a++;
        while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r')) b--;
        if (b > a) {
            int len = (int)(b - a);
            if (w < 0) w = len;
            if (len != w || w > ICON_MAX || h >= ICON_MAX) return;
            for (int x = 0; x < w; x++) {
                int v = hexval(a[x]);
                if (v < 0) return;
                e->icon[h * w + x] = (uint8_t)v;
            }
            h++;
        }
        art = *end ? end + 1 : end;
    }
    if (w > 0 && h > 0) { e->icon_w = w; e->icon_h = h; }
}

static void scan(entry_t *e, const char *fallback)
{
    vm_meta_t *m = plat_asset_alloc(sizeof *m);   // too big for a task stack
    if (m && vm_scan(e->csrc, e->len, &base, m) == 0 && m->title[0]) {
        snprintf(e->title, sizeof e->title, "%s", m->title);
        snprintf(e->tagline, sizeof e->tagline, "%s", m->tagline);
        decode_icon(e, m->icon);
    } else {
        snprintf(e->title, sizeof e->title, "%s", fallback);
        e->tagline[0] = 0;
        e->icon_w = e->icon_h = 0;
    }
    plat_asset_free(m);
}

int os_add_cart(const char *src, size_t len, const char *fallback_name)
{
    if (ncarts == MAX_CARTS) return -1;
    entry_t *e = &carts[ncarts];
    memset(e, 0, sizeof *e);
    e->csrc = src;
    e->len = len;
    scan(e, fallback_name);
    printf("{\"t\":\"shell\",\"cart\":%d,\"title\":\"%s\",\"icon\":%d}\n", ncarts, e->title, e->icon_w > 0);
    return ncarts++;
}

// ---------- settings ----------
static void apply_settings(void) { plat_backlight(set.light * 10); }
static void save_settings(void) { if (!safe) plat_settings_save(&set, sizeof set); }

void os_init(const vm_opts_t *opts, int safe_mode)
{
    base = *opts;
    base.deterministic = 0;
    safe = safe_mode;
    os_settings_t s;
    if (!safe && plat_settings_load(&s, sizeof s) == 0 && s.version == 1 && s.sound <= 10
        && s.light >= 1 && s.light <= 10 && s.left_handed <= 1) set = s;
    if (!frozen) frozen = plat_asset_alloc(SCREEN_W * SCREEN_H);
    apply_settings();
}

// ---------- drawing helpers ----------
static void box(int x, int y, int w, int h, int c)   // corners clipped by one pixel
{
    gfx_fill_rect(x + 1, y, w - 2, h, c);
    gfx_fill_rect(x, y + 1, w, h - 2, c);
}

static void button(int x, int y, int w, int h, int c, int shadow)
{
    box(x, y + 3, w, h, shadow);
    box(x, y, w, h, c);
}

static void centre_text(const char *s, int cx, int y, int c, int big)
{
    gfx_text(s, cx - gfx_text_width(s, big ? 2 : 1) / 2, y, c, big ? 2 : 1);
}

static void logo(int x, int y, int s)
{
    gfx_fill_rect(x + s / 2, y, s, s, OS_YELLOW);
    gfx_fill_rect(x, y + s, s, s, OS_TEAL);
    gfx_fill_rect(x + s, y + s, s, s, OS_CORAL);
}

static void battery(int x, int y)
{
    gfx_rect(x, y, 18, 9, OS_NAVY);
    gfx_fill_rect(x + 18, y + 3, 2, 3, OS_NAVY);
    for (int i = 0; i < 3; i++) gfx_fill_rect(x + 2 + i * 5, y + 2, 4, 5, OS_NAVY);
}

static void knob_glyph(int x, int y, int c)
{
    static const char *g[7] = { "..###..", ".#...#.", "#..#..#", "#..#..#", "#.....#", ".#...#.", "..###.." };
    for (int r = 0; r < 7; r++)
        for (int k = 0; k < 7; k++)
            if (g[r][k] == '#') gfx_pixel(x + k, y + r, c);
}

static void footer(const char *scroll_label, const char *select_label, int dark)
{
    int y = SCREEN_H - 16, ink = dark ? OS_CREAM : OS_NAVY;
    const char *l = set.left_handed ? scroll_label : select_label;
    const char *r = set.left_handed ? select_label : scroll_label;
    knob_glyph(92, y, ink);
    gfx_text(l, 103, y, ink, 1);
    knob_glyph(182, y, ink);
    gfx_text(r, 193, y, ink, 1);
}

static void back_arrow(int focused)
{
    if (focused) button(6, 4, 22, 20, OS_CORAL, OS_RED);
    gfx_text("<", 12, 8, focused ? OS_WHITE : OS_NAVY, 2);
}

static void draw_icon(const entry_t *e, int cx, int cy, int scale, int flip)
{
    if (e->icon_w)
        gfx_blit(e->icon, e->icon_w, e->icon_h, cx - e->icon_w * scale / 2, cy - e->icon_h * scale / 2, flip, 0, scale);
    else {   // no icon: the title's first letter
        char s[2] = { e->title[0] ? e->title[0] : '?', 0 };
        centre_text(s, cx, cy - 7, OS_NAVY, 1);
    }
}

static void upper(char *out, size_t n, const char *s)
{
    size_t i = 0;
    for (; s[i] && i + 1 < n; i++) out[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];
    out[i] = 0;
}

// ---------- screens ----------
static void draw_welcome(void)
{
    gfx_clear(OS_CREAM);
    logo(10, 6, 5);
    gfx_text("TinyGame", 30, 6, OS_NAVY, 2);
    battery(SCREEN_W - 30, 8);
    logo(62, 52, 16);
    button(146, 50, 124, 30, focus == 1 ? OS_CORAL : OS_GREY, focus == 1 ? OS_RED : OS_DGREY);
    centre_text("START", 208, 58, OS_WHITE, 1);
    const char *items[] = { NULL, NULL, "Resume", "Settings" };
    for (int i = 2; i <= 3; i++) {
        int y = 92 + (i - 2) * 14;
        int c = focus == i ? OS_CORAL : (i == 2 && last_game < 0) ? OS_GREY : OS_NAVY;
        if (focus == i) gfx_text(">", 172, y, c, 1);
        gfx_text(items[i], 182, y, c, 1);
    }
    if (safe) gfx_text("SAFE MODE", 10, 132, OS_CORAL, 1);
    footer("Browse", "Select", 0);
}

static int tile_colour(int i) { static const int c[] = { OS_TEAL, OS_YELLOW, OS_CORAL }; return c[i % 3]; }

static void draw_gallery(void)
{
    gfx_clear(OS_CREAM);
    back_arrow(pick < 0);
    gfx_text("GAMES", 34, 8, OS_NAVY, 2);
    if (ncarts == 0) { centre_text("No games yet", SCREEN_W / 2, 80, OS_NAVY, 1); footer("Browse", "Select", 0); return; }
    int p = pick < 0 ? 0 : pick;
    int first = p - 2;
    if (first > ncarts - 5) first = ncarts - 5;
    if (first < 0) first = 0;
    for (int i = first; i < ncarts && i < first + 5; i++) {
        int x = 22 + (i - first) * 58, sel = i == pick;
        int size = sel ? 48 : 40, y = sel ? 40 : 46;
        if (sel) x -= 4;
        button(x, y, size, size, sel ? OS_CORAL : tile_colour(i), OS_NAVY);
        box(x + 4, y + 4, size - 8, size - 8, OS_CREAM);
        draw_icon(&carts[i], x + size / 2, y + size / 2, 2, 0);
    }
    char name[32];
    if (pick < 0) {
        centre_text("BACK", SCREEN_W / 2, 110, OS_NAVY, 1);
    } else {
        int sx = 22 + (pick - first) * 58 + 20;
        for (int k = 0; k < 4; k++) gfx_fill_rect(sx - k, 101 + k, 1 + 2 * k, 1, OS_NAVY);
        upper(name, sizeof name, carts[pick].title);
        centre_text(name, SCREEN_W / 2, 110, OS_NAVY, gfx_text_width(name, 2) <= SCREEN_W - 16);
        char n[16];
        snprintf(n, sizeof n, "%d / %d", pick + 1, ncarts);
        centre_text(n, SCREEN_W / 2, 128, OS_NAVY, 0);
    }
    footer("Browse", "Select", 0);
}

static void draw_card(void)
{
    const entry_t *e = &carts[pick];
    gfx_clear(OS_CREAM);
    back_arrow(focus == 0);
    char n[16];
    snprintf(n, sizeof n, "%d / %d", pick + 1, ncarts);
    gfx_text(n, SCREEN_W - 40, 10, OS_NAVY, 1);
    gfx_fill_rect(30, 112, 100, 2, OS_MINT);
    draw_icon(e, 64, 62, 2, 0);
    draw_icon(e, 108, 86, 2, 1);
    char name[32];
    upper(name, sizeof name, e->title);
    int big = gfx_text_width(name, 2) <= 150;
    centre_text(name, 228, big ? 40 : 44, OS_NAVY, big);
    // Tagline: one line, or two split at the space nearest the middle.
    int len = (int)strlen(e->tagline);
    if (gfx_text_width(e->tagline, 1) <= 150) centre_text(e->tagline, 228, 62, OS_NAVY, 0);
    else {
        int cut = len / 2, best = -1;
        for (int d = 0; d < len / 2 && best < 0; d++) {
            if (cut + d < len && e->tagline[cut + d] == ' ') best = cut + d;
            else if (cut - d > 0 && e->tagline[cut - d] == ' ') best = cut - d;
        }
        if (best < 0) best = cut;
        char a[48], b[48];
        snprintf(a, sizeof a, "%.*s", best, e->tagline);
        snprintf(b, sizeof b, "%s", e->tagline + best + (e->tagline[best] == ' '));
        centre_text(a, 228, 56, OS_NAVY, 0);
        centre_text(b, 228, 66, OS_NAVY, 0);
    }
    button(176, 82, 104, 26, focus == 1 ? OS_CORAL : OS_GREY, focus == 1 ? OS_RED : OS_DGREY);
    centre_text("PLAY", 228, 88, OS_WHITE, 1);
    footer("Browse", "Select", 0);
}

static void bars(int x, int y, int level)
{
    for (int i = 1; i <= 5; i++) gfx_fill_rect(x + (i - 1) * 9, y, 7, 7, i <= (level + 1) / 2 ? OS_TEAL : OS_MINT);
}

static void draw_settings(void)
{
    gfx_clear(OS_CREAM);
    gfx_text("SETTINGS", 12, 10, OS_NAVY, 2);
    for (int i = 1; i <= NSETTINGS; i++) {
        int y = 34 + (i - 1) * 22, on = settings_row == i;
        if (on) gfx_text(">", 14, y + 3, OS_CORAL, 1);
        gfx_text(SETTINGS_ROWS[i - 1], 26, y, on ? OS_CORAL : OS_NAVY, 2);
        if (i == 1) bars(240, y + 4, set.sound);
        else if (i == 2) bars(240, y + 4, set.light);
        else if (i == 3) gfx_text(set.left_handed ? "LEFT" : "RIGHT", 240, y, OS_NAVY, 2);
    }
    footer("Browse", settings_row == 3 ? "Switch" : "Edit", 0);
}

static void draw_level(void)
{
    int v = level_kind == 1 ? set.sound : set.light;
    gfx_clear(OS_CREAM);
    gfx_text(level_kind == 1 ? "SOUND" : "LIGHT", 12, 10, OS_NAVY, 2);
    char pct[8];
    snprintf(pct, sizeof pct, "%d%%", v * 10);
    centre_text(pct, SCREEN_W / 2, 46, OS_NAVY, 1);
    for (int i = 1; i <= 10; i++) gfx_fill_rect(70 + (i - 1) * 18, 76, 14, 14, i <= v ? OS_TEAL : OS_MINT);
    centre_text(set.left_handed ? "TURN THE LEFT KNOB TO ADJUST" : "TURN THE RIGHT KNOB TO ADJUST", SCREEN_W / 2, 104, OS_GREY, 0);
    if (level_kind == 1) centre_text("(no speaker fitted yet)", SCREEN_W / 2, 118, OS_GREY, 0);
    footer("Level", "Done", 0);
}

static void draw_paused(void)
{
    uint8_t *fb = plat_framebuffer();
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = (uint8_t)(OS_DIM + (frozen[i] & 15));
    box(84, 20, 152, 112, OS_CREAM);
    gfx_text("PAUSED", 98, 30, OS_NAVY, 2);
    static const char *items[] = { "CONTINUE", "SETTINGS", "EXIT GAME" };
    for (int i = 1; i <= 3; i++) {
        int y = 54 + (i - 1) * 24;
        if (focus == i) { button(106, y - 4, 118, 20, OS_CORAL, OS_RED); gfx_text(">", 96, y + 1, OS_NAVY, 1); }
        gfx_text(items[i - 1], 112, y, focus == i ? OS_WHITE : OS_NAVY, 2);
    }
    gfx_fill_rect(0, SCREEN_H - 18, SCREEN_W, 18, OS_NAVY);
    footer("Browse", "Select", 1);
}

// ---------- flow ----------
static void go(os_mode_t m, int f) { mode = m; focus = f; notch_acc = 0; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void launch(int i)
{
    last_game = i;
    pick = i;
    vm_opts_t o = base;
    if (hash_left) { o.deterministic = 1; }
    if (vm_start(carts[i].src ? carts[i].src : carts[i].csrc, carts[i].len, carts[i].title, &o) != 0) go(OS_ERROR, 1);
    else go(OS_GAME, 1);
}

static uint32_t hash_seed;

void os_restart_game(void)
{
    if (mode == OS_GAME || mode == OS_PAUSED || mode == OS_ERROR) vm_stop();
    if (last_game >= 0) launch(last_game);
}

void os_hash_game(int frames, uint32_t seed)
{
    hash_left = frames;
    hash_seed = seed;
    base.seed = seed;
    os_restart_game();
    if (last_game < 0) hash_left = 0;
}

int os_hash_frames_left(void) { return hash_left; }

void os_run_source(const char *src, size_t len)
{
    if (mode == OS_GAME || mode == OS_PAUSED || mode == OS_ERROR) vm_stop();
    mode = OS_GALLERY;
    int idx = -1;
    for (int i = 0; i < ncarts; i++) if (carts[i].uploaded) idx = i;
    if (idx < 0) {
        if (ncarts == MAX_CARTS) return;
        memmove(&carts[1], &carts[0], sizeof carts[0] * ncarts);   // uploads go first in the gallery
        ncarts++;
        if (last_game >= 0) last_game++;
        idx = 0;
        memset(&carts[0], 0, sizeof carts[0]);
    } else {
        plat_asset_free(carts[idx].src);
    }
    entry_t *e = &carts[idx];
    e->src = plat_asset_alloc(len + 1);
    if (!e->src) return;
    memcpy(e->src, src, len);
    e->src[len] = 0;
    e->csrc = e->src;
    e->len = len;
    e->uploaded = 1;
    scan(e, "Uploaded");
    launch(idx);
}

int os_fps(void) { return mode == OS_GAME ? vm_fps() : 30; }
os_mode_t os_mode(void) { return mode; }
const char *os_game_name(void) { return last_game >= 0 ? carts[last_game].title : ""; }

void os_frame(const os_input_t *oi)
{
    const vm_input_t *in = &oi->in;
    int side = set.left_handed ? VM_LEFT : VM_RIGHT;
    notch_acc += in->delta[side];
    int notches = notch_acc / 4;
    notch_acc -= notches * 4;
    int select = in->pressed[0] || in->pressed[1];

    switch (mode) {
    case OS_WELCOME:
        focus = clampi(focus + notches, 1, 3);
        if (focus == 2 && last_game < 0 && notches) focus = notches > 0 ? 3 : 1;   // Resume is greyed out
        if (select) {
            if (focus == 1) { pick = last_game >= 0 ? last_game : 0; go(OS_GALLERY, 1); }
            else if (focus == 2 && last_game >= 0) launch(last_game);
            else if (focus == 3) { settings_from = OS_WELCOME; settings_row = 1; go(OS_SETTINGS, 1); }
        }
        break;
    case OS_GALLERY:
        pick = clampi(pick + notches, -1, ncarts - 1);
        if (select) { if (pick < 0) { pick = 0; go(OS_WELCOME, 1); } else go(OS_CARD, 1); }
        break;
    case OS_CARD:
        focus = clampi(focus + notches, 0, 1);
        if (select) { if (focus == 0) go(OS_GALLERY, 1); else launch(pick); }
        break;
    case OS_SETTINGS:
        settings_row = clampi(settings_row + notches, 1, NSETTINGS);
        if (select) {
            if (settings_row <= 2) { level_kind = settings_row; go(OS_LEVEL, 1); }
            else if (settings_row == 3) { set.left_handed = !set.left_handed; notch_acc = 0; save_settings(); }
            else { save_settings(); go(settings_from, settings_from == OS_PAUSED ? 2 : 3); }
        }
        break;
    case OS_LEVEL:
        if (level_kind == 1) set.sound = (uint8_t)clampi(set.sound + notches, 0, 10);
        else { set.light = (uint8_t)clampi(set.light + notches, 1, 10); if (notches) apply_settings(); }
        if (select) { save_settings(); go(OS_SETTINGS, 1); }
        break;
    case OS_GAME:
        if (hash_left > 0) {   // hash runs: no input, no chords, like the simulator
            vm_input_t none;
            memset(&none, 0, sizeof none);
            if (vm_frame(&none) != 0) { hash_left = 0; go(OS_ERROR, 1); break; }
            hash_left--;
            return;            // the game drew this frame
        }
        if (oi->chord_exit) { vm_stop(); go(OS_GALLERY, 1); break; }
        if (oi->chord_pause) {
            memcpy(frozen, plat_framebuffer(), SCREEN_W * SCREEN_H);
            go(OS_PAUSED, 1);
            break;
        }
        if (vm_frame(in) != 0) go(OS_ERROR, 1);
        return;                // the game drew this frame
    case OS_PAUSED:
        focus = clampi(focus + notches, 1, 3);
        if (oi->chord_exit) { vm_stop(); go(OS_GALLERY, 1); break; }
        if (select) {
            if (focus == 1) {
                memcpy(plat_framebuffer(), frozen, SCREEN_W * SCREEN_H);   // the game's last frame
                go(OS_GAME, 1);
                return;
            }
            if (focus == 2) { settings_from = OS_PAUSED; settings_row = 1; go(OS_SETTINGS, 1); }
            else { vm_stop(); go(OS_GALLERY, 1); }
        }
        break;
    case OS_ERROR:
        if (select || oi->chord_exit) { vm_stop(); go(OS_GALLERY, 1); }
        break;
    }

    switch (mode) {
    case OS_WELCOME:  draw_welcome(); break;
    case OS_GALLERY:  draw_gallery(); break;
    case OS_CARD:     draw_card(); break;
    case OS_SETTINGS: draw_settings(); break;
    case OS_LEVEL:    draw_level(); break;
    case OS_PAUSED:   draw_paused(); break;
    case OS_ERROR:    vm_draw_error_screen(); break;
    case OS_GAME:     break;   // a launch drew nothing yet; the next frame runs the game
    }
}
