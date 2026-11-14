// TinyTwist console: boots the display and input, runs Lua carts, and takes dev commands over
// USB-Serial-JTAG. Commands (one per line):
//   cart <bytes>   then exactly <bytes> of Lua source: load and run it (max 128 KB)
//   builtin        run the cart compiled into the firmware
//   restart        rerun the current cart
//   reboot
// Output is JSON lines: boot, cart, stats (1/s), log, error.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "display.h"
#include "gfx.h"
#include "game.h"
#include "input.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"

#define CART_MAX (128 * 1024)

extern const char hello_lua_start[] asm("_binary_hello_lua_start");
extern const char hello_lua_end[] asm("_binary_hello_lua_end");

static void load_builtin(void)
{
    game_load(hello_lua_start, (size_t)(hello_lua_end - hello_lua_start) - 1, "hello");   // -1: embedded NUL
}

static int read_byte(void)
{
    uint8_t ch;
    while (usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY) != 1) { }
    return ch;
}

static void receive_cart(long len)
{
    if (len <= 0 || len > CART_MAX) {
        printf("{\"t\":\"error\",\"kind\":\"upload\",\"msg\":\"cart must be 1-%d bytes\"}\n", CART_MAX);
        return;
    }
    char *buf = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { printf("{\"t\":\"error\",\"kind\":\"upload\",\"msg\":\"no memory\"}\n"); return; }
    long got = 0;
    while (got < len) {
        int n = usb_serial_jtag_read_bytes(buf + got, len - got, pdMS_TO_TICKS(2000));
        if (n <= 0) break;
        got += n;
    }
    if (got != len) {
        printf("{\"t\":\"error\",\"kind\":\"upload\",\"msg\":\"timed out after %ld of %ld bytes\"}\n", got, len);
    } else {
        printf("{\"t\":\"upload\",\"bytes\":%ld}\n", len);
        game_load(buf, len, "upload");
    }
    free(buf);
}

static void console_task(void *arg)
{
    char line[96];
    int n = 0;
    for (;;) {
        int ch = read_byte();
        if (ch != '\n' && ch != '\r') { if (n < (int)sizeof line - 1) line[n++] = (char)ch; continue; }
        if (!n) continue;
        line[n] = 0; n = 0;
        if (!strncmp(line, "cart ", 5)) receive_cart(atol(line + 5));
        else if (!strcmp(line, "builtin")) load_builtin();
        else if (!strcmp(line, "restart")) game_restart();
        else if (!strcmp(line, "reboot")) esp_restart();
        else printf("{\"t\":\"error\",\"kind\":\"command\",\"msg\":\"unknown command\"}\n");
    }
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t ucfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ucfg.rx_buffer_size = 4096;
    usb_serial_jtag_driver_install(&ucfg);
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("{\"t\":\"boot\",\"fw\":\"console 0.1\",\"board\":\"%s\",\"idf\":\"%s\",\"reset\":%d,\"heap_int\":%u}\n",
           BOARD_NAME, esp_get_idf_version(), (int)esp_reset_reason(),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    display_init();
    gfx_init_palette();
    if (input_init() != ESP_OK) printf("{\"t\":\"error\",\"kind\":\"boot\",\"msg\":\"input_init failed\"}\n");
    display_present();
    display_backlight(100);

    game_start();
    load_builtin();
    xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 3, NULL, 0);
}
