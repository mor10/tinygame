// TinyTwist console: boots the display and input, runs Lua carts, and takes dev commands over
// USB-Serial-JTAG. Commands (one per line):
//   cart <bytes> [id] then exactly <bytes> of Lua source: stored as /carts/<id>.lua (default
//                  "uploaded") and played (max 128 KB)
//   play <id>      launch a stored or built-in cart
//   rm <id>        delete a stored cart
//   ls             list the gallery
//   gamma <r> <g> <b> per-channel colour correction exponents (1.0 = none, lower is brighter)
//   saveflush <ms> write saves at most this often (default 1000; 0 = every frame, for tests)
//   restart        relaunch the last game
//   hash <n> [seed] rerun the current cart deterministically for n frames, printing frame hashes
//   reboot
// Output is JSON lines: boot, cart, stats (1/s), log, error.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "display.h"
#include "gfx.h"
#include "vm.h"
#include "save.h"
#include "game.h"
#include "input.h"
#include "imu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"

#define CART_MAX (128 * 1024)

static int read_byte(void)
{
    uint8_t ch;
    while (usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY) != 1) { }
    return ch;
}

static void receive_cart(long len, const char *id)
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
        game_load(buf, len, id);
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
        if (!strncmp(line, "cart ", 5)) {
            char *end;
            long len = strtol(line + 5, &end, 10);
            while (*end == ' ') end++;
            receive_cart(len, *end ? end : "uploaded");
        }
        else if (!strncmp(line, "play ", 5)) game_play(line + 5);
        else if (!strncmp(line, "rm ", 3)) game_delete(line + 3);
        else if (!strcmp(line, "ls")) game_list();
        else if (!strncmp(line, "saveflush ", 10)) save_set_flush_interval(atol(line + 10) * 1000LL);
        else if (!strncmp(line, "gamma ", 6)) {
            float r, g, b;
            if (sscanf(line + 6, "%f %f %f", &r, &g, &b) == 3 && r > 0.2f && g > 0.2f && b > 0.2f && r < 3 && g < 3 && b < 3)
                display_set_gamma(r, g, b);
        }
        else if (!strcmp(line, "restart")) game_restart();
        else if (!strncmp(line, "hash ", 5)) {
            char *end;
            long frames = strtol(line + 5, &end, 10);
            game_hash(frames > 0 ? (int)frames : 300, (uint32_t)strtoul(end, NULL, 10));
        }
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
    if (imu_init() != ESP_OK) printf("{\"t\":\"error\",\"kind\":\"boot\",\"msg\":\"imu_init failed\"}\n");
    if (input_init() != ESP_OK) printf("{\"t\":\"error\",\"kind\":\"boot\",\"msg\":\"input_init failed\"}\n");
    display_present();

    // Safe mode: right knob held down at power-on. The shell then ignores saved settings.
    vTaskDelay(pdMS_TO_TICKS(30));   // let the click debounce settle
    input_state_t boot_in;
    input_snapshot(&boot_in);
    int safe = boot_in.down[KNOB_RIGHT];
    if (safe) printf("{\"t\":\"boot\",\"safe_mode\":1}\n");

    game_start(safe);
    xTaskCreatePinnedToCore(console_task, "console", 4096, NULL, 3, NULL, 0);
}
