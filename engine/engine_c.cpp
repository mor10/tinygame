#include "engine_c.h"

#include <vector>

#include "core/button_edges.h"
#include "engine.h"

namespace {
tg::Engine g_engine;
tg::ButtonEdges g_edges;
std::vector<tg::gfx::Color> g_pixels;
int g_width = 0;
int g_height = 0;
char g_probe[2048];
}  // namespace

extern "C" {

int tg_engine_load(const uint8_t* bundle, uint32_t size, int screen_w, int screen_h) {
    g_edges.reset();
    g_width = 0;
    g_height = 0;
    g_pixels.clear();
    if (bundle == nullptr && size != 0) return 0;
    if (!g_engine.load(bundle, size, screen_w, screen_h)) return 0;
    g_width = screen_w;
    g_height = screen_h;
    g_pixels.assign(static_cast<std::size_t>(screen_w) * static_cast<std::size_t>(screen_h), 0);
    return 1;
}

const char* tg_engine_error(void) { return g_engine.error(); }

void tg_engine_tick(int32_t left_delta, int32_t right_delta, int buttons, float tilt_x,
                    float tilt_y, int left_handed) {
    g_engine.tick(g_edges.step(left_delta, right_delta, buttons, tilt_x, tilt_y, left_handed != 0));
}

const uint16_t* tg_engine_draw(void) {
    if (g_pixels.empty()) return nullptr;
    tg::gfx::Canvas canvas({g_pixels.data(), g_width, g_height, g_width});
    g_engine.draw(canvas);
    return g_pixels.data();
}

const char* tg_engine_probe(void) {
    g_engine.probeText(g_probe, sizeof(g_probe));
    return g_probe;
}

}  // extern "C"
