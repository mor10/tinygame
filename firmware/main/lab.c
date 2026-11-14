// TinyTwist Screen Lab: test pages + switchable flush strategies + timing stats.
// Renders into an 8-bit indexed 320x172 framebuffer, then expands to RGB565 strips for the panel.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "board.h"
#include "lcd.h"
#include "input.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#define LW LOGICAL_W
#define LH LOGICAL_H

const uint8_t *font_glyph(char c);

enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_BLUE, C_NAVY, C_CREAM, C_CORAL,
       C_TEAL, C_YELLOW, C_GREY, C_DGREY, C_MAGENTA, C_CYAN, C_COUNT };

static const uint16_t PAL565[C_COUNT] = {
    0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x18C9, 0xFF9A, 0xF30A,
    0x2D53, 0xF5A6, 0x8410, 0x39E7, 0xF81F, 0x07FF,
};
static uint16_t pal_be[256];   // byte-swapped for the wire

static uint8_t fb[LW * LH];
static uint8_t prev[LW * LH];

enum { PG_GEOMETRY, PG_HLINE, PG_VLINE, PG_BOX, PG_CHECKER, PG_STRIPES, PG_RAIN, PG_FLASH, PG_SCROLL, PG_INPUT, PG_COUNT };
static const char *PAGE_NAMES[PG_COUNT] = { "geometry", "hline", "vline", "box", "checker", "stripes", "rain", "flash", "scroll", "input" };

enum { MODE_HW, MODE_SW };

static struct {
    int page, mode, mhz, fps, diff, strip, speed, hud, inv, bl;
    int madctl[2];
    int sw_fx, sw_fy;
} cfg = {
    .page = PG_HLINE, .mode = MODE_SW, .mhz = 80, .fps = 30, .diff = 1, .strip = 16,
    .speed = 8, .hud = 1, .inv = BOARD_LCD_INVERT, .bl = 100,
    .madctl = { BOARD_HW_MADCTL, BOARD_SW_MADCTL }, .sw_fx = 0, .sw_fy = 1,
};

static QueueHandle_t s_cmdq;

// ---------- live tuning with the encoders ----------
// Left knob: FRCTRL2 (0xC6) 00..1F. Right knob: back+front porch lines (BPA and FPA in 0xB2), 01..40.
// Clicking a knob restores its default. The encoder task only sets the targets; the lab task
// applies them between frames so no command races a DMA flush.
#define TUNE_FR_DEFAULT 0x05
#define TUNE_FP_DEFAULT 0x0C
static volatile int tune_on = 1;
static volatile int tune_fr = 0x00, tune_fp = 0x01;   // start at the best values found so far (clicks restore the defaults above)
static int applied_fr = -1, applied_fp = -1;
// Nominal FRCTRL2 refresh rates (Hz) from the ST7789 datasheet; the real rate runs a few Hz off.
static const uint8_t FR_HZ[32] = { 119, 111, 105, 99, 94, 90, 86, 82, 78, 75, 72, 69, 67, 64, 62, 60,
                                   58, 57, 55, 53, 52, 50, 49, 48, 46, 45, 44, 43, 42, 41, 40, 39 };

static void apply_tune(void)
{
    if (tune_fr != applied_fr) {
        uint8_t v = (uint8_t)tune_fr;
        lcd_cmd(0xC6, &v, 1);
        applied_fr = tune_fr;
    }
    if (tune_fp != applied_fp) {
        uint8_t p[5] = { (uint8_t)tune_fp, (uint8_t)tune_fp, 0x00, 0x33, 0x33 };   // back and front porch together
        lcd_cmd(0xB2, p, 5);
        applied_fp = tune_fp;
    }
}

static input_state_t in_st;                 // latest snapshot, taken once per frame
static int64_t ev_until[8];                 // sticky display time for events (us)
enum { EV_L_PRESS, EV_R_PRESS, EV_L_LONG, EV_R_LONG, EV_PAUSE, EV_EXIT };
static int tune_acc[2];                     // transitions not yet turned into a tuning step

