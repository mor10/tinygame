#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565

/* Use the C library allocator: large buffers land in the 8 MB PSRAM instead of
 * a fixed 64 KB LVGL pool, so games can create plenty of objects. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB

#define LV_USE_DRAW_SW 1
/* ~60 Hz display refresh for smooth motion. */
#define LV_DEF_REFR_PERIOD 16

/* TinyGame installs its own theme (src/os/style.cpp). */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0

/* Fonts: the style library scales the 8x8 pixel font (tg::Font). */
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_UNSCII_8 1
#define LV_FONT_DEFAULT &lv_font_unscii_8

/* Widgets games may use. */
#define LV_USE_LABEL 1
#define LV_USE_BAR 1
#define LV_USE_IMAGE 1
#define LV_USE_CANVAS 1
#define LV_USE_LINE 1

/* Screenshots for the dev tools (`shot`). */
#define LV_USE_SNAPSHOT 1

#endif
