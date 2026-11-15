// OS shell: Welcome, the game shelf, Game Card, Settings, Paused, the Colour Card and the game/error
// states around the vm. Look and layout: docs/ref-designs (design-OS.png, design-system.png).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "os.h"
#include "gfx.h"
#include "font_aa.h"
#include "platform.h"

#define MAX_CARTS 32
#define ICON_MAX  16

typedef struct {
    char id[32];            // save id, and the file name for stored carts
    char *src;              // owned copy for an upload that couldn't be stored, else NULL
    const char *csrc;       // built-in source compiled into the firmware, else NULL
    int stored;             // source lives on the carts volume and is read at launch
    size_t len;
    char title[24], tagline[48];
    uint8_t icon[ICON_MAX * ICON_MAX];
    int icon_w, icon_h;     // 0 when the cart has no usable icon
    uint8_t styled, body, win;   // cartridge colour and icon window colour, chosen from the icon (see style_entry)
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
static int notch_acc;
static uint8_t *frozen;               // the game frame behind the pause menu
static int hash_left;

// No SOUND row: this hardware has no audio, and a control that does nothing would confuse kids.
static const char *SETTINGS_ROWS[] = { "Light", "Hands", "Back" };
enum { ROW_LIGHT = 1, ROW_HANDS, ROW_BACK, NSETTINGS = ROW_BACK };

// ---------- carts ----------
static int hexval(char c)
{
    if (c == '.') return GFX_CLEAR;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'v') return c - 'a' + 10;   // a-v are colours 10-31
    if (c >= 'A' && c <= 'V') return c - 'A' + 10;
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

static void scan(entry_t *e, const char *src, size_t len, const char *fallback)
{
    vm_meta_t *m = plat_asset_alloc(sizeof *m);   // too big for a task stack
    if (m && src && vm_scan(src, len, &base, m) == 0 && m->title[0]) {
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

static void announce(int i)
{
    printf("{\"t\":\"shell\",\"cart\":%d,\"id\":\"%s\",\"title\":\"%s\",\"stored\":%d,\"icon\":%d}\n",
           i, carts[i].id, carts[i].title, carts[i].stored, carts[i].icon_w > 0);
}

int os_add_cart(const char *src, size_t len, const char *id)
{
    if (ncarts == MAX_CARTS) return -1;
    entry_t *e = &carts[ncarts];
    memset(e, 0, sizeof *e);
    snprintf(e->id, sizeof e->id, "%s", id);
    e->csrc = src;
    e->len = len;
    scan(e, src, len, id);
    announce(ncarts);
    return ncarts++;
}

static int find_id(const char *id)
{
    for (int i = 0; i < ncarts; i++) if (!strcmp(carts[i].id, id)) return i;
    return -1;
}

// Adds (or refreshes) a cart from the carts volume. Returns its index, or -1.
static int add_stored(const char *id)
{
    size_t len;
    char *src = plat_cart_load(id, &len);
    if (!src) return -1;
    int i = find_id(id);
    if (i < 0) {
        if (ncarts == MAX_CARTS) { plat_asset_free(src); return -1; }
        i = ncarts++;
    } else plat_asset_free(carts[i].src);
    entry_t *e = &carts[i];
    memset(e, 0, sizeof *e);
    snprintf(e->id, sizeof e->id, "%s", id);
    e->stored = 1;
    e->len = len;
    scan(e, src, len, id);
    plat_asset_free(src);
    announce(i);
    return i;
}

void os_scan_storage(void)
{
    static char ids[MAX_CARTS][32];
    int n = plat_cart_list(ids, MAX_CARTS);
    for (int i = 0; i < n; i++) add_stored(ids[i]);
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
// "Deep Toy Blue": a flat electric-blue field, white type, yellow action blocks with a navy drop-edge,
// primary-colour cartridges, plain text lists with a pointer, and Space Grotesk type. y is the top of the capital letters.
static int tick;   // counts frames, for the bobbing blocks
#define SHADOW 4   // drop-edge offset, 45 degrees down and right

static void sky(void) { gfx_clear(OS_BLUE); }

static void text_l(const char *s, int x, int y, int c, int font) { gfx_aa_text(s, x, y, c, font); }
static void text_c(const char *s, int cx, int y, int c, int font) { gfx_aa_text(s, cx - gfx_aa_width(s, font) / 2, y, c, font); }
static void text_r(const char *s, int right, int y, int c, int font) { gfx_aa_text(s, right - gfx_aa_width(s, font), y, c, font); }

// Solid block with a drop-edge in a darker tone of itself: toy yellow when focused. Text on it is navy.
static void button(int x, int y, int w, int h, int on)
{
    gfx_fill_rect(x + SHADOW, y + SHADOW, w, h, on ? OS_SHADE + 1 : OS_SHADE + 5);   // shadow: the colour, 20% darker
    gfx_fill_rect(x, y, w, h, on ? OS_YELLOW : OS_PALE);
}

static void pointer(int x, int y, int c)   // right-pointing triangle, 9 x 12
{
    for (int r = 0; r < 12; r++) {
        int d = r * 2 - 11, w = 9 - (d < 0 ? -d : d) * 9 / 11;
        gfx_fill_rect(x, y + r, w, 1, c);
    }
}

static void chevron(int x, int y, int c)   // "<", 7 x 14
{
    for (int k = 0; k < 7; k++) { gfx_fill_rect(x + 6 - k, y + k, 2, 1, c); gfx_fill_rect(x + 6 - k, y + 13 - k, 2, 1, c); }
}

static void underline(int x, int y, int w, int c) { gfx_fill_rect(x, y, w, 3, c); }

// A convex quad, filled a row at a time.
static void quad(const int p[8], int c)
{
    int y0 = p[1], y1 = p[1];
    for (int i = 1; i < 4; i++) { if (p[i * 2 + 1] < y0) y0 = p[i * 2 + 1]; if (p[i * 2 + 1] > y1) y1 = p[i * 2 + 1]; }
    for (int y = y0; y <= y1; y++) {
        int lo = 9999, hi = -9999;
        for (int i = 0; i < 4; i++) {
            int ax = p[i * 2], ay = p[i * 2 + 1], bx = p[((i + 1) % 4) * 2], by = p[((i + 1) % 4) * 2 + 1];
            if ((y < ay && y < by) || (y > ay && y > by)) continue;
            if (ay == by) {
                if (ax < lo) lo = ax;
                if (ax > hi) hi = ax;
                if (bx < lo) lo = bx;
                if (bx > hi) hi = bx;
                continue;
            }
            int x = ax + (bx - ax) * (y - ay) / (by - ay);
            if (x < lo) lo = x;
            if (x > hi) hi = x;
        }
        if (hi >= lo) gfx_fill_rect(lo, y, hi - lo + 1, 1, c);
    }
}

// One block seen from a corner. (cx, cy) is the bottom point of its top face; e is the edge length.
static void iso_cube(int cx, int cy, int e, int first)
{
    int k = e * 866 / 1000, h = e / 2;
    const int left[8]  = { cx - k, cy - h, cx, cy, cx, cy + e, cx - k, cy + h };
    const int right[8] = { cx, cy, cx + k, cy - h, cx + k, cy + h, cx, cy + e };
    const int top[8]   = { cx, cy - e, cx + k, cy - h, cx, cy, cx - k, cy - h };
    quad(left, first + 1);
    quad(right, first + 2);
    quad(top, first);
}

static void hero_blocks(int cx, int cy)
{
    static const int bob[8] = { 0, -1, -2, -3, -3, -2, -1, 0 };
    int dy = bob[(tick / 4) & 7], e = 31, k = e * 866 / 1000;
    iso_cube(cx - k, cy + 3, e, OS_CUBE + 3);       // left
    iso_cube(cx + k, cy + 3, e, OS_CUBE + 6);       // right
    iso_cube(cx, cy - 12 + dy, e, OS_CUBE);          // top, bobbing
}

static int cart_colour(int i) { static const int c[] = { OS_RED, OS_YELLOW, OS_GREEN, OS_PURPLE, OS_ORANGE }; return c[i % 5]; }

static double luminance(uint32_t rgb)
{
    double l = 0;
    static const double k[3] = { 0.2126, 0.7152, 0.0722 };
    for (int c = 0; c < 3; c++) l += k[c] * pow(((rgb >> (16 - 8 * c)) & 0xFF) / 255.0, 2.2);
    return l;
}

static double contrast(uint32_t a, uint32_t b)
{
    double la = luminance(a), lb = luminance(b);
    if (la < lb) { double t = la; la = lb; lb = t; }
    return (la + 0.05) / (lb + 0.05);
}

// Picks the cartridge colour and the icon window colour for one game so the icon stays readable on
// its sticker: every colour the icon uses is tested against each candidate window, and the window
// whose worst contrast is best wins. Tinted windows keep their cartridge colour; the royal blue and
// pale blue windows go on a cartridge picked by position.
static void style_entry(entry_t *e, int i)
{
    if (e->styled) return;
    e->styled = 1;
    int used[32] = { 0 };
    for (int k = 0; k < e->icon_w * e->icon_h; k++) if (e->icon[k] < 32) used[e->icon[k]]++;
    struct { int win, body; } cand[12];
    int n = 0;
    for (int k = 0; k < 5; k++) { cand[n].win = OS_TINT + k; cand[n++].body = cart_colour(k); }
    for (int k = 0; k < 5; k++) { cand[n].win = OS_TINT2 + k; cand[n++].body = cart_colour(k); }
    cand[n].win = OS_ROYAL; cand[n++].body = cart_colour(i);
    cand[n].win = OS_PALE; cand[n++].body = cart_colour(i);
    int best = i % 5;   // no icon: the plain tint for this slot
    double best_score = -1;
    if (e->icon_w) {
        for (int c = 0; c < n; c++) {
            double worst = 99;
            for (int k = 0; k < 32; k++) {
                if (used[k] < 2) continue;
                double r = contrast(gfx_palette_rgb(cand[c].win), gfx_palette_rgb(k));
                if (r < worst) worst = r;
            }
            if (worst > best_score + 0.05) { best_score = worst; best = c; }
        }
        e->win = (uint8_t)cand[best].win;
        e->body = (uint8_t)cand[best].body;
    } else {
        e->win = (uint8_t)(OS_TINT + best);
        e->body = (uint8_t)cart_colour(best);
    }
}

// A cartridge: chamfered top corners, a colour body, an icon window ("sticker"), a navy drop-edge.
static void cart_shape(int x, int y, int w, int h, int c)
{
    gfx_fill_rect(x, y + 7, w, h - 7, c);
    for (int r = 0; r < 7; r++) gfx_fill_rect(x + 7 - r, y + r, w - 2 * (7 - r), 1, c);
}

static int shade_slot(int body) { return body == OS_RED ? 0 : body == OS_YELLOW ? 1 : body == OS_GREEN ? 2 : body == OS_PURPLE ? 3 : 4; }

static void cartridge(int x, int y, int w, int h, int i)
{
    entry_t *e = &carts[i];
    style_entry(e, i);
    cart_shape(x + SHADOW, y + SHADOW, w, h, OS_SHADE + shade_slot(e->body));   // 20% darker than the body
    cart_shape(x, y, w, h, e->body);
    gfx_fill_rect(x + 5, y + 11, w - 10, h - 16, e->win);
}

static void logo(int x, int y)   // three small blocks in a row
{
    gfx_fill_rect(x, y, 7, 7, OS_RED);
    gfx_fill_rect(x + 9, y, 7, 7, OS_YELLOW);
    gfx_fill_rect(x + 18, y, 7, 7, OS_GREEN);
}

static void battery(int x, int y)
{
    gfx_rect(x, y, 22, 11, OS_WHITE);
    gfx_fill_rect(x + 22, y + 3, 2, 5, OS_WHITE);
    for (int i = 0; i < 3; i++) gfx_fill_rect(x + 2 + i * 6, y + 2, 5, 7, OS_WHITE);
}

static void knob_glyph(int x, int y, int c)
{
    static const char *g[7] = { "..###..", ".#...#.", "#..#..#", "#..#..#", "#.....#", ".#...#.", "..###.." };
    for (int r = 0; r < 7; r++)
        for (int k = 0; k < 7; k++)
            if (g[r][k] == '#') gfx_pixel(x + k, y + r, c);
}

// Hints sit in the two bottom corners.
static void footer(const char *scroll_label, const char *select_label, int ink)
{
    const char *l = set.left_handed ? scroll_label : select_label;
    const char *r = set.left_handed ? select_label : scroll_label;
    int y = SCREEN_H - 17;
    knob_glyph(12, y + 1, ink);
    text_l(l, 24, y, ink, FONT_S);
    knob_glyph(SCREEN_W - 12 - 7 - 5 - gfx_aa_width(r, FONT_S), y + 1, ink);
    text_r(r, SCREEN_W - 12, y, ink, FONT_S);
}

static void back_arrow(int focused)
{
    if (focused) {
        button(8, 6, 30, 24, 1);
        chevron(17, 11, OS_INK);
    } else chevron(17, 11, OS_WHITE);
}

static void draw_icon(const entry_t *e, int cx, int cy, int scale, int flip)
{
    if (e->icon_w)
        gfx_blit(e->icon, e->icon_w, e->icon_h, cx - e->icon_w * scale / 2, cy - e->icon_h * scale / 2, flip, 0, scale);
    else {   // no icon: the title's first letter
        char s[2] = { e->title[0] ? e->title[0] : '?', 0 };
        if (s[0] >= 'a' && s[0] <= 'z') s[0] -= 32;
        text_c(s, cx, cy - gfx_aa_cap(FONT_XL) / 2, OS_INK, FONT_XL);
    }
}

// Greedy word wrap to maxw pixels, lines 14 px apart.
static void wrap_text(const char *s, int x, int y, int maxw, int c, int lines)
{
    char b[64];
    while (*s && lines-- > 0) {
        int n = 0, best = 0;
        for (;;) {
            if (s[n] == ' ' || s[n] == 0) {
                snprintf(b, sizeof b, "%.*s", n > 63 ? 63 : n, s);
                if (n > 0 && gfx_aa_width(b, FONT_S) > maxw && best > 0) break;
                best = n;
                if (s[n] == 0) break;
            }
            if (n >= 62) { if (!best) best = n; break; }
            n++;
        }
        if (!best) best = n;
        snprintf(b, sizeof b, "%.*s", best > 63 ? 63 : best, s);
        text_l(b, x, y, c, FONT_S);
        y += 14;
        s += best;
        while (*s == ' ') s++;
    }
}

// ---------- screens ----------
static void draw_welcome(void)
{
    sky();
    logo(12, 11);
    text_l("TinyGame", 44, 9, OS_WHITE, FONT_M);
    battery(SCREEN_W - 34, 10);
    hero_blocks(86, 96);
    if (focus == 1) {
        pointer(150, 51, OS_WHITE);
        button(166, 38, 134, 38, 1);
        text_c("START", 233, 46, OS_INK, FONT_XL);
    } else text_l("START", 166, 46, OS_WHITE, FONT_XL);
    for (int i = 2; i <= 3; i++) {
        int y = 92 + (i - 2) * 26, off = i == 2 && last_game < 0;
        const char *t = i == 2 ? "Resume" : "Settings";
        text_l(t, 166, y, off ? OS_MUTED : OS_WHITE, FONT_M);
        if (focus == i) { pointer(150, y - 1, OS_WHITE); underline(166, y + 15, gfx_aa_width(t, FONT_M), OS_YELLOW); }
    }
    if (safe) text_l("Safe mode", 166, 132, OS_MUTED, FONT_S);
    footer("Browse", "Select", OS_WHITE);
}

// The shelf is a strip of cartridges centred on the selected one. Cartridges at the ends run off the
// screen, so a half-hidden one tells you there are more to scroll to.
#define SHELF_W 64        // cartridge width and height
#define SHELF_H 66
#define SHELF_SEL_W 80    // the selected cartridge is bigger and raised
#define SHELF_SEL_H 84
#define SHELF_GAP 12

static void draw_gallery(void)
{
    sky();
    back_arrow(pick < 0);
    text_l("Games", 44, 12, OS_WHITE, FONT_L);
    if (ncarts == 0) { text_c("No games yet", SCREEN_W / 2, 76, OS_WHITE, FONT_L); footer("Browse", "Select", OS_WHITE); return; }
    char n[16];
    snprintf(n, sizeof n, "%d / %d", (pick < 0 ? 0 : pick) + 1, ncarts);
    text_r(n, SCREEN_W - 12, 15, OS_WHITE, FONT_S);
    int p = pick < 0 ? 0 : pick;
    int base = 118;   // bottom edge shared by all cartridges
    int sel_x = (SCREEN_W - SHELF_SEL_W) / 2;
    for (int i = 0; i < ncarts; i++) {
        int sel = i == pick, w = sel ? SHELF_SEL_W : SHELF_W, h = sel ? SHELF_SEL_H : SHELF_H, x;
        if (i < p) x = sel_x - (p - i) * (SHELF_W + SHELF_GAP);
        else if (i == p) x = sel_x + (sel ? 0 : (SHELF_SEL_W - SHELF_W) / 2);
        else x = sel_x + SHELF_SEL_W + SHELF_GAP + (i - p - 1) * (SHELF_W + SHELF_GAP);
        if (x + w + SHADOW <= 0 || x >= SCREEN_W) continue;
        cartridge(x, base - h, w, h, i);
        draw_icon(&carts[i], x + w / 2, base - h + 11 + (h - 16) / 2, sel ? 4 : 3, 0);
    }
    if (pick < 0) {
        text_c("Back", SCREEN_W / 2, 132, OS_WHITE, FONT_L);
    } else {
        const char *name = carts[pick].title;
        text_c(name, SCREEN_W / 2, 132, OS_WHITE, gfx_aa_width(name, FONT_L) <= SCREEN_W - 16 ? FONT_L : FONT_M);
    }
    footer("Browse", "Select", OS_WHITE);
}

static void draw_card(void)
{
    const entry_t *e = &carts[pick];
    sky();
    back_arrow(focus == 0);
    char n[16];
    snprintf(n, sizeof n, "%d / %d", pick + 1, ncarts);
    text_r(n, SCREEN_W - 12, 15, OS_WHITE, FONT_S);
    cartridge(12, 40, 126, 104, pick);
    draw_icon(e, 75, 40 + 11 + 44, 4, 0);
    text_l(e->title, 152, 40, OS_WHITE, gfx_aa_width(e->title, FONT_L) <= 160 ? FONT_L : FONT_M);
    wrap_text(e->tagline, 152, 70, 160, OS_WHITE, 3);
    button(152, 112, 140, 30, focus == 1);
    text_c("PLAY", 222, 118, OS_INK, FONT_L);
    footer("Browse", "Select", OS_WHITE);
}

static void draw_settings(void)
{
    sky();
    back_arrow(0);
    text_l("Settings", 44, 12, OS_WHITE, FONT_L);
    battery(SCREEN_W - 34, 10);
    for (int i = 1; i <= NSETTINGS; i++) {
        int y = 50 + (i - 1) * 34, on = settings_row == i;
        const char *t = SETTINGS_ROWS[i - 1];
        text_l(t, 50, y, OS_WHITE, FONT_L);
        if (on) { pointer(30, y + 3, OS_WHITE); underline(50, y + 22, gfx_aa_width(t, FONT_L), OS_YELLOW); }
        if (i == ROW_LIGHT)
            for (int k = 0; k < 5; k++)
                gfx_fill_rect(176 + k * 26, y + 3, 20, 12, k < (set.light + 1) / 2 ? OS_YELLOW : OS_INK);
        else if (i == ROW_HANDS) text_r(set.left_handed ? "<  Left  >" : "<  Right  >", 300, y + 2, OS_WHITE, FONT_M);
    }
    if (settings_row == ROW_BACK) back_arrow(1);
    footer("Browse", settings_row == ROW_HANDS ? "Switch" : "Edit", OS_WHITE);
}

static void draw_level(void)
{
    int v = set.light;
    sky();
    text_l("Light", 14, 12, OS_WHITE, FONT_L);
    char pct[8];
    snprintf(pct, sizeof pct, "%d%%", v * 10);
    text_c(pct, SCREEN_W / 2, 38, OS_WHITE, FONT_XL);
    for (int i = 1; i <= 10; i++) {
        gfx_fill_rect(70 + (i - 1) * 18, 79, 14, 14, OS_WHITE);
        gfx_fill_rect(70 + (i - 1) * 18, 76, 14, 14, i <= v ? OS_YELLOW : OS_MUTED);
    }
    text_c(set.left_handed ? "Turn the left knob" : "Turn the right knob", SCREEN_W / 2, 102, OS_WHITE, FONT_M);
    text_c("to adjust", SCREEN_W / 2, 120, OS_WHITE, FONT_M);
    footer("Level", "Done", OS_WHITE);
}

static void draw_paused(void)
{
    uint8_t *fb = plat_framebuffer();
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = (uint8_t)(OS_DIM + (frozen[i] & 31));
    gfx_fill_rect(48 + SHADOW, 8 + SHADOW, 224, 132, OS_SHADE + 5);
    gfx_fill_rect(48, 8, 224, 132, OS_PALE);
    text_l("Paused", 70, 18, OS_INK, FONT_L);
    static const char *items[] = { "Continue", "Settings", "Exit game" };
    for (int i = 1; i <= 3; i++) {
        int y = 52 + (i - 1) * 28;
        text_l(items[i - 1], 94, y, OS_INK, FONT_M);
        if (focus == i) { pointer(74, y - 1, OS_INK); underline(94, y + 15, gfx_aa_width(items[i - 1], FONT_M), OS_RED); }
    }
    gfx_fill_rect(0, SCREEN_H - 22, SCREEN_W, 22, OS_INK);
    footer("Browse", "Select", OS_WHITE);
}

// ---------- colour card ----------
// Test patterns for judging the panel, with live tuning of the per-channel correction. The values
// are not saved: read them off the screen (the board also prints them on the serial console).
static int cc_page, cc_chan;
static int gam[3] = { 100, 100, 100 };   // exponents in hundredths
enum { CC_BARS, CC_RAMPS, CC_OS, CC_GAME, CC_TUNE, CC_PAGES };

static void cc_label(const char *name, int page)
{
    char b[48];
    snprintf(b, sizeof b, "%d/%d  %s", page + 1, CC_PAGES, name);
    text_l(b, 8, SCREEN_H - 15, OS_WHITE, FONT_S);
    text_r("Click: next", SCREEN_W - 8, SCREEN_H - 15, OS_WHITE, FONT_S);
}

static void cc_ramp(int y, int h, int chan)   // 16 steps of one channel (3 = grey)
{
    for (int i = 0; i < 16; i++) {
        uint32_t v = (uint32_t)(i * 17);
        uint32_t rgb = chan == 0 ? v << 16 : chan == 1 ? v << 8 : chan == 2 ? v : (v << 16) | (v << 8) | v;
        gfx_fill_rect(i * 20, y, 20, h, gfx_color(rgb));
    }
}

static void draw_colourcard(void)
{
    gfx_clear(0);
    switch (cc_page) {
    case CC_BARS: {
        static const uint32_t bars[8] = { 0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00, 0xFF00FF, 0xFF0000, 0x0000FF, 0x000000 };
        for (int i = 0; i < 8; i++) gfx_fill_rect(i * 40, 0, 40, 112, gfx_color(bars[i]));
        cc_ramp(112, 38, 3);
        cc_label("Colour bars and greys", cc_page);
        break;
    }
    case CC_RAMPS:
        for (int c = 0; c < 4; c++) cc_ramp(c * 37, 37, c);
        cc_label("Red, green, blue, grey", cc_page);
        break;
    case CC_OS: {
        static const int col[15] = { OS_BLUE, OS_YELLOW, OS_RED, OS_GREEN, OS_PURPLE, OS_ORANGE, OS_PALE, OS_ROYAL, OS_WHITE, OS_INK,
                                     OS_SHADE, OS_SHADE + 1, OS_SHADE + 2, OS_SHADE + 3, OS_SHADE + 4 };
        for (int i = 0; i < 15; i++) gfx_fill_rect((i % 5) * 64, (i / 5) * 50, 64, 50, col[i]);
        cc_label("The OS colours", cc_page);
        break;
    }
    case CC_GAME:
        for (int i = 0; i < 32; i++) gfx_fill_rect((i % 8) * 40, (i / 8) * 37, 40, 37, i);
        cc_label("The 32 game colours", cc_page);
        break;
    default: {
        static const char *names[3] = { "Red", "Green", "Blue" };
        text_l("Tune", 12, 8, OS_WHITE, FONT_L);
        for (int c = 0; c < 3; c++) {
            char b[24];
            snprintf(b, sizeof b, "%s  %d.%02d", names[c], gam[c] / 100, gam[c] % 100);
            if (c == cc_chan) pointer(8, 40 + c * 24, OS_WHITE);
            text_l(b, 26, 40 + c * 24, OS_WHITE, FONT_M);
        }
        for (int i = 0; i < 8; i++) gfx_fill_rect(150 + i * 20, 12, 20, 30, gfx_color((uint32_t)(i * 36) * 0x010101));
        static const int sw[6] = { OS_YELLOW, OS_RED, OS_GREEN, OS_PURPLE, OS_ORANGE, OS_BLUE };
        for (int i = 0; i < 6; i++) gfx_fill_rect(150 + (i % 3) * 53, 50 + (i / 3) * 44, 53, 44, sw[i]);
        text_l("Turn: change", 12, 118, OS_WHITE, FONT_S);
        text_l("Click: next colour", 12, 132, OS_WHITE, FONT_S);
        text_l("Not saved", 12, 146, OS_MUTED, FONT_S);
        break;
    }
    }
}

// ---------- flow ----------
static void go(os_mode_t m, int f) { mode = m; focus = f; notch_acc = 0; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void stop_game(void);

static void launch(int i)
{
    entry_t *e = &carts[i];
    if (!strcmp(e->id, "colorcard")) {   // not a real cart: the OS shows its own colour card
        float r, g, b;
        plat_get_gamma(&r, &g, &b);
        gam[0] = (int)(r * 100 + 0.5f); gam[1] = (int)(g * 100 + 0.5f); gam[2] = (int)(b * 100 + 0.5f);
        cc_page = 0; cc_chan = 0;
        pick = i;
        go(OS_COLOURCARD, 1);
        return;
    }
    last_game = i;
    pick = i;
    vm_opts_t o = base;
    o.deterministic = hash_left > 0;
    o.save_id = e->id;
    const char *src = e->src ? e->src : e->csrc;
    char *loaded = NULL;
    size_t len = e->len;
    if (e->stored) {
        loaded = plat_cart_load(e->id, &len);
        src = loaded;
    }
    if (!src) {
        static const char *msg = "error('This game could not be read from storage')";
        vm_start(msg, strlen(msg), e->title, &o);   // records the error for the error screen
        go(OS_ERROR, 1);
        return;
    }
    int ok = vm_start(src, len, e->title, &o) == 0;
    plat_asset_free(loaded);   // Lua has compiled it; the text isn't needed any more
    go(ok ? OS_GAME : OS_ERROR, 1);
}


void os_restart_game(void)
{
    stop_game();
    if (last_game >= 0) launch(last_game);
}

void os_hash_game(int frames, uint32_t seed)
{
    hash_left = frames;
    base.seed = seed;
    os_restart_game();
    if (last_game < 0) hash_left = 0;
}

int os_hash_frames_left(void) { return hash_left; }

static int valid_id(const char *id)
{
    size_t n = strlen(id);
    if (n < 1 || n > 31) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

static void stop_game(void)
{
    if (mode == OS_GAME || mode == OS_PAUSED || mode == OS_ERROR) vm_stop();
    mode = OS_GALLERY;
}

int os_run_source(const char *src, size_t len, const char *id)
{
    if (!valid_id(id)) {
        printf("{\"t\":\"error\",\"kind\":\"upload\",\"msg\":\"cart names are 1-31 of a-z 0-9 _ -\"}\n");
        return -1;
    }
    stop_game();
    int i;
    if (plat_cart_store(id, src, len) == 0) {
        i = add_stored(id);
    } else {   // no storage (simulator without --storage): keep it in memory until the next reboot
        i = find_id(id);
        if (i < 0) {
            if (ncarts == MAX_CARTS) return -1;
            i = ncarts++;
        } else plat_asset_free(carts[i].src);
        entry_t *e = &carts[i];
        memset(e, 0, sizeof *e);
        snprintf(e->id, sizeof e->id, "%s", id);
        e->src = plat_asset_alloc(len + 1);
        if (!e->src) return -1;
        memcpy(e->src, src, len);
        e->src[len] = 0;
        e->len = len;
        scan(e, e->src, len, id);
        announce(i);
    }
    if (i < 0) return -1;
    launch(i);
    return 0;
}

void os_list(void)
{
    for (int i = 0; i < ncarts; i++) announce(i);
}

int os_play(const char *id)
{
    int i = find_id(id);
    if (i < 0) return -1;
    stop_game();
    launch(i);
    return 0;
}

int os_delete(const char *id)
{
    int i = find_id(id);
    if (i < 0 || !carts[i].stored) return -1;
    if (last_game == i) { stop_game(); last_game = -1; }
    if (plat_cart_delete(id) != 0) return -1;
    plat_asset_free(carts[i].src);
    memmove(&carts[i], &carts[i + 1], sizeof carts[0] * (ncarts - i - 1));
    ncarts--;
    if (last_game > i) last_game--;
    if (pick >= ncarts) pick = ncarts - 1;
    return 0;
}

int os_fps(void) { return mode == OS_GAME ? vm_fps() : 30; }
os_mode_t os_mode(void) { return mode; }
const char *os_game_name(void) { return last_game >= 0 ? carts[last_game].title : ""; }

void os_frame(const os_input_t *oi)
{
    tick++;
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
            if (settings_row == ROW_LIGHT) go(OS_LEVEL, 1);
            else if (settings_row == ROW_HANDS) { set.left_handed = !set.left_handed; notch_acc = 0; save_settings(); }
            else { save_settings(); go(settings_from, settings_from == OS_PAUSED ? 2 : 3); }
        }
        break;
    case OS_LEVEL:
        set.light = (uint8_t)clampi(set.light + notches, 1, 10);
        if (notches) apply_settings();
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
    case OS_COLOURCARD:
        if (oi->chord_pause || oi->chord_exit) { go(OS_GALLERY, 1); break; }
        if (cc_page == CC_TUNE && notches) {
            gam[cc_chan] = clampi(gam[cc_chan] + notches * 5, 30, 150);
            plat_set_gamma(gam[0] / 100.0f, gam[1] / 100.0f, gam[2] / 100.0f);
        }
        if (select) {
            if (cc_page == CC_TUNE) { if (++cc_chan == 3) { cc_chan = 0; cc_page = 0; } }
            else cc_page++;
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
    case OS_COLOURCARD: draw_colourcard(); break;
    case OS_GAME:     break;   // a launch drew nothing yet; the next frame runs the game
    }
}
