#include "os.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdarg>
#include <cstdio>

#include "devtools.h"
#include "display.h"
#include "imu.h"
#include "input.h"
#include "input_adapter.h"
#include "persistence.h"
#include "power.h"
#include "power_state.h"
#include "registry.h"
#include "rgb_led.h"
#include "shell_state.h"
#include "shell_ui.h"
#include "status_light.h"
#include "storage.h"
#include "tinygame.h"

namespace os {

namespace {

using shell::Action;
using shell::Screen;
using status_light::Pattern;

constexpr float MAX_FRAME_SECONDS = 0.1f;
constexpr std::uint32_t INPUT_LOG_INTERVAL_MS = 1000;

shell::State shell_state;
status_light::StatusLight led;
persistence::SettingsSave settings_save;  // decides when settings are written to flash
settings::Settings applied_settings;
std::uint32_t last_update_us = 0;
std::uint32_t last_input_log_at = 0;
Screen logged_screen = Screen::Welcome;
input_adapter::Adapter input_adapter;
power_state::State power_state;

// The running game.
struct Running {
    tg::Game* game = nullptr;
    lv_obj_t* screen = nullptr;
    std::uint8_t index = 0;
    std::uint32_t led_rgb = 0;
    persistence::BestScore best;  // read from flash once per run
};
Running running;

const tg::GameInfo& runningInfo() {
    return registry::get(running.index);
}

void startGame(std::uint8_t index) {
    const tg::GameInfo& info = registry::get(index);
    running.index = index;
    running.led_rgb = info.color;
    running.best.value = storage::loadBest(info.id);
    running.screen = lv_obj_create(nullptr);
    running.game = info.create();
    running.game->start(running.screen);
    lv_screen_load(running.screen);
    Serial.printf("[APP] start %s heap=%lu\n", info.id,
                  static_cast<unsigned long>(ESP.getFreeHeap()));
}

// The old game is deleted after the OS screens have moved on (see update()).
Running takeRunning() {
    Running old = running;
    running.game = nullptr;
    running.screen = nullptr;
    return old;
}

void destroy(Running& old) {
    if (old.screen != nullptr) {
        lv_obj_delete(old.screen);
    }
    delete old.game;
}

void updateLed(std::uint32_t now_ms) {
    switch (shell_state.screen()) {
        case Screen::Welcome:
            led.setBase(status_light::fromHex(tg::color::ACCENT), Pattern::Breathe);
            break;
        case Screen::Launcher: {
            const std::uint8_t s = shell_state.selected();
            led.setBase(status_light::fromHex(s < registry::count() ? registry::get(s).color
                                                                    : tg::color::SILVER));
            break;
        }
        case Screen::Game:
            led.setBase(status_light::fromHex(running.led_rgb));
            break;
        case Screen::Pause:
            led.setBase(status_light::fromHex(tg::color::WARN), Pattern::Blink);
            break;
        case Screen::Settings:
            led.setBase(status_light::fromHex(shell_state.editing() ? tg::color::WARN
                                                                    : tg::color::SILVER));
            break;
        default:
            led.setBase(status_light::fromHex(tg::color::PLUM));
            break;
    }
    const status_light::Rgb c = led.output(now_ms, shell_state.settings().led());
    rgb_led::set(c.r, c.g, c.b);
}

// Applies settings live (brightness while editing) and saves confirmed ones.
void handleSettings() {
    const settings::Settings& current = shell_state.settings();
    if (current.brightness() != applied_settings.brightness()) {
        display::setBrightness(current.brightness());
    }
    applied_settings = current;
    if (!settings_save.needed(current, shell_state.editing())) {
        return;
    }
    const bool saved = storage::saveSettings(current);
    settings_save.attempted(current, saved);
    Serial.println(saved ? "[APP] settings saved" : "[APP] settings save failed; confirm to retry");
}

void logState(const input::State& hw, std::uint32_t now_ms) {
    if (shell_state.screen() != logged_screen) {
        logged_screen = shell_state.screen();
        Serial.printf("[APP] screen=%s\n", shell::screenName(logged_screen));
    }
    if (devtools::overrides().input_log && now_ms - last_input_log_at >= INPUT_LOG_INTERVAL_MS) {
        last_input_log_at = now_ms;
        const imu::Reading& m = imu::reading();
        for (int side = 0; side < 2; ++side) {
            const auto& e = side == 0 ? hw.left : hw.right;
            const auto& effective = side == 0 ? input_adapter.leftEvidence() :
                                               input_adapter.rightEvidence();
            Serial.printf("[INPUT] %s physical total=%ld ab=%u dir=%d down=%d held=%lu "
                          "clicks=%lu last=%s invalid=%lu overrun=%lu "
                          "effective total=%ld clicks=%lu last=%s\n",
                          side == 0 ? "L" : "R", static_cast<long>(e.total), e.ab,
                          e.last_direction, e.button.down,
                          static_cast<unsigned long>(e.button.held_ms),
                          static_cast<unsigned long>(e.button.clicks),
                          input::eventName(e.button.last_event),
                          static_cast<unsigned long>(e.invalid),
                          static_cast<unsigned long>(e.overruns),
                          static_cast<long>(effective.total),
                          static_cast<unsigned long>(effective.clicks),
                          input::eventName(effective.last_event));
        }
        Serial.printf("[INPUT] idle_sleep=180s ready=%d wake=%s tilt=%+.2f,%+.2f\n",
                      power_state.acceptingInput(), power::wakeName(),
                      static_cast<double>(m.tilt_x), static_cast<double>(m.tilt_y));
    }
}

}  // namespace

bool initialize() {
    Serial.begin(115200);
    Serial.println("[BOOT] TinyGame starting");
    Serial.printf("[BOOT] PSRAM: %lu bytes\n", static_cast<unsigned long>(ESP.getPsramSize()));

    if (!power::initialize()) return false;
    if (!input::initialize(millis())) return false;
    rgb_led::initialize();
    imu::initialize();
    if (!display::initialize()) {
        Serial.println("[BOOT] display failed");
        return false;
    }
    tg::styleInit(display::lvDisplay());

    registry::sortByName();
    for (std::uint8_t i = 0; i < registry::errorCount(); ++i) {
        const registry::Error& error = registry::error(i);
        Serial.printf("[BOOT] rejected game %s: %s\n", error.id ? error.id : "(null)", error.reason);
    }
    applied_settings = storage::loadSettings();
    settings_save.begin(applied_settings);
    display::setBrightness(applied_settings.brightness());
    shell_state.begin(registry::count(), applied_settings);
    for (std::uint8_t i = 0; i < registry::count(); ++i) {
        Serial.printf("[BOOT] game %s\n", registry::get(i).id);
    }
    Serial.printf("[BOOT] ready, %u games. Type 'help' for dev commands.\n",
                  static_cast<unsigned>(registry::count()));
    last_update_us = micros();
    power_state.initialize(millis());
    return true;
}

void update() {
    const std::uint32_t now_ms = millis();
    const std::uint32_t now_us = micros();
    float dt = static_cast<float>(now_us - last_update_us) / 1'000'000.0f;
    last_update_us = now_us;
    if (dt > MAX_FRAME_SECONDS) {
        dt = MAX_FRAME_SECONDS;
    }

    input::update(now_ms);
    const bool buttons_pressed = power::buttonsPressed();
    if (power_state.settling()) {
        power_state.update(now_ms, false, buttons_pressed);
        if (power_state.ready()) power::enterSleep();
        return;
    }
    devtools::poll();
    imu::update(now_ms);
    const devtools::Overrides& dev = devtools::overrides();
    const input::State& hw = input::state();
    const bool activity = hw.left.delta != 0 || hw.right.delta != 0 ||
                          hw.left.button.events != 0 || hw.right.button.events != 0 ||
                          dev.left.delta != 0 || dev.right.delta != 0 ||
                          dev.left.events != 0 || dev.right.events != 0 ||
                          dev.left.down || dev.right.down;
    power_state.update(now_ms, activity, buttons_pressed);
    if (!power_state.acceptingInput()) {
        devtools::endFrame();
        lv_timer_handler();
        return;
    }
    const bool left_handed = shell_state.settings().leftHanded();
    const std::uint8_t events = hw.left.button.events | dev.left.events |
                                hw.right.button.events | dev.right.events;
    const tg::Input in = input_adapter.read(hw, imu::reading(), dev, now_ms, left_handed);

    Action action = shell_state.update(in.main.delta, in.click, (events & input::EVENT_LONG_PRESS) != 0);
    if (dev.launch >= 0) {
        action = shell_state.launch(static_cast<std::uint8_t>(dev.launch));
        if (action == Action::Launch && running.game != nullptr) {
            action = Action::Restart;  // replace the running game
        }
    }
    if (action == Action::Sleep) power_state.request();
    if (power_state.pending()) {
        if (power_state.canPrepare()) {
            if (power::prepareWake() && imu::setSleeping(true)) {
                if (display::prepareSleep()) {
                    rgb_led::set(0, 0, 0);
                    Serial.println("[POWER] sleeping; click either knob to wake");
                    power_state.begin(millis());
                } else {
                    imu::setSleeping(false);
                    display::setBrightness(applied_settings.brightness());
                    power_state.cancel(millis());
                }
            } else {
                power_state.cancel(millis());
            }
        }
        devtools::endFrame();
        return;
    }

    // New game screens are created before the OS screens update, and old
    // ones are deleted after, so LVGL always has a screen to show.
    Running old;
    switch (action) {
        case Action::Launch:
            startGame(shell_state.game());
            break;
        case Action::Restart:
            old = takeRunning();
            startGame(shell_state.game());
            break;
        case Action::Quit:
            old = takeRunning();
            break;
        case Action::Pause:
            running.game->paused(true);
            break;
        case Action::Resume:
            running.game->paused(false);
            break;
        case Action::None:
        case Action::Sleep:
            break;
    }
    shell_ui::update(shell_state, running.screen,
                     {hw, imu::reading(), imu::available(), imu::healthy(),
                      input_adapter.leftEvidence(), input_adapter.rightEvidence(),
                      power_state.acceptingInput(), power::wakeName()}, now_ms);
    destroy(old);

    // A click that just launched or resumed a game is not passed to it.
    if (shell_state.screen() == Screen::Game && action == Action::None) {
        running.game->update(in, dt);
    }

    handleSettings();
    updateLed(now_ms);
    logState(hw, now_ms);
    devtools::endFrame();
    lv_timer_handler();
}

}  // namespace os

// ---- tg:: API used by games -----------------------------------------------------

namespace tg {

detail::Registrar::Registrar(const GameInfo& info) {
    registry::add(info);
}

std::uint32_t bestScore() {
    return os::running.game ? os::running.best.value : 0;
}

// Only a successful write counts as a new best.
bool submitScore(std::uint32_t score) {
    if (!os::running.game) return false;
    const bool saved = os::running.best.submit(score, [](std::uint32_t value) {
        const bool ok = storage::saveBest(os::runningInfo().id, value);
        if (!ok) Serial.printf("[APP] %s best score save failed\n", os::runningInfo().id);
        return ok;
    });
    if (!saved) return false;
    Serial.printf("[APP] %s new best %lu\n", os::runningInfo().id,
                  static_cast<unsigned long>(score));
    return true;
}

void led(std::uint32_t rgb) {
    os::running.led_rgb = rgb;
}

void flashLed(std::uint32_t rgb, std::uint32_t ms) {
    os::led.flash(status_light::fromHex(rgb), ms, millis());
}

void log(const char* fmt, ...) {
    char buffer[160];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Serial.printf("[GAME %s] %s\n", os::running.game ? os::runningInfo().id : "?", buffer);
}

}  // namespace tg