static void input_frame(int64_t now)
{
    input_snapshot(&in_st);
    for (int i = 0; i < 2; i++) {
        tune_acc[i] += in_st.delta[i];
        while (tune_acc[i] >= 4 || tune_acc[i] <= -4) {      // 4 transitions = one tuning step
            int d = tune_acc[i] > 0 ? 1 : -1;
            tune_acc[i] -= 4 * d;
            if (!tune_on) continue;
            if (i == 0) { int v = tune_fr + d; tune_fr = v < 0 ? 0 : v > 31 ? 31 : v; }
            else        { int v = tune_fp + d; tune_fp = v < 1 ? 1 : v > 64 ? 64 : v; }
        }
        if (in_st.pressed[i] && tune_on) { if (i == 0) tune_fr = TUNE_FR_DEFAULT; else tune_fp = TUNE_FP_DEFAULT; }
        if (in_st.pressed[i])    { ev_until[EV_L_PRESS + i] = now + 400000; printf("{\"t\":\"input\",\"ev\":\"%s_press\"}\n", i ? "right" : "left"); }
        if (in_st.long_press[i]) { ev_until[EV_L_LONG + i] = now + 800000;  printf("{\"t\":\"input\",\"ev\":\"%s_long\"}\n", i ? "right" : "left"); }
    }
    if (in_st.chord_pause) { ev_until[EV_PAUSE] = now + 1000000; printf("{\"t\":\"input\",\"ev\":\"chord_pause\"}\n"); }
    if (in_st.chord_exit)  { ev_until[EV_EXIT] = now + 1500000;  printf("{\"t\":\"input\",\"ev\":\"chord_exit\"}\n"); }
}

// ---------- drawing ----------
static inline void px(int x, int y, uint8_t c)
{
    if ((unsigned)x < LW && (unsigned)y < LH) fb[y * LW + x] = c;
}
static void fill(int x, int y, int w, int h, uint8_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LW) w = LW - x;
    if (y + h > LH) h = LH - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) memset(&fb[(y + j) * LW + x], c, w);
}
static void rect(int x, int y, int w, int h, uint8_t c)
{
    fill(x, y, w, 1, c); fill(x, y + h - 1, w, 1, c);
    fill(x, y, 1, h, c); fill(x + w - 1, y, 1, h, c);
}
static void line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static int text(const char *s, int x, int y, uint8_t c, int scale)
{
    for (; *s; s++) {
        const uint8_t *g = font_glyph(*s);
        for (int cx = 0; cx < 5; cx++)
            for (int cy = 0; cy < 7; cy++)
                if (g[cx] & (1 << cy)) fill(x + cx * scale, y + cy * scale, scale, scale, c);
        x += 6 * scale;
    }
    return x;
}
static int text_w(const char *s, int scale) { return (int)strlen(s) * 6 * scale - scale; }

// ---------- stats ----------
static struct {
    int frames, late;
    int64_t render_sum, flush_sum, flush_max, flush_min, bytes_sum;
} st;
static float hud_fps, hud_flush_ms;

