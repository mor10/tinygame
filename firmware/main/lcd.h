#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define LCD_BUF_PIXELS (320 * 32)   // one DMA strip buffer: up to 32 lines of 320 px

esp_err_t lcd_init(int pclk_hz, uint8_t madctl, bool invert);
void      lcd_vendor_tuning(void);
esp_err_t lcd_set_pclk(int pclk_hz);
int       lcd_get_pclk(void);
void      lcd_cmd(uint8_t cmd, const uint8_t *data, int len);
void      lcd_set_window(int x0, int y0, int x1, int y1);   // inclusive, raw controller addresses
uint16_t *lcd_get_buf(void);                                 // blocks until a DMA strip buffer is free
void      lcd_send(uint16_t *buf, int pixels, bool cont);    // queue RAMWR (or RAMWRC when cont)
void      lcd_wait_idle(void);                               // wait for all queued strips
void      lcd_backlight(int percent);
