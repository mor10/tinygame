// WebAssembly entry points for the browser simulator and the headless runner.
//
// The page (sim/web/simulator.js) and sim/headless.mjs call these few C
// functions; everything else is the ordinary portable engine.
//
// For now this runs the scrolling demo. The places that change when the real
// engine facade (engine/engine_c.h) is implemented are marked "ENGINE SWITCH".
// If you add or rename a function here, update EXPORTS in tools/sim_build.py.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <emscripten/emscripten.h>

// ENGINE SWITCH 1 of 4: replace these three includes with "../../engine/engine_c.h".
#include "../../engine/demo/scroll_demo.h"
#include "../../engine/gfx/canvas.h"
#include "../../engine/world/stage.h"

namespace {

// Same palette as engine/host/demo_main.cpp, so frames match the golden images.
constexpr int DEMO_PALETTE = 1;
constexpr int MAX_SIDE = 1024;

int g_width = 0;
int g_height = 0;
bool g_running = false;
std::vector<tg::gfx::Color> g_pixels;   // the frame handed to JavaScript
std::vector<std::uint8_t> g_bundle;     // the last bundle the page sent
tg::demo::ScrollDemo g_demo;

}  // namespace

extern "C" {

// Status values returned by tg_sim_load_bundle.
enum { TG_SIM_BUNDLE_OK = 1, TG_SIM_BUNDLE_FAILED = 0, TG_SIM_BUNDLE_UNSUPPORTED = -1 };

// Starts (or restarts) for a screen size. view: 0 = Standard, 1 = Fill.
// Returns 1 on success, 0 on failure.
EMSCRIPTEN_KEEPALIVE int tg_sim_init(int screen_w, int screen_h, int view) {
    g_running = false;
    if (screen_w <= 0 || screen_h <= 0 || screen_w > MAX_SIDE || screen_h > MAX_SIDE) return 0;
    g_width = screen_w;
    g_height = screen_h;
    g_pixels.assign(static_cast<std::size_t>(screen_w) * screen_h, 0);
    // ENGINE SWITCH 2 of 4: when a bundle is stored, call
    //   tg_engine_load(g_bundle.data(), g_bundle.size(), screen_w, screen_h)
    // instead. (The view will then come from the game, not from the page.)
    g_demo = tg::demo::ScrollDemo();
    g_running = g_demo.init(screen_w, screen_h, view == 1 ? tg::View::Fill : tg::View::Standard,
                            DEMO_PALETTE);
    return g_running ? 1 : 0;
}

// One 60 Hz tick. buttons: bit 0 = left knob held, bit 1 = right knob held.
// Knob deltas are in transitions (80 per turn, clockwise positive); tilt is -1..1.
EMSCRIPTEN_KEEPALIVE void tg_sim_tick(std::int32_t left_delta, std::int32_t right_delta,
                                      int buttons, float tilt_x, float tilt_y) {
    if (!g_running) return;
    // ENGINE SWITCH 3 of 4: replace with
    //   tg_engine_tick(left_delta, right_delta, buttons, tilt_x, tilt_y, 0);
    // The demo has no controls, so the input is accepted and dropped here.
    (void)left_delta;
    (void)right_delta;
    (void)buttons;
    (void)tilt_x;
    (void)tilt_y;
    g_demo.tick();
}

// Draws the current frame and returns it: width * height RGB565 pixels.
// The pointer is valid until the next tg_sim_init.
EMSCRIPTEN_KEEPALIVE const std::uint16_t* tg_sim_draw(void) {
    if (!g_running) return g_pixels.data();
    // ENGINE SWITCH 4 of 4: replace the rest of this function with
    //   return tg_engine_draw();
    tg::gfx::Canvas canvas({g_pixels.data(), g_width, g_height, g_width});
    g_demo.draw(canvas);
    return g_pixels.data();
}

EMSCRIPTEN_KEEPALIVE int tg_sim_width(void) { return g_width; }
EMSCRIPTEN_KEEPALIVE int tg_sim_height(void) { return g_height; }

// Hands the engine a game bundle (.tgb bytes; copy them in with malloc first).
// The bytes are kept, but games cannot run yet, so this always answers
// TG_SIM_BUNDLE_UNSUPPORTED and the demo keeps running. When the engine facade
// exists (see ENGINE SWITCH 2) this restarts the engine with the bundle.
EMSCRIPTEN_KEEPALIVE int tg_sim_load_bundle(const std::uint8_t* bytes, std::uint32_t size) {
    if (bytes == nullptr && size != 0) return TG_SIM_BUNDLE_FAILED;
    g_bundle.assign(bytes, bytes + size);
    return TG_SIM_BUNDLE_UNSUPPORTED;
}

}  // extern "C"