// ---------- pages ----------
static void draw_geometry(void)
{
    char buf[48];
    memset(fb, C_BLACK, sizeof fb);
    rect(0, 0, LW, LH, C_WHITE);          // outermost pixel ring: must be fully visible
    rect(2, 2, LW - 4, LH - 4, C_YELLOW);
    text("TL", 6, 6, C_WHITE, 2);
    text("TR", LW - 6 - text_w("TR", 2), 6, C_WHITE, 2);
    text("BL", 6, LH - 6 - 14, C_WHITE, 2);
    text("BR", LW - 6 - text_w("BR", 2), LH - 6 - 14, C_WHITE, 2);
    // up arrow
    int ax = LW / 2;
    for (int t = -1; t <= 1; t++) {
        line(ax + t, 14, ax + t, 50, C_WHITE);
        line(ax + t, 14, ax - 14 + t, 28, C_WHITE);
        line(ax + t, 14, ax + 14 + t, 28, C_WHITE);
    }
    text("UP", ax - text_w("UP", 1) / 2, 54, C_WHITE, 1);
    // centre cross
    fill(LW / 2 - 12, LH / 2, 25, 1, C_RED);
    fill(LW / 2, LH / 2 - 12, 1, 25, C_GREEN);
    // colour bars with labels: if a label doesn't match its colour, colour order/inversion is wrong
    const uint8_t bars[] = { C_RED, C_GREEN, C_BLUE, C_WHITE, C_NAVY, C_CREAM, C_CORAL, C_TEAL, C_YELLOW };
    const char *lab[] = { "R", "G", "B", "W", "NV", "CR", "CO", "TE", "YE" };
    int bx = 34;
    for (int i = 0; i < 9; i++) {
        fill(bx + i * 28, 104, 24, 24, bars[i]);
        text(lab[i], bx + i * 28 + 12 - text_w(lab[i], 1) / 2, 131, C_WHITE, 1);
    }
    snprintf(buf, sizeof buf, "%dX%d %s MADCTL %02X", LW, LH, cfg.mode == MODE_HW ? "HW" : "SW",
             cfg.madctl[cfg.mode]);
    text(buf, LW / 2 - text_w(buf, 1) / 2, 74, C_CYAN, 1);
    if (cfg.mode == MODE_SW) {
        snprintf(buf, sizeof buf, "SWFLIP %d %d", cfg.sw_fx, cfg.sw_fy);
        text(buf, LW / 2 - text_w(buf, 1) / 2, 86, C_CYAN, 1);
    }
}

