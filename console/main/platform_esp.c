// platform.h on the board: ESP timer, the display back buffer, and internal/PSRAM heaps.
#include "platform.h"
#include "display.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "lcd.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

int64_t  plat_time_us(void) { return esp_timer_get_time(); }
uint8_t *plat_framebuffer(void) { return display_backbuffer(); }
void     plat_set_color(int index, uint32_t rgb) { display_set_color(index, rgb); }

void *plat_lua_realloc(void *ptr, size_t nsize, int prefer_fast)
{
    uint32_t caps = (prefer_fast ? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM) | MALLOC_CAP_8BIT;
    void *p = ptr ? heap_caps_realloc(ptr, nsize, caps) : heap_caps_malloc(nsize, caps);
    if (!p && prefer_fast) {
        caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        p = ptr ? heap_caps_realloc(ptr, nsize, caps) : heap_caps_malloc(nsize, caps);
    }
    return p;
}

void plat_lua_free(void *ptr) { heap_caps_free(ptr); }
int  plat_is_fast(const void *ptr) { return esp_ptr_internal(ptr); }

void *plat_asset_alloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void  plat_asset_free(void *ptr) { heap_caps_free(ptr); }

void plat_backlight(int percent) { display_backlight(percent); }
void plat_set_gamma(float r, float g, float b) { display_set_gamma(r, g, b); }
void plat_get_gamma(float *r, float *g, float *b) { display_get_gamma(r, g, b); }

