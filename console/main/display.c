// Display pipeline: triple 8-bit buffers (back, front, last-sent), flush task on core 0.
#include <string.h>
#include "display.h"
#include "board.h"
#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#define LW SCREEN_W
#define LH SCREEN_H
// Native portrait scan (MADCTL 0x00), rotated in software with swflip 0 1 from the lab:
// panel row py is logical column x = py, read bottom-to-top.
#define PW LH
#define PH LW
#define STRIP 16

static uint8_t buf_a[LW * LH], buf_b[LW * LH], sent[LW * LH];
static uint8_t *back = buf_a, *front = buf_b;
static uint16_t pal_be[256];
static SemaphoreHandle_t frame_ready, flush_done;
static display_stats_t stats;
static portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;

static inline int row_base(int py) { return (LH - 1) * LW + py; }   // step is -LW

static int flush_diff(void)
{
    int bytes = 0;
    for (int py = 0; py < PH; py += STRIP) {
        int rows = PH - py < STRIP ? PH - py : STRIP;
        int x0 = PW, x1 = -1;
        for (int r = 0; r < rows; r++) {
            int i = row_base(py + r);
            for (int x = 0; x < PW; x++, i -= LW)
                if (front[i] != sent[i]) { if (x < x0) x0 = x; if (x > x1) x1 = x; }
        }
        if (x1 < 0) continue;
        int w = x1 - x0 + 1;
        lcd_set_window(BOARD_SW_COL_OFF + x0, BOARD_SW_ROW_OFF + py,
                       BOARD_SW_COL_OFF + x1, BOARD_SW_ROW_OFF + py + rows - 1);
        uint16_t *b = lcd_get_buf(), *o = b;
        for (int r = 0; r < rows; r++) {
            int i = row_base(py + r) - x0 * LW;
            for (int x = x0; x <= x1; x++, i -= LW) *o++ = pal_be[front[i]];
        }
        lcd_send(b, rows * w, false);
        bytes += rows * w * 2;
    }
    lcd_wait_idle();
    memcpy(sent, front, sizeof sent);
    return bytes;
}

static void flush_task(void *arg)
{
    for (;;) {
        xSemaphoreTake(frame_ready, portMAX_DELAY);
        int64_t t0 = esp_timer_get_time();
        int bytes = flush_diff();
        uint32_t us = (uint32_t)(esp_timer_get_time() - t0);
        portENTER_CRITICAL(&stats_lock);
        stats.flushes++;
        stats.flush_us_sum += us;
        stats.bytes_sum += bytes;
        if (us > stats.flush_us_max) stats.flush_us_max = us;
        portEXIT_CRITICAL(&stats_lock);
        xSemaphoreGive(flush_done);
    }
}

void display_set_color(int index, uint32_t rgb)
{
    if (index < 0 || index > 255) return;
    uint16_t c = ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
    pal_be[index] = (uint16_t)((c >> 8) | (c << 8));
}

void display_init(void)
{
    lcd_init(80 * 1000000, BOARD_SW_MADCTL, BOARD_LCD_INVERT);   // includes the vendor tuning
    lcd_cmd(0xC6, (uint8_t[]){ 0x00 }, 1);                       // FRCTRL2: fastest refresh (lab winner)
    lcd_cmd(0xB2, (uint8_t[]){ 0x01, 0x01, 0x00, 0x33, 0x33 }, 5); // porch 1 line each side
    memset(buf_a, 0, sizeof buf_a);
    memset(buf_b, 0, sizeof buf_b);
    memset(sent, 0xFF, sizeof sent);   // forces a full first push
    frame_ready = xSemaphoreCreateBinary();
    flush_done = xSemaphoreCreateBinary();
    xSemaphoreGive(flush_done);
    xTaskCreatePinnedToCore(flush_task, "flush", 4096, NULL, 7, NULL, 0);
}

uint8_t *display_backbuffer(void) { return back; }

void display_present(void)
{
    xSemaphoreTake(flush_done, portMAX_DELAY);   // the flush task is done with front
    uint8_t *t = front; front = back; back = t;
    xSemaphoreGive(frame_ready);
    memcpy(back, front, LW * LH);                // drawing continues from the frame just shown
}

void display_backlight(int percent) { lcd_backlight(percent); }

void display_take_stats(display_stats_t *out)
{
    portENTER_CRITICAL(&stats_lock);
    *out = stats;
    memset(&stats, 0, sizeof stats);
    portEXIT_CRITICAL(&stats_lock);
}