static void draw_page(int f)
{
    int v = cfg.speed;
    switch (cfg.page) {
    case PG_GEOMETRY: draw_geometry(); break;
    case PG_HLINE: {
        memset(fb, C_BLACK, sizeof fb);
        int y = (f * v) % LH;
        fill(0, y, LW, 3, C_WHITE);
        fill(0, (y + LH / 2) % LH, LW, 3, C_CORAL);   // second line half a screen away
        break;
    }
    case PG_VLINE: {
        memset(fb, C_BLACK, sizeof fb);
        int x = (f * v) % LW;
        fill(x, 0, 3, LH, C_WHITE);
        fill((x + LW / 2) % LW, 0, 3, LH, C_CORAL);
        break;
    }
    case PG_BOX: {
        memset(fb, C_NAVY, sizeof fb);
        int span_x = LW - 40, span_y = LH - 40;
        int tx = (f * v) % (2 * span_x), ty = (f * v) % (2 * span_y);
        int x = tx < span_x ? tx : 2 * span_x - tx;
        int y = ty < span_y ? ty : 2 * span_y - ty;
        fill(x, y, 40, 40, C_CREAM);
        rect(x, y, 40, 40, C_CORAL);
        break;
    }
    case PG_CHECKER: {
        int off = (f * v) % 32;
        for (int y = 0; y < LH; y++)
            for (int x = 0; x < LW; x++)
                fb[y * LW + x] = ((((x + off) >> 4) ^ (y >> 4)) & 1) ? C_TEAL : C_CREAM;
        break;
    }
    case PG_INPUT: {
        // Input bench: positions in fine steps (80 per turn), turns, speed, click state and events.
        // Pass test: N full turns clockwise then N back must return POS to exactly 0.
        int64_t now = esp_timer_get_time();
        memset(fb, C_NAVY, sizeof fb);
        for (int i = 0; i < 2; i++) {
            int x = i ? 168 : 8;
            char b[32];
            text(i ? "RIGHT" : "LEFT", x, 8, C_YELLOW, 2);
            snprintf(b, sizeof b, "POS %ld", (long)in_st.pos[i]);              text(b, x, 30, C_WHITE, 2);
            snprintf(b, sizeof b, "TURNS %.2f", in_st.pos[i] / (float)INPUT_TRANSITIONS_PER_TURN); text(b, x, 50, C_CYAN, 2);
            snprintf(b, sizeof b, "SPD %d/S", (int)in_st.speed[i]);            text(b, x, 70, C_WHITE, 1);
            fill(x, 86, 60, 24, in_st.down[i] ? C_CORAL : C_DGREY);
            text(in_st.down[i] ? "DOWN" : "UP", x + 8, 94, C_WHITE, 2);
            if (now < ev_until[EV_L_PRESS + i]) text("PRESS", x, 116, C_GREEN, 1);
            if (now < ev_until[EV_L_LONG + i])  text("LONG", x + 40, 116, C_GREEN, 1);
        }
        if (now < ev_until[EV_PAUSE]) text("CHORD: PAUSE", 8, 132, C_GREEN, 2);
        if (now < ev_until[EV_EXIT])  text("CHORD: FORCE EXIT", 8, 150, C_CORAL, 2);
        break;
    }
    case PG_SCROLL: {
        // Side-scroller stress test: three parallax layers (hills at 1/4, mid hills at 1/2, ground
        // and platforms at full speed) moving left by `speed` px/frame. Flat colours mean only the
        // shape edges change, so diff flush sends about a fifth of a full frame (use `diff 0` to force
        // the full 107 KB write as the worst case).
        memset(fb, C_CREAM, sizeof fb);
        int off = f * v;
        for (int x = 0; x < LW; x++) {
            int p = (x + off / 4) % 160; if (p > 80) p = 160 - p;
            int hh = 18 + p * 30 / 80;
            fill(x, 118 - hh, 1, hh, C_CYAN);
            int q = (x + off / 2 + 40) % 112; if (q > 56) q = 112 - q;
            int h2 = 10 + q * 24 / 56;
            fill(x, 122 - h2, 1, h2, C_TEAL);
        }
        for (int x = 0; x < LW; x++) {
            int gw = x + off;
            int seg = gw / 64;
            uint32_t h = (uint32_t)(seg + 1) * 2654435761u;
            if (((h >> 20) % 7) != 0) {
                int top = 112 + ((h >> 16) % 3) * 12;
                fill(x, top, 1, LH - top, C_NAVY);
                fill(x, top, 1, 3, C_GREEN);
            }
            int m = gw % 64;
            if (((h >> 8) % 4) == 1 && m >= 8 && m < 48) {
                int py = 60 + ((h >> 12) % 3) * 14;
                fill(x, py, 1, 6, C_NAVY);
                fill(x, py, 1, 2, C_GREEN);
                if (m >= 24 && m < 28) fill(x, py - 14, 1, 5, C_YELLOW);   // coin above the platform
            }
        }
        {   // player: coral block standing on the terrain under it
            int gw = 60 + off, seg = gw / 64;
            uint32_t h = (uint32_t)(seg + 1) * 2654435761u;
            int top = (((h >> 20) % 7) == 0) ? 112 : 112 + ((h >> 16) % 3) * 12;
            fill(60, top - 16, 14, 16, C_CORAL);
            fill(64, top - 12, 2, 3, C_WHITE);
            fill(69, top - 12, 2, 3, C_WHITE);
        }
        break;
    }
    case PG_FLASH: {
        // Whole screen flips black <-> white every 2 s. Film the flip in slow-motion at a slow
        // refresh (frctrl 1f): the front where new colour replaces old moves in the panel's scan direction.
        memset(fb, ((f / 60) & 1) ? C_WHITE : C_BLACK, sizeof fb);
        break;
    }
    case PG_RAIN: {
        // Game-like content: 24 falling blocks of mixed size, colour and speed (1..speed px/frame)
        // over a static background with a few resting blocks along the bottom. Stateless: every
        // drop's position is a function of the frame number, so runs are repeatable.
        static const uint8_t cols[] = { C_WHITE, C_CORAL, C_YELLOW, C_CYAN, C_GREEN, C_MAGENTA };
        memset(fb, C_NAVY, sizeof fb);
        for (int i = 0; i < 6; i++) fill(20 + i * 52, LH - 8, 30, 8, C_DGREY);
        int sp = v < 1 ? 1 : v;
        for (int i = 0; i < 24; i++) {
            uint32_t h = (uint32_t)(i + 1) * 2654435761u;
            int w = 6 + (h >> 24) % 11, hh = 6 + (h >> 16) % 11;
            int x = (h >> 4) % (LW - w);
            int spd = 1 + (h >> 12) % sp;
            int y = (int)((f * spd + (h >> 8) % (LH + hh)) % (LH + hh)) - hh;
            fill(x, y, w, hh, cols[i % 6]);
        }
        break;
    }
    case PG_STRIPES: {
        int off = f * v;
        for (int y = 0; y < LH; y++)
            for (int x = 0; x < LW; x++)
                fb[y * LW + x] = (((x + y + off) >> 3) & 1) ? C_YELLOW : C_NAVY;
        break;
    }
    }
    if (tune_on) {
        char l1[32], l2[32];
        snprintf(l1, sizeof l1, "FR %02X ~%dHZ", tune_fr, FR_HZ[tune_fr]);
        // estimate: nominal rate assumes the default 12+12 porch lines (344 lines per frame)
        snprintf(l2, sizeof l2, "PORCH %02X ~%dHZ", tune_fp, (int)(FR_HZ[tune_fr] * 344.f / (320 + 2 * tune_fp) + 0.5f));
        int w = text_w(l2, 2) + 8;
        fill(0, LH - 34, w, 34, C_BLACK);
        text(l1, 4, LH - 32, C_YELLOW, 2);
        text(l2, 4, LH - 16, C_CYAN, 2);
    } else if (cfg.hud && cfg.page != PG_GEOMETRY) {
        char buf[48];
        snprintf(buf, sizeof buf, "%s %dM %s %dFPS FL %.1fMS", cfg.mode == MODE_HW ? "HW" : "SW", cfg.mhz,
                 cfg.diff ? "DIFF" : "FULL", (int)(hud_fps + 0.5f), hud_flush_ms);
        fill(0, LH - 11, text_w(buf, 1) + 6, 11, C_BLACK);
        text(buf, 3, LH - 9, C_WHITE, 1);
    }
}

