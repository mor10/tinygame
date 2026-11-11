// WebAssembly entry points for the browser simulator and the headless runner.
//
// The page (sim/web/simulator.js) and sim/headless.mjs call these few C
// functions; everything else is the ordinary portable engine.
//
// With no game bundle loaded this shows the scrolling demo; once a bundle is
// loaded the real engine (engine/engine_c.h) runs it. The places where the two
// meet are marked "ENGINE SWITCH". If you add or rename a function here, update EXPORTS in tools/sim_build.py.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

// ENGINE SWITCH 1 of 4: the engine's flat C interface, next to the demo.
#include "../../engine/engine_c.h"
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
std::vector<std::uint8_t> g_bundle;     // the last good bundle the page sent
bool g_has_game = false;                // true while the engine is running g_bundle
std::string g_load_error;               // why the last bundle was refused
tg::demo::ScrollDemo g_demo;

int g_view = 0;

// Starts the scrolling demo for the current screen size.
bool startDemo() {
    g_has_game = false;
    g_demo = tg::demo::ScrollDemo();
    g_running = g_demo.init(g_width, g_height, g_view == 1 ? tg::View::Fill : tg::View::Standard,
                            DEMO_PALETTE);
    return g_running;
}

}  // namespace

extern "C" {

// Status values returned by tg_sim_load_bundle.
enum { TG_SIM_BUNDLE_OK = 0, TG_SIM_BUNDLE_FAILED = -1 };

// Starts (or restarts) for a screen size. view: 0 = Standard, 1 = Fill.
// Returns 1 on success, 0 on failure.
EMSCRIPTEN_KEEPALIVE int tg_sim_init(int screen_w, int screen_h, int view) {
    g_running = false;
    if (screen_w <= 0 || screen_h <= 0 || screen_w > MAX_SIDE || screen_h > MAX_SIDE) return 0;
    g_width = screen_w;
    g_height = screen_h;
    g_view = view;
    g_pixels.assign(static_cast<std::size_t>(screen_w) * screen_h, 0);
    // ENGINE SWITCH 2 of 4: with a good bundle stored the engine runs it (the
    // view then comes from the game, not from the page); otherwise the demo.
    g_has_game = false;
    if (!g_bundle.empty()) {
        g_has_game = tg_engine_load(g_bundle.data(), static_cast<std::uint32_t>(g_bundle.size()),
                                    screen_w, screen_h) == 1;
        if (g_has_game) {
            g_running = true;
            return 1;
        }
        g_load_error = tg_engine_error();
    }
    return startDemo() ? 1 : 0;
}

// One 60 Hz tick. buttons: bit 0 = left knob held, bit 1 = right knob held.
// Knob deltas are in transitions (80 per turn, clockwise positive); tilt is -1..1.
EMSCRIPTEN_KEEPALIVE void tg_sim_tick(std::int32_t left_delta, std::int32_t right_delta,
                                      int buttons, float tilt_x, float tilt_y) {
    if (!g_running) return;
    // ENGINE SWITCH 3 of 4: a game gets the input; the demo has no controls,
    // so for it the input is accepted and dropped.
    if (g_has_game) {
        tg_engine_tick(left_delta, right_delta, buttons, tilt_x, tilt_y, 0);
        return;
    }
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
    // ENGINE SWITCH 4 of 4: the engine draws a game; otherwise the demo draws.
    if (g_has_game) return tg_engine_draw();
    tg::gfx::Canvas canvas({g_pixels.data(), g_width, g_height, g_width});
    g_demo.draw(canvas);
    return g_pixels.data();
}

EMSCRIPTEN_KEEPALIVE int tg_sim_width(void) { return g_width; }
EMSCRIPTEN_KEEPALIVE int tg_sim_height(void) { return g_height; }

// Hands the engine a game bundle (.tgb bytes; copy them in with malloc first)
// and starts it at the current screen size. Returns 0 when the game runs, or a
// negative number when it could not be loaded; tg_sim_error() then says why.
// A refused bundle leaves the demo running.
EMSCRIPTEN_KEEPALIVE int tg_sim_load_bundle(const std::uint8_t* bytes, std::uint32_t size) {
    if (bytes == nullptr && size != 0) {
        g_load_error = "No bundle bytes were given.";
        return TG_SIM_BUNDLE_FAILED;
    }
    if (g_width <= 0 || g_height <= 0) {
        g_load_error = "The simulator has not started yet.";
        return TG_SIM_BUNDLE_FAILED;
    }
    std::vector<std::uint8_t> copy(bytes, bytes + size);
    const bool ok = tg_engine_load(copy.data(), size, g_width, g_height) == 1;
    g_has_game = ok;
    if (!ok) {
        g_load_error = tg_engine_error();
        if (g_load_error.empty()) g_load_error = "The game could not be loaded.";
        g_bundle.clear();
        startDemo();
        return TG_SIM_BUNDLE_FAILED;
    }
    g_load_error.clear();
    g_bundle = std::move(copy);
    g_running = true;
    return TG_SIM_BUNDLE_OK;
}

// "" when all is well; otherwise why the last bundle was refused, or the
// error that stopped the running game. Valid until the next call.
EMSCRIPTEN_KEEPALIVE const char* tg_sim_error(void) {
    return g_has_game ? tg_engine_error() : g_load_error.c_str();
}

// 1 while a game (not the demo) is running.
EMSCRIPTEN_KEEPALIVE int tg_sim_has_game(void) { return g_has_game ? 1 : 0; }

// The engine's probe text ("tick 120\nscore 10\n..."); "" for the demo.
EMSCRIPTEN_KEEPALIVE const char* tg_sim_probe(void) { return g_has_game ? tg_engine_probe() : ""; }

}  // extern "C"
