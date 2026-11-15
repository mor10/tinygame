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
void plat_set_gamma(float r, float g, float b) { display_set_gamma(r, g, b); }
void plat_get_gamma(float *r, float *g, float *b) { display_get_gamma(r, g, b); }

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