// ---------- flush ----------
// Panel geometry for the current mode: "columns" x "rows" as the controller addresses them.
static int pw, ph, col_off, row_off;

static void apply_geometry(void)
{
    if (cfg.mode == MODE_HW) { pw = LW; ph = LH; col_off = BOARD_HW_COL_OFF; row_off = BOARD_HW_ROW_OFF; }
    else                     { pw = LH; ph = LW; col_off = BOARD_SW_COL_OFF; row_off = BOARD_SW_ROW_OFF; }
}

// For panel row py: framebuffer index of panel pixel px is base + px * step.
static inline void row_map(int py, int *base, int *step)
{
    if (cfg.mode == MODE_HW) { *base = py * LW; *step = 1; return; }
    int lx = cfg.sw_fx ? (LW - 1 - py) : py;
    if (!cfg.sw_fy) { *base = lx; *step = LW; }
    else            { *base = (LH - 1) * LW + lx; *step = -LW; }
}

static int flush_full(void)
{
    lcd_set_window(col_off, row_off, col_off + pw - 1, row_off + ph - 1);
    int strip = cfg.strip;
    if (strip * pw > LCD_BUF_PIXELS) strip = LCD_BUF_PIXELS / pw;
    bool first = true;
    for (int py = 0; py < ph; py += strip) {
        int rows = ph - py < strip ? ph - py : strip;
        uint16_t *b = lcd_get_buf(), *o = b;
        for (int r = 0; r < rows; r++) {
            int base, step;
            row_map(py + r, &base, &step);
            for (int x = 0, i = base; x < pw; x++, i += step) *o++ = pal_be[fb[i]];
        }
        lcd_send(b, rows * pw, !first);
        first = false;
    }
    lcd_wait_idle();
    return pw * ph * 2;
}

