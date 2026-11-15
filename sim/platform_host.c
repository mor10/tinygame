// platform.h for the simulator: a plain framebuffer, malloc, and the host clock.
#include <stdlib.h>
#include <time.h>
#include "platform.h"
#include "sim.h"

uint8_t  sim_fb[SCREEN_W * SCREEN_H];
uint32_t sim_palette[256];

int64_t plat_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

uint8_t *plat_framebuffer(void) { return sim_fb; }
void     plat_set_color(int index, uint32_t rgb) { if (index >= 0 && index < 256) sim_palette[index] = rgb; }

void *plat_lua_realloc(void *ptr, size_t nsize, int prefer_fast) { (void)prefer_fast; return realloc(ptr, nsize); }
void  plat_lua_free(void *ptr) { free(ptr); }
int   plat_is_fast(const void *ptr) { (void)ptr; return 0; }

void *plat_asset_alloc(size_t n) { return malloc(n); }
void  plat_asset_free(void *ptr) { free(ptr); }

int  sim_backlight = 100;
void plat_backlight(int percent) { sim_backlight = percent; }
int  plat_settings_load(void *buf, size_t n) { (void)buf; (void)n; return -1; }
void plat_settings_save(const void *buf, size_t n) { (void)buf; (void)n; }

// ---------- storage ----------
// With --storage DIR, carts live in DIR/carts/<id>.lua and saves in DIR/saves/<id>.sav (written to
// a temp file, then renamed). Without it, saves are kept in memory for the run and nothing is stored.
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

const char *sim_storage_dir;

typedef struct { char id[32]; char *buf; size_t n; } mem_save_t;
static mem_save_t mem_saves[32];

void plat_storage_init(void)
{
    if (!sim_storage_dir) return;
    char p[512];
    mkdir(sim_storage_dir, 0755);
    snprintf(p, sizeof p, "%s/carts", sim_storage_dir); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/saves", sim_storage_dir); mkdir(p, 0755);
}

static int cmp_ids(const void *a, const void *b) { return strcmp(a, b); }

int plat_cart_list(char ids[][32], int max)
{
    if (!sim_storage_dir) return 0;
    char p[512];
    snprintf(p, sizeof p, "%s/carts", sim_storage_dir);
    DIR *d = opendir(p);
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

static int write_atomic(const char *path, const void *buf, size_t n)
{
    char tmp[600];
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
    if (!sim_storage_dir) return NULL;
    char p[512];
    snprintf(p, sizeof p, "%s/carts/%s.lua", sim_storage_dir, id);
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = n >= 0 ? malloc(n + 1) : NULL;
    if (buf && fread(buf, 1, n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) { buf[n] = 0; *len = (size_t)n; }
    return buf;
}

int plat_cart_store(const char *id, const char *src, size_t len)
{
    if (!sim_storage_dir) return -1;
    char p[512];
    snprintf(p, sizeof p, "%s/carts/%s.lua", sim_storage_dir, id);
    return write_atomic(p, src, len);
}

int plat_cart_delete(const char *id)
{
    if (!sim_storage_dir) return -1;
    char p[512];
    snprintf(p, sizeof p, "%s/carts/%s.lua", sim_storage_dir, id);
    return remove(p) == 0 ? 0 : -1;
}

int plat_save_read(const char *id, void *buf, size_t max)
{
    if (!sim_storage_dir) {
        for (int i = 0; i < 32; i++)
            if (mem_saves[i].buf && !strcmp(mem_saves[i].id, id)) {
                size_t n = mem_saves[i].n < max ? mem_saves[i].n : max;
                memcpy(buf, mem_saves[i].buf, n);
                return (int)n;
            }
        return -1;
    }
    char p[512];
    snprintf(p, sizeof p, "%s/saves/%s.sav", sim_storage_dir, id);
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, max, f);
    fclose(f);
    return (int)n;
}

int plat_save_write(const char *id, const void *buf, size_t n)
{
    if (!sim_storage_dir) {
        int slot = -1;
        for (int i = 0; i < 32; i++) {
            if (mem_saves[i].buf && !strcmp(mem_saves[i].id, id)) { slot = i; break; }
            if (!mem_saves[i].buf && slot < 0) slot = i;
        }
        if (slot < 0) return -1;
        free(mem_saves[slot].buf);
        mem_saves[slot].buf = malloc(n);
        if (!mem_saves[slot].buf) return -1;
        memcpy(mem_saves[slot].buf, buf, n);
        mem_saves[slot].n = n;
        snprintf(mem_saves[slot].id, sizeof mem_saves[slot].id, "%s", id);
        return 0;
    }
    char p[512];
    snprintf(p, sizeof p, "%s/saves/%s.sav", sim_storage_dir, id);
    return write_atomic(p, buf, n);
}
