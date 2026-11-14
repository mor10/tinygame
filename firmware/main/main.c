// TinyTwist Screen Lab entry point: brings up the panel, starts the lab, reads serial commands.
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

void lab_start(void);
void lab_submit_cmd(const char *line);

static void console_task(void *arg)
{
    char line[96];
    int n = 0;
    uint8_t ch;
    for (;;) {
        if (usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY) != 1) continue;
        if (ch == '\n' || ch == '\r') {
            if (n) { line[n] = 0; lab_submit_cmd(line); n = 0; }
        } else if (n < (int)sizeof line - 1) {
            line[n++] = (char)ch;
        }
    }
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t ucfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&ucfg);
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdout, NULL, _IOLBF, 0);

    printf("{\"t\":\"boot\",\"fw\":\"screenlab 0.1\",\"board\":\"%s\",\"idf\":\"%s\",\"reset\":%d,"
           "\"psram\":%u,\"heap_int\":%u}\n",
           BOARD_NAME, esp_get_idf_version(), (int)esp_reset_reason(),
           (unsigned)esp_psram_get_size(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    if (lcd_init(40 * 1000000, BOARD_SW_MADCTL, BOARD_LCD_INVERT) != ESP_OK) {
        printf("{\"t\":\"error\",\"msg\":\"lcd_init failed\"}\n");
        return;
    }
    lab_start();
    xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 4, NULL, 0);
}