static int flush_diff(void)
{
    int strip = cfg.strip;
    if (strip * pw > LCD_BUF_PIXELS) strip = LCD_BUF_PIXELS / pw;
    int bytes = 0;
    for (int py = 0; py < ph; py += strip) {
        int rows = ph - py < strip ? ph - py : strip;
        int x0 = pw, x1 = -1;
        for (int r = 0; r < rows; r++) {
            int base, step;
            row_map(py + r, &base, &step);
            for (int x = 0, i = base; x < pw; x++, i += step)
                if (fb[i] != prev[i]) { if (x < x0) x0 = x; if (x > x1) x1 = x; }
        }
        if (x1 < 0) continue;
        int w = x1 - x0 + 1;
        lcd_set_window(col_off + x0, row_off + py, col_off + x1, row_off + py + rows - 1);
        uint16_t *b = lcd_get_buf(), *o = b;
        for (int r = 0; r < rows; r++) {
            int base, step;
            row_map(py + r, &base, &step);
            for (int x = x0, i = base + x0 * step; x <= x1; x++, i += step) *o++ = pal_be[fb[i]];
        }
        lcd_send(b, rows * w, false);
        bytes += rows * w * 2;
    }
    lcd_wait_idle();
    return bytes;
}

// ---------- config / commands ----------
static void print_cfg(void)
{
    printf("{\"t\":\"cfg\",\"board\":\"%s\",\"page\":\"%s\",\"mode\":\"%s\",\"madctl\":\"0x%02X\","
           "\"sw_fx\":%d,\"sw_fy\":%d,\"mhz\":%d,\"fps\":%d,\"diff\":%d,\"strip\":%d,\"speed\":%d,"
           "\"inv\":%d,\"bl\":%d,\"hud\":%d,\"tune\":%d,\"frctrl\":\"0x%02X\",\"fp\":\"0x%02X\"}\n",
           BOARD_NAME, PAGE_NAMES[cfg.page], cfg.mode == MODE_HW ? "hw" : "sw", cfg.madctl[cfg.mode],
           cfg.sw_fx, cfg.sw_fy, cfg.mhz, cfg.fps, cfg.diff, cfg.strip, cfg.speed, cfg.inv, cfg.bl, cfg.hud, tune_on, tune_fr, tune_fp);
}

static void invalidate(void) { memset(prev, 0xFF, sizeof prev); }

static void send_madctl(void)
{
    uint8_t m = cfg.madctl[cfg.mode];
    lcd_cmd(0x36, &m, 1);
    apply_geometry();
    invalidate();
}

