// platform.h on the board: ESP timer, the display back buffer, and internal/PSRAM heaps.
#include "platform.h"
#include "display.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"

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
