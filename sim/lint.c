// Static checks for a cart's source: other consoles' words and other languages' operators, plus
// the cart table's title, tagline and icon. Runtime problems (unknown names, bugs, stuck loops) are
// found by the smoke run in main.c. Code inside strings and comments is ignored, so sprite art and
// explanations can say anything.
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "lint.h"

typedef struct { const char *pat; int word; const char *msg; } rule_t;

// word = 1: the pattern must start at a word boundary (so "abtn(" doesn't match "btn(").
static const rule_t RULES[] = {
    { "btn(", 1, "TinyGame has no btn(): use knob.pressed(\"left\"), knob.held(...) or knob.turn(...)" },
    { "btnp(", 1, "TinyGame has no btnp(): use knob.pressed(\"left\") or knob.turn(...)" },
    { "spr(", 1, "TinyGame has no spr(): use gfx.sprite(name, x, y)" },
    { "cls(", 1, "TinyGame has no cls(): use gfx.clear(colour)" },
    { "pset(", 1, "TinyGame has no pset(): use gfx.pixel(x, y, colour)" },
    { "rectfill(", 1, "TinyGame has no rectfill(): use gfx.fill_rect(x, y, w, h, colour)" },
    { "circfill(", 1, "TinyGame has no circfill(): use gfx.fill_circle(x, y, r, colour)" },
    { "love.", 1, "This is LOVE2D code; TinyGame uses gfx, knob, tilt and save (see the API card)" },
    { "pygame", 1, "This is Python/pygame; TinyGame carts are Lua using gfx, knob, tilt and save" },
    { "function _init", 1, "Use init(), update() and draw(), without the underscore" },
    { "function _update", 1, "Use init(), update() and draw(), without the underscore" },
    { "function _draw", 1, "Use init(), update() and draw(), without the underscore" },
    { "function update(dt", 1, "update() takes no dt: every frame is 1/30 s, so move a fixed amount per frame" },
    { "+=", 0, "Lua has no +=: write x = x + 1" },
    { "-=", 0, "Lua has no -=: write x = x - 1" },
    { "*=", 0, "Lua has no *=: write x = x * 2" },
    { "/=", 0, "Lua has no /=: write x = x / 2" },
    { "!=", 0, "Lua writes 'not equal' as ~=" },
    { "++", 0, "Lua has no ++: write x = x + 1" },
    { "&&", 0, "Lua writes && as and" },
    { "||", 0, "Lua writes || as or" },
    { "require", 1, "Carts can't load other files: put everything in one .lua file" },
    { "os.", 1, "Carts can't use os: use sys.time() for time" },
    { "io.", 1, "Carts can't use io: use save.set/save.get to keep data" },
    { "load(", 1, "Carts can't load code from strings" },
    { "dofile", 1, "Carts can't load other files" },
};

static int is_word(char c) { return isalnum((unsigned char)c) || c == '_' || c == '.'; }

// Length of a long bracket opener like [[ or [==[ at p, or 0.
static int long_open(const char *p, const char *end, int *level)
{
    if (p >= end || *p != '[') return 0;
    const char *q = p + 1;
    int lv = 0;
    while (q < end && *q == '=') { lv++; q++; }
    if (q < end && *q == '[') { *level = lv; return (int)(q - p + 1); }
    return 0;
}

static const char *long_close(const char *p, const char *end, int level, int *lines)
{
    for (; p < end; p++) {
        if (*p == '\n') (*lines)++;
        if (*p == ']') {
            const char *q = p + 1;
            int lv = 0;
            while (q < end && *q == '=') { lv++; q++; }
            if (lv == level && q < end && *q == ']') return q + 1;
        }
    }
    return end;
}

int lint_source(const char *src, size_t len, lint_report_fn report, void *ctx)
{
    const char *p = src, *end = src + len;
    int line = 1, errors = 0;
    int last_line[sizeof RULES / sizeof RULES[0] + 1] = { 0 };   // each rule reports once per line
    while (p < end) {
        char c = *p;
        if (c == '\n') { line++; p++; continue; }
        if (c == '-' && p + 1 < end && p[1] == '-') {   // comment
            int lv, n = long_open(p + 2, end, &lv);
            if (n) { p = long_close(p + 2 + n, end, lv, &line); continue; }
            while (p < end && *p != '\n') p++;
            continue;
        }
        if (c == '"' || c == '\'') {                    // short string
            p++;
            while (p < end && *p != c && *p != '\n') { if (*p == '\\' && p + 1 < end) p++; p++; }
            if (p < end && *p == c) p++;
            continue;
        }
        int lv, n = long_open(p, end, &lv);
        if (n) { p = long_close(p + n, end, lv, &line); continue; }   // long string, e.g. sprite art
        if (c == '/' && p + 1 < end && p[1] == '/') {    // // is integer division unless it opens a line
            const char *b = p;
            while (b > src && (b[-1] == ' ' || b[-1] == '\t')) b--;
            size_t k = sizeof RULES / sizeof RULES[0];
            if ((b == src || b[-1] == '\n') && last_line[k] != line) {
                report(ctx, 1, line, "Lua comments start with --, not //");
                errors++; last_line[k] = line;
            }
            p += 2;
            continue;
        }
        for (size_t r = 0; r < sizeof RULES / sizeof RULES[0]; r++) {
            size_t pl = strlen(RULES[r].pat);
            if (last_line[r] == line || (size_t)(end - p) < pl || strncmp(p, RULES[r].pat, pl)) continue;
            if (RULES[r].word && p > src && is_word(p[-1])) continue;
            report(ctx, 1, line, RULES[r].msg);
            errors++; last_line[r] = line;
            break;
        }
        p++;
    }
    return errors;
}

int lint_icon(const char *art, lint_report_fn report, void *ctx)
{
    if (!art || !*art) { report(ctx, 0, -1, "cart.icon is missing: the gallery will show the first letter of the title"); return 0; }
    int rows = 0, width = -1;
    while (*art) {
        const char *e = strchr(art, '\n');
        if (!e) e = art + strlen(art);
        const char *a = art, *b = e;
        while (a < b && isspace((unsigned char)*a)) a++;
        while (b > a && isspace((unsigned char)b[-1])) b--;
        if (b > a) {
            int w = (int)(b - a);
            if (width < 0) width = w;
            if (w != width) {   // the game still runs; the gallery shows the title's first letter instead
                report(ctx, 0, -1, "cart.icon rows must all be the same length (the gallery will show a letter instead)");
                return 0;
            }
            for (const char *q = a; q < b; q++)
                if (!(*q == '.' || isxdigit((unsigned char)*q))) {
                    report(ctx, 0, -1, "cart.icon may only use . and 0-9 a-f (the gallery will show a letter instead)");
                    return 0;
                }
            rows++;
        }
        art = *e ? e + 1 : e;
    }
    if (width != 16 || rows != 16) {
        char m[96];
        snprintf(m, sizeof m, "cart.icon is %dx%d; it should be 16x16 to fill the gallery tile", width, rows);
        report(ctx, 0, -1, m);
        return 0;
    }
    return 0;
}