static void handle_cmd(char *line)
{
    char *cmd = strtok(line, " \t\r\n");
    if (!cmd) return;
    char *a = strtok(NULL, " \t\r\n");
    char *b = strtok(NULL, " \t\r\n");
    bool ok = true;

    if (!strcmp(cmd, "page") && a) {
        if (!strcmp(a, "next")) cfg.page = (cfg.page + 1) % PG_COUNT;
        else {
            int n = -1;
            for (int i = 0; i < PG_COUNT; i++) if (!strcmp(a, PAGE_NAMES[i])) n = i;
            if (n < 0) n = atoi(a);
            if (n >= 0 && n < PG_COUNT) cfg.page = n; else ok = false;
        }
        invalidate();
    } else if (!strcmp(cmd, "mode") && a) {
        cfg.mode = !strcmp(a, "hw") ? MODE_HW : MODE_SW;
        send_madctl();
    } else if (!strcmp(cmd, "madctl") && a) {
        cfg.madctl[cfg.mode] = (int)strtol(a, NULL, 16) & 0xFF;
        send_madctl();
    } else if (!strcmp(cmd, "swflip") && a && b) {
        cfg.sw_fx = atoi(a) ? 1 : 0; cfg.sw_fy = atoi(b) ? 1 : 0;
        invalidate();
    } else if (!strcmp(cmd, "mhz") && a) {
        int m = atoi(a);
        if (m < 1 || m > 80) ok = false;
        else { cfg.mhz = m; lcd_set_pclk(m * 1000000); invalidate(); }
    } else if (!strcmp(cmd, "fps") && a) {
        cfg.fps = atoi(a);
    } else if (!strcmp(cmd, "diff") && a) {
        cfg.diff = atoi(a) ? 1 : 0; invalidate();
    } else if (!strcmp(cmd, "strip") && a) {
        int s = atoi(a); cfg.strip = s < 1 ? 1 : s > 32 ? 32 : s;
    } else if (!strcmp(cmd, "speed") && a) {
        cfg.speed = atoi(a);
    } else if (!strcmp(cmd, "hud") && a) {
        cfg.hud = atoi(a) ? 1 : 0;
    } else if (!strcmp(cmd, "inv") && a) {
        cfg.inv = atoi(a) ? 1 : 0; lcd_cmd(cfg.inv ? 0x21 : 0x20, NULL, 0);
    } else if (!strcmp(cmd, "bl") && a) {
        cfg.bl = atoi(a); lcd_backlight(cfg.bl);
    } else if (!strcmp(cmd, "frctrl") && a) {
        uint8_t v = (uint8_t)strtol(a, NULL, 16);
        lcd_cmd(0xC6, &v, 1);   // FRCTRL2: 0x0F = 60 Hz default, lower = faster, 0x1F = 39 Hz
        if (v < 32) { tune_fr = v; applied_fr = v; }
        printf("{\"t\":\"note\",\"frctrl\":\"0x%02X\"}\n", v);
    } else if (!strcmp(cmd, "tune") && a) {
        tune_on = atoi(a) ? 1 : 0;
    } else if (!strcmp(cmd, "init") && a && !strcmp(a, "vendor")) {
        lcd_vendor_tuning();
        printf("{\"t\":\"note\",\"init\":\"vendor\"}\n");
        invalidate();
    } else if (!strcmp(cmd, "raw") && a) {
        // raw <cmd hex> [data hex bytes...] e.g. "raw b2 0c 0c 00 33 33" (porch control)
        uint8_t data[16]; int n = 0;
        uint8_t c = (uint8_t)strtol(a, NULL, 16);
        if (b) { data[n++] = (uint8_t)strtol(b, NULL, 16);
                 char *t; while (n < 16 && (t = strtok(NULL, " \t\r\n"))) data[n++] = (uint8_t)strtol(t, NULL, 16); }
        lcd_cmd(c, data, n);
        invalidate();
    } else if (!strcmp(cmd, "cfg") || !strcmp(cmd, "stats")) {
        // printed below
    } else if (!strcmp(cmd, "reboot")) {
        esp_restart();
    } else if (!strcmp(cmd, "help")) {
        printf("{\"t\":\"help\",\"cmds\":\"page <n|name|next> | mode hw|sw | madctl <hex> | swflip <fx> <fy> | "
               "mhz <n> | fps <n, 0=unlimited> | diff 0|1 | strip <1-32> | speed <px/frame> | hud 0|1 | "
               "inv 0|1 | bl <0-100> | frctrl <hex> | tune 0|1 | init vendor | raw <cmd> [bytes] | cfg | reboot\"}\n");
    } else {
        ok = false;
    }
    if (!ok) printf("{\"t\":\"error\",\"msg\":\"bad command\",\"cmd\":\"%s\"}\n", cmd);
    print_cfg();
}

void lab_submit_cmd(const char *line)
{
    char buf[96];
    strlcpy(buf, line, sizeof buf);
    xQueueSend(s_cmdq, buf, 0);
}

