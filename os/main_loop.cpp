// New in v2. Replaces v1's src/os/os.cpp for this first cut: no LVGL, no shell,
// no games. It runs the engine's scrolling demo and shows its frames.
//
// What came from v1 os.cpp and is kept in the same order:
//  - the start-up order: power, input, LED, IMU, display;
//  - the sleep flow: wake-press swallowing (power_state), the both-knobs
//    gesture (sleep_gesture), prepareWake -> IMU off -> panel off -> LED off ->
//    settle -> deep sleep.
// What is different:
//  - SLEEP GESTURE IN THIS CUT: holding both knobs for 3 seconds goes straight
//    to sleep once both are released. v1 asked for confirmation on a screen;
//    there is no shell here to draw that, so there is no confirmation.
//  - idle auto-off is switched off (the settings object below), because a
//    timing run should not fall asleep on its own.
//  - nothing waits in a loop: every phase does a little and returns.

#include "main_loop.h"

#include <Arduino.h>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"

#include "../engine/core/fixed_step.h"
#include "../engine/gfx/canvas.h"
#include "board.h"
#include "demo_scene.h"
#include "display.h"
#include "frame_pacer.h"
#include "imu.h"
#include "input.h"
#include "input_adapter.h"
#include "loop_stats.h"
#include "power.h"
#include "power_state.h"
#include "rgb_led.h"
#include "settings.h"
#include "sleep_gesture.h"
#include "tick_input.h"