// ---------- power: battery, USB, deep sleep ----------
int plat_battery_mv(void)
{
    static adc_oneshot_unit_handle_t adc;
    static adc_cali_handle_t cali;
    static adc_channel_t chan;
    static int state;   // 0 = untried, 1 = ready, -1 = unavailable
    if (!state) {
        adc_unit_t unit;
        adc_oneshot_unit_init_cfg_t uc = { .unit_id = ADC_UNIT_1 };
        adc_oneshot_chan_cfg_t cc = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
        adc_cali_curve_fitting_config_t kc = { .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
        state = adc_oneshot_io_to_channel(BOARD_BAT_ADC_GPIO, &unit, &chan) == ESP_OK && unit == ADC_UNIT_1
                && adc_oneshot_new_unit(&uc, &adc) == ESP_OK
                && adc_oneshot_config_channel(adc, chan, &cc) == ESP_OK
                && adc_cali_create_scheme_curve_fitting(&kc, &cali) == ESP_OK ? 1 : -1;
    }
    if (state != 1) return -1;
    int v[9], n = 0;
    for (int i = 0; i < 9; i++) {
        int mv;
        if (adc_oneshot_read(adc, chan, &mv) != ESP_OK || adc_cali_raw_to_voltage(cali, mv, &mv) != ESP_OK) continue;
        v[n++] = mv;
    }
    if (n == 0) return -1;
    for (int i = 1; i < n; i++)   // insertion sort, then the median: one noisy sample can't move the reading
        for (int j = i; j > 0 && v[j - 1] > v[j]; j--) { int t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    return v[n / 2] * BOARD_BAT_DIV;
}

int plat_usb_powered(void)
{
#if BOARD_USB_SENSE_GPIO >= 0
    return gpio_get_level(BOARD_USB_SENSE_GPIO);
#else
    return usb_serial_jtag_is_connected();
#endif
}

// The game to resume survives deep sleep in RTC memory.
static RTC_DATA_ATTR char resume_id[32];

int plat_woke_from_sleep(void) { return esp_reset_reason() == ESP_RST_DEEPSLEEP; }
const char *plat_wake_resume_id(void) { return plat_woke_from_sleep() ? resume_id : ""; }

void plat_release_sleep_holds(void)   // run first thing after a wake: the backlight pin was held off
{
    gpio_hold_dis(BOARD_LCD_BL);
    gpio_deep_sleep_hold_dis();
}

void plat_deep_sleep(const char *resume)
{
    snprintf(resume_id, sizeof resume_id, "%s", resume ? resume : "");
    display_backlight(0);
    lcd_sleep();
    gpio_set_level(BOARD_LCD_BL, 0);
    gpio_hold_en(BOARD_LCD_BL);        // keep the backlight off while the chip sleeps
    gpio_deep_sleep_hold_en();
    // The clicks wake it, but a click still held from the gesture would wake it at once: wait for release.
    for (int i = 0, up = 0; up < 5 && i < 1000; i++) {   // up to 10 s, then sleep anyway
        up = gpio_get_level(BOARD_ENC_L_CLICK) && gpio_get_level(BOARD_ENC_R_CLICK) ? up + 1 : 0;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const int pins[2] = { BOARD_ENC_L_CLICK, BOARD_ENC_R_CLICK };
    uint64_t mask = 0;
    for (int i = 0; i < 2; i++) {
        rtc_gpio_init(pins[i]);
        rtc_gpio_set_direction(pins[i], RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pulldown_dis(pins[i]);
        rtc_gpio_pullup_en(pins[i]);   // the clicks are active low, so they need their pull-ups while asleep
        mask |= 1ULL << pins[i];
    }
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    printf("{\"t\":\"sleep\",\"resume\":\"%s\"}\n", resume_id);
    fflush(stdout);
    esp_deep_sleep_start();
}

static int nvs_ready(void)
{
    static int state;   // 0 = untried, 1 = ok, -1 = failed
    if (!state) {
        esp_err_t e = nvs_flash_init();
        if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            e = nvs_flash_init();
        }
        state = e == ESP_OK ? 1 : -1;
    }
    return state == 1;
}

int plat_settings_load(void *buf, size_t n)
{
    nvs_handle_t h;
    if (!nvs_ready() || nvs_open("tinygame", NVS_READONLY, &h) != ESP_OK) return -1;
    size_t got = n;
    esp_err_t e = nvs_get_blob(h, "settings", buf, &got);
    nvs_close(h);
    return e == ESP_OK && got == n ? 0 : -1;
}

void plat_settings_save(const void *buf, size_t n)
{
    nvs_handle_t h;
    if (!nvs_ready() || nvs_open("tinygame", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "settings", buf, n);
    nvs_commit(h);
    nvs_close(h);
}

// ---------- storage: LittleFS volumes /carts and /saves ----------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_littlefs.h"

static int mount(const char *label, const char *path)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = path, .partition_label = label, .format_if_mount_failed = true, .dont_mount = false,
    };
    esp_err_t e = esp_vfs_littlefs_register(&conf);
    size_t total = 0, used = 0;
    esp_littlefs_info(label, &total, &used);
    printf("{\"t\":\"storage\",\"volume\":\"%s\",\"ok\":%d,\"kb_total\":%u,\"kb_used\":%u}\n",
           label, e == ESP_OK, (unsigned)(total / 1024), (unsigned)(used / 1024));
    return e == ESP_OK ? 0 : -1;
}

void plat_storage_init(void)
{
    mount("carts", "/carts");
    mount("saves", "/saves");
}

static int cmp_ids(const void *a, const void *b) { return strcmp(a, b); }

int plat_cart_list(char ids[][32], int max)
{
    DIR *d = opendir("/carts");
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        size_t l = strlen(e->d_name);
        if (l < 5 || l > 35 || strcmp(e->d_name + l - 4, ".lua")) continue;
        memcpy(ids[n], e->d_name, l - 4);
        ids[n][l - 4] = 0;
        n++;
    }
    closedir(d);
    qsort(ids, n, 32, cmp_ids);
    return n;
}

static char *read_all(const char *path, size_t *len, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (n >= 0 && (size_t)n <= max) ? plat_asset_alloc(n + 1) : NULL;
    if (buf && fread(buf, 1, n, f) != (size_t)n) { plat_asset_free(buf); buf = NULL; }
    fclose(f);
    if (buf) { buf[n] = 0; *len = (size_t)n; }
    return buf;
}

// Writes to <path>.tmp, then renames over <path>: LittleFS renames atomically, so a power cut
// leaves the old file or the new one.
static int write_atomic(const char *path, const void *buf, size_t n)
{
    char tmp[64];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(buf, 1, n, f) == n;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return -1; }
    return rename(tmp, path) == 0 ? 0 : -1;
}

char *plat_cart_load(const char *id, size_t *len)
{
    char path[48];
    snprintf(path, sizeof path, "/carts/%s.lua", id);
    return read_all(path, len, 128 * 1024);
}

int plat_cart_store(const char *id, const char *src, size_t len)
{
    char path[48];
    snprintf(path, sizeof path, "/carts/%s.lua", id);
    return write_atomic(path, src, len);
}

int plat_cart_delete(const char *id)
{
    char path[48];
    snprintf(path, sizeof path, "/carts/%s.lua", id);
    return remove(path) == 0 ? 0 : -1;
}

int plat_save_read(const char *id, void *buf, size_t max)
{
    char path[48];
    snprintf(path, sizeof path, "/saves/%s.sav", id);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, max, f);
    fclose(f);
    return (int)n;
}

int plat_save_write(const char *id, const void *buf, size_t n)
{
    char path[48];
    snprintf(path, sizeof path, "/saves/%s.sav", id);
    return write_atomic(path, buf, n);
}