// ---------- main loop ----------
static void lab_task(void *arg)
{
    for (int i = 0; i < C_COUNT; i++) pal_be[i] = (PAL565[i] >> 8) | (PAL565[i] << 8);
    apply_geometry();
    send_madctl();
    lcd_backlight(cfg.bl);
    print_cfg();

    gpio_config_t btn = { .pin_bit_mask = 1ULL << BOARD_BOOT_BTN, .mode = GPIO_MODE_INPUT,
                          .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&btn);
    int btn_prev = 1;

    st.flush_min = INT64_MAX;
    int64_t next = esp_timer_get_time(), window_start = next;
    char line[96];

    for (int f = 0;; f++) {
        while (xQueueReceive(s_cmdq, line, 0) == pdTRUE) handle_cmd(line);
        apply_tune();
        input_frame(esp_timer_get_time());
        int bv = gpio_get_level(BOARD_BOOT_BTN);
        if (!bv && btn_prev) { cfg.page = (cfg.page + 1) % PG_COUNT; invalidate(); print_cfg(); }
        btn_prev = bv;

        int64_t t0 = esp_timer_get_time();
        draw_page(f);
        int64_t t1 = esp_timer_get_time();
        int bytes = cfg.diff ? flush_diff() : flush_full();
        int64_t t2 = esp_timer_get_time();
        memcpy(prev, fb, sizeof fb);

        int64_t fl = t2 - t1;
        st.frames++;
        st.render_sum += t1 - t0;
        st.flush_sum += fl;
        st.bytes_sum += bytes;
        if (fl > st.flush_max) st.flush_max = fl;
        if (fl < st.flush_min) st.flush_min = fl;

        int64_t now = esp_timer_get_time();
        if (now - window_start >= 1000000) {
            float secs = (now - window_start) / 1e6f;
            hud_fps = st.frames / secs;
            hud_flush_ms = st.flush_sum / (float)st.frames / 1000.f;
            printf("{\"t\":\"stats\",\"page\":\"%s\",\"mode\":\"%s\",\"madctl\":\"0x%02X\",\"mhz\":%d,"
                   "\"diff\":%d,\"strip\":%d,\"fps_target\":%d,\"fps\":%.1f,\"late\":%d,\"render_us\":%lld,"
                   "\"flush_us_min\":%lld,\"flush_us_avg\":%lld,\"flush_us_max\":%lld,\"kb_per_frame\":%.1f,"
                   "\"heap_int\":%u}\n",
                   PAGE_NAMES[cfg.page], cfg.mode == MODE_HW ? "hw" : "sw", cfg.madctl[cfg.mode], cfg.mhz,
                   cfg.diff, cfg.strip, cfg.fps, hud_fps, st.late, st.render_sum / st.frames, st.flush_min,
                   st.flush_sum / st.frames, st.flush_max, st.bytes_sum / (float)st.frames / 1024.f,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            memset(&st, 0, sizeof st);
            st.flush_min = INT64_MAX;
            window_start = now;
        }

        if (cfg.fps > 0) {
            int64_t period = 1000000 / cfg.fps;
            next += period;
            now = esp_timer_get_time();
            if (now > next) { st.late++; next = now; vTaskDelay(1); }
            else {
                while (next - esp_timer_get_time() > 1500) vTaskDelay(1);
                while (esp_timer_get_time() < next) { }
            }
        } else {
            next = esp_timer_get_time();
            vTaskDelay(1);
        }
    }
}

void lab_start(void)
{
    s_cmdq = xQueueCreate(8, 96);
    if (cfg.mhz != 40) lcd_set_pclk(cfg.mhz * 1000000);
    if (input_init() != ESP_OK) printf("{\"t\":\"error\",\"msg\":\"input_init failed\"}\n");
    xTaskCreatePinnedToCore(lab_task, "lab", 8192, NULL, 5, NULL, 1);
}
