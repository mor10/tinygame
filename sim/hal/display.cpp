#include "display.h"

#include <algorithm>
#include <cstring>
#include "Arduino.h"
#include "display_config.h"
#include "sim_hal.h"

namespace {
lv_display_t* screen = nullptr;
std::uint8_t backlight = 90;
alignas(4) std::uint16_t frame[display_config::WIDTH * display_config::HEIGHT]{};
alignas(4) std::uint8_t draw_buffer[display_config::WIDTH *
                                  display_config::DRAW_BUFFER_LINES * 2];

void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels) {
    const auto width = area->x2 - area->x1 + 1;
    const auto stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
    for (int y = area->y1; y <= area->y2; ++y) {
        std::memcpy(frame + y * display_config::WIDTH + area->x1,
                    pixels + (y - area->y1) * stride, width * 2);
    }
    lv_display_flush_ready(display);
}
}  // namespace

namespace display {
bool initialize() {
    lv_init();
    lv_tick_set_cb(millis);
    screen = lv_display_create(display_config::WIDTH, display_config::HEIGHT);
    if (!screen) return false;
    lv_display_set_color_format(screen, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(screen, flush);
    lv_display_set_buffers(screen, draw_buffer, nullptr, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("[LCD] Simulator RGB565 320x172 initialized");
    return true;
}
lv_display_t* lvDisplay() { return screen; }
void setBrightness(std::uint8_t percent) { backlight = std::min<std::uint8_t>(100, percent); }
bool prepareSleep() {
    backlight = 0;
    return true;
}
}  // namespace display

namespace sim {
const std::uint16_t* framebuffer() { return frame; }
std::uint8_t brightness() { return backlight; }
}  // namespace sim
