// platform.h on the board: ESP timer, the display back buffer, and internal/PSRAM heaps.
#include "platform.h"
#include "display.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "nvs_flash.h"
#include "nvs.h"

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