namespace main_loop {
namespace {

constexpr int SCREEN_W = display_config::WIDTH;
constexpr int SCREEN_H = display_config::HEIGHT;
constexpr std::size_t FRAME_BYTES =
    static_cast<std::size_t>(SCREEN_W) * SCREEN_H * sizeof(std::uint16_t);
constexpr std::uint32_t WATCHDOG_SECONDS = 5;
// A dim solid green says "the loop is running" without lighting up a room.
constexpr std::uint8_t LED_R = 0, LED_G = 6, LED_B = 0;

// Everything the loop owns. File-local, created once.
struct State {
    bool ready = false;
    std::uint16_t* frame = nullptr;  // SCREEN_W * SCREEN_H RGB565, internal RAM
    demo_scene::Scene scene;
    tg::FixedStep step;
    tick_input::Latch latch;
    input_adapter::Adapter adapter;
    power_state::State power;
    sleep_gesture::Gesture gesture;
    frame_pacer::Pacer pacer;
    loop_stats::Stats stats;
    settings::Settings settings;  // defaults; see applyDefaults()
    std::uint32_t last_us = 0;
    std::uint32_t last_failure_log_ms = 0;
};
State app;

// ---- setup ----------------------------------------------------------------

void applyDefaults() {
    // HOOK (handedness, brightness): when the settings screen returns, load
    // these from storage::loadSettings() instead. Until then: defaults, and
    // main knob = right knob because Settings{}.leftHanded() is false.
    app.settings = settings::Settings{};
    app.settings.value[settings::AUTO_OFF] = 0;  // no idle sleep in this cut
}

bool allocateFrame() {
    // Internal RAM (not PSRAM): the SPI write reads this buffer, and a frame is
    // only 110 kB. Allocated once; never freed.
    app.frame = static_cast<std::uint16_t*>(
        heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (app.frame == nullptr) return false;
    std::memset(app.frame, 0, FRAME_BYTES);
    return true;
}

void startWatchdog() {
    // The Arduino core may already have set the task watchdog up; a second init
    // then reports an error that is harmless, so only the add is checked.
    esp_task_wdt_init(WATCHDOG_SECONDS, true);
    if (esp_task_wdt_add(nullptr) != ESP_OK) Serial.println("[BOOT] watchdog add failed");
}

// ---- one pass of the loop, in phases --------------------------------------

// Phase 1+2: read the knobs and the motion sensor. Returns what a game would
// see this pass, and whether the console is accepting input yet (it is not
// while the press that woke it from sleep is still down).
tg::Input readInput(std::uint32_t now_ms, bool buttons_pressed, bool& accepting) {
    imu::update(now_ms);
    const input::State& hw = input::state();
    const imu::Reading& motion = imu::reading();
    const bool activity = hw.left.delta != 0 || hw.right.delta != 0 ||
                          hw.left.button.events != 0 || hw.right.button.events != 0;
    app.power.update(now_ms, activity, buttons_pressed, app.settings);
    accepting = app.power.acceptingInput();
    // Always read: the adapter must see every pass to catch press and release edges.
    tg::Input in = app.adapter.read(hw, motion.tilt_x, motion.tilt_y, app.settings.leftHanded());
    return in;
}

// Phase 3a: the both-knobs gesture. While it owns the switches the scene must
// not see them (v1 did the same).
void sleepGesture(tg::Input& in, std::uint32_t now_ms) {
    const bool requested = app.gesture.update(in.left.down, in.right.down, now_ms);
    if (app.gesture.blocked()) input_adapter::hideButtons(in);
    if (requested) app.power.request();
}

// Phase 3b: carry out a requested sleep. Returns true while sleep is pending,
// in which case the rest of the pass is skipped (as v1 did).
bool prepareSleep() {
    if (!app.power.pending()) return false;
    if (!app.power.canPrepare()) return true;  // wait, without blocking, for release
    if (power::prepareWake() && imu::setSleeping(true)) {
        if (display::prepareSleep()) {
            rgb_led::set(0, 0, 0);
            Serial.println("[POWER] sleeping; click either knob to wake");
            app.power.begin(millis());
        } else {
            imu::setSleeping(false);
            display::setBrightness(app.settings.brightness());
            app.power.cancel(millis());
        }
    } else {
        app.power.cancel(millis());
    }
    return true;
}

// Phase 3c: the panel is off and the settle time is running; sleep when done.
// Returns true while settling. enterSleep() returns only if a switch was down.
bool settleSleep(std::uint32_t now_ms, bool buttons_pressed) {
    if (!app.power.settling()) return false;
    app.power.update(now_ms, false, buttons_pressed);
    if (app.power.ready()) power::enterSleep();
    return true;
}

// Phase 4: run as many 60 Hz scene ticks as real time calls for.
int runTicks(std::uint32_t now_us, const tg::Input& in, bool accepting) {
    const std::uint32_t elapsed = now_us - app.last_us;
    app.last_us = now_us;
    app.latch.add(accepting ? in : tg::Input{});
    const int ticks = app.step.advanceMicros(elapsed);
    for (int i = 0; i < ticks; ++i) app.scene.tick(app.latch.take());
    app.stats.ticks(ticks);
    return ticks;
}

// Phase 5: draw into the frame buffer and send it to the panel, at most 30
// times a second. Returns true if a frame went out.
bool drawAndPresent() {
    if (!app.pacer.due(micros())) return false;
    const std::uint32_t draw_started = micros();
    tg::gfx::Canvas canvas({app.frame, SCREEN_W, SCREEN_H, SCREEN_W});
    app.scene.draw(canvas);
    const std::uint32_t draw_us = micros() - draw_started;
    display::present(app.frame);
    app.stats.frame(draw_us, display::presentMicros());
    return true;
}

// Phase 6: one timing line every few seconds.
void reportTimings(std::uint32_t now_ms) {
    if (!app.stats.due(now_ms)) return;
    char line[160];
    loop_stats::format(line, sizeof(line), app.stats.take(now_ms), ESP.getFreeHeap());
    Serial.println(line);
}

// Phase 7: do not spin. After real work, just let other tasks run; with
// nothing to do, sleep one RTOS tick (1 ms). Input is interrupt driven, so
// nothing is missed while waiting.
void pace(bool did_work) {
    if (did_work) {
        yield();
    } else {
        delay(1);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("[BOOT] TinyGame v2 first cut (engine scrolling demo)");
    Serial.printf("[BOOT] board=%s panel=%dx%d\n", board::NAME, SCREEN_W, SCREEN_H);
    Serial.printf("[BOOT] PSRAM: %lu bytes, free heap %lu\n",
                  static_cast<unsigned long>(ESP.getPsramSize()),
                  static_cast<unsigned long>(ESP.getFreeHeap()));

    applyDefaults();
    if (!power::initialize()) return;
    if (!input::initialize(millis())) return;
    rgb_led::initialize();
    imu::initialize();  // a missing sensor is reported by the driver; tilt stays 0
    if (!allocateFrame()) {
        Serial.println("[BOOT] frame buffer allocation failed");
        return;
    }
    if (!display::initialize()) {
        Serial.println("[BOOT] display failed");
        return;
    }
    display::present(app.frame);  // black, so the first image is not noise
    if (!app.scene.init(SCREEN_W, SCREEN_H)) {
        Serial.println("[BOOT] demo scene failed to start");
        return;
    }
    if (board::has_rgb_led) rgb_led::set(LED_R, LED_G, LED_B);

    app.power.initialize(millis());
    app.last_us = micros();
    app.stats.begin(millis());
    startWatchdog();
    app.ready = true;
    Serial.printf("[BOOT] ready, free heap %lu\n", static_cast<unsigned long>(ESP.getFreeHeap()));
}

void loop() {
    if (!app.ready) {
        // Setup failed and said why. Do not spin; report now and then.
        const std::uint32_t now_ms = millis();
        if (now_ms - app.last_failure_log_ms >= 5000) {
            app.last_failure_log_ms = now_ms;
            Serial.println("[BOOT] not running: setup failed, see the lines above");
        }
        delay(100);
        return;
    }
    esp_task_wdt_reset();

    const std::uint32_t now_ms = millis();
    const std::uint32_t now_us = micros();

    input::update(now_ms);
    const bool buttons_pressed = power::buttonsPressed();
    if (settleSleep(now_ms, buttons_pressed)) {
        pace(false);
        return;
    }

    bool accepting = false;
    tg::Input in = readInput(now_ms, buttons_pressed, accepting);
    if (accepting) sleepGesture(in, now_ms);
    if (prepareSleep()) {
        pace(false);
        return;
    }

    const int ticks = runTicks(now_us, in, accepting);
    const bool presented = drawAndPresent();
    reportTimings(now_ms);
    pace(ticks > 0 || presented);
}

}  // namespace main_loop
