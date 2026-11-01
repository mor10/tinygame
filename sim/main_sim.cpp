#include <Arduino.h>
#include <emscripten.h>

#include "sim_hal.h"

void setup();
void loop();

namespace {
bool stopped = false;
bool wake_requested = false;

EM_JS(void, notifyWake, (int side), {
    if (Module.onWake) Module.onWake(side);
});

void frame() {
    if (!stopped && !sim::sleeping()) loop();
}
}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void tg_sim_turn(int side, std::int32_t delta) {
    sim::turn(side, delta);
}
EMSCRIPTEN_KEEPALIVE void tg_sim_button(int side, int down) {
    if (side < 0 || side > 1) return;
    if (sim::sleeping()) {
        // Deep sleep loses RAM on the board. The host creates a fresh factory
        // instance on wake rather than rerunning setup over live OS globals.
        if (down && !wake_requested) {
            wake_requested = true;
            notifyWake(side);
        }
        return;
    }
    sim::button(side, down != 0);
}
EMSCRIPTEN_KEEPALIVE void tg_sim_tilt(float x, float y) { sim::tilt(x, y); }
EMSCRIPTEN_KEEPALIVE int tg_sim_command(const char* command) {
    return !stopped && !sim::sleeping() && Serial.enqueue(command);
}
EMSCRIPTEN_KEEPALIVE const std::uint16_t* tg_sim_framebuffer() {
    return sim::framebuffer();
}
EMSCRIPTEN_KEEPALIVE int tg_sim_sleeping() { return sim::sleeping(); }
EMSCRIPTEN_KEEPALIVE int tg_sim_brightness() { return sim::brightness(); }
EMSCRIPTEN_KEEPALIVE std::uint32_t tg_sim_led() { return sim::led(); }
EMSCRIPTEN_KEEPALIVE void tg_sim_stop() {
    stopped = true;
    emscripten_cancel_main_loop();
}
EMSCRIPTEN_KEEPALIVE void tg_sim_pause() { emscripten_pause_main_loop(); }
EMSCRIPTEN_KEEPALIVE void tg_sim_resume() {
    if (!stopped) emscripten_resume_main_loop();
}
}

int main() {
    setup();
    frame();
    emscripten_set_main_loop(frame, 0, false);
    return 0;
}
