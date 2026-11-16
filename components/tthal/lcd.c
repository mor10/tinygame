// Minimal ST7789 transport: raw commands + pipelined DMA strips over esp_lcd's SPI panel IO.
// We deliberately skip esp_lcd's panel driver so MADCTL, offsets and windows stay fully under our control.
#include "lcd.h"
#include "board.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define NBUF 3

static const char *TAG = "lcd";
static esp_lcd_panel_io_handle_t s_io;
static uint16_t *s_bufs[NBUF];
static int s_next;
static SemaphoreHandle_t s_free;
static int s_pclk;

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_free, &woken);
    return woken == pdTRUE;
}

static esp_err_t make_io(int pclk_hz)
{
    esp_lcd_panel_io_spi_config_t cfg = {
        .dc_gpio_num = BOARD_LCD_DC,
        .cs_gpio_num = BOARD_LCD_CS,
        .pclk_hz = pclk_hz,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = BOARD_LCD_SPI_MODE,
        .trans_queue_depth = NBUF + 2,
        .on_color_trans_done = on_color_done,
    };
    s_pclk = pclk_hz;
    return esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST, &cfg, &s_io);
}

void lcd_cmd(uint8_t cmd, const uint8_t *data, int len)
{
    esp_lcd_panel_io_tx_param(s_io, cmd, data, len);
}

static void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BOARD_BL_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);
    ledc_channel_config_t c = {
        .gpio_num = BOARD_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ledc_channel_config(&c);
}

void lcd_backlight(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, percent * 1023 / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}


// Generic ST7789 reference tuning (porch, gate, VCOM, power, frame rate, gamma).
// Without it the panel runs on power-on defaults, which showed more tearing in the screen lab.
void lcd_vendor_tuning(void)
{
    lcd_cmd(0xB2, (uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
    lcd_cmd(0xB7, (uint8_t[]){0x35}, 1);
    lcd_cmd(0xBB, (uint8_t[]){0x19}, 1);
    lcd_cmd(0xC0, (uint8_t[]){0x2C}, 1);
    lcd_cmd(0xC2, (uint8_t[]){0x01}, 1);
    lcd_cmd(0xC3, (uint8_t[]){0x12}, 1);
    lcd_cmd(0xC4, (uint8_t[]){0x20}, 1);
    lcd_cmd(0xC6, (uint8_t[]){0x0F}, 1);
    lcd_cmd(0xD0, (uint8_t[]){0xA4, 0xA1}, 2);
    lcd_cmd(0xE0, (uint8_t[]){0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}, 14);
    lcd_cmd(0xE1, (uint8_t[]){0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}, 14);
}

esp_err_t lcd_init(int pclk_hz, uint8_t madctl, bool invert)
{
    backlight_init();

    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_BUF_PIXELS * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "bus");
    ESP_RETURN_ON_ERROR(make_io(pclk_hz), TAG, "io");

    s_free = xSemaphoreCreateCounting(NBUF, NBUF);
    for (int i = 0; i < NBUF; i++) {
        s_bufs[i] = heap_caps_malloc(LCD_BUF_PIXELS * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ESP_RETURN_ON_FALSE(s_bufs[i], ESP_ERR_NO_MEM, TAG, "dma buf");
    }

    gpio_config_t rst = { .pin_bit_mask = 1ULL << BOARD_LCD_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst);
    gpio_set_level(BOARD_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(BOARD_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_cmd(0x01, NULL, 0);                         // SWRESET
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(0x11, NULL, 0);                         // SLPOUT
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_cmd(0x3A, (uint8_t[]){0x55}, 1);            // COLMOD: 16-bit RGB565
    lcd_cmd(0x36, &madctl, 1);                      // MADCTL
    lcd_cmd(invert ? 0x21 : 0x20, NULL, 0);         // INVON / INVOFF
    lcd_cmd(0x13, NULL, 0);                         // NORON
#if BOARD_VENDOR_INIT
    lcd_vendor_tuning();
#endif
    lcd_cmd(0x29, NULL, 0);                         // DISPON
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

void lcd_wait_idle(void)
{
    for (int i = 0; i < NBUF; i++) xSemaphoreTake(s_free, portMAX_DELAY);
    for (int i = 0; i < NBUF; i++) xSemaphoreGive(s_free);
}

esp_err_t lcd_set_pclk(int pclk_hz)
{
    lcd_wait_idle();
    esp_lcd_panel_io_del(s_io);
    return make_io(pclk_hz);
}

int lcd_get_pclk(void) { return s_pclk; }

void lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t c[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t r[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    lcd_cmd(0x2A, c, 4);  // CASET
    lcd_cmd(0x2B, r, 4);  // RASET
}

uint16_t *lcd_get_buf(void)
{
    xSemaphoreTake(s_free, portMAX_DELAY);
    uint16_t *b = s_bufs[s_next];
    s_next = (s_next + 1) % NBUF;   // transactions complete in order, so round-robin is safe
    return b;
}

void lcd_send(uint16_t *buf, int pixels, bool cont)
{
    esp_lcd_panel_io_tx_color(s_io, cont ? 0x3C : 0x2C, buf, pixels * 2);
}

// Display off and the controller asleep, ahead of deep sleep. The next boot's lcd_init resets the panel.
void lcd_sleep(void)
{
    lcd_wait_idle();
    lcd_cmd(0x28, NULL, 0);   // DISPOFF
    lcd_cmd(0x10, NULL, 0);   // SLPIN
    vTaskDelay(pdMS_TO_TICKS(120));
}
