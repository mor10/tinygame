#include "test_card.h"

#include <Arduino.h>
#include <lvgl.h>
#include <tusb.h>

#include <cstdio>

#include "dev_line.h"
#include "display.h"
#include "encoder_counter.h"
#include "imu.h"
#include "input.h"
#include "power.h"
#include "protocol.h"
#include "tg_board.h"
#include "tg_style.h"

namespace {

constexpr int SWATCH_SIZE = 12;
constexpr int SWATCH_GAP = 2;
constexpr int SWATCH_Y = 28;
constexpr int STATUS_X = tg::board::WIDTH / 2;
constexpr std::uint32_t REFRESH_MS = 100;

lv_obj_t* left_total = nullptr;
lv_obj_t* left_button = nullptr;
lv_obj_t* left_hold_event = nullptr;
lv_obj_t* left_errors = nullptr;
lv_obj_t* right_total = nullptr;
lv_obj_t* right_button = nullptr;
lv_obj_t* right_hold_event = nullptr;
lv_obj_t* right_errors = nullptr;
lv_obj_t* tilt_x = nullptr;
lv_obj_t* tilt_y = nullptr;
lv_obj_t* gyro_x = nullptr;
lv_obj_t* gyro_y = nullptr;
lv_obj_t* gyro_z = nullptr;
lv_obj_t* sensor_status = nullptr;
bool input_ready = false;
bool imu_ready = false;
bool counter_ready = false;
std::uint32_t last_refresh = 0;
devtools::LineReader serial_reader;

bool serialHostActive() {
    return tud_mounted() && (tud_cdc_get_line_state() & 1U) != 0;
}

const char* shortEvent(input::ButtonEvent event) {
    switch (event) {
        case input::ButtonEvent::Pressed: return "DN";
        case input::ButtonEvent::Released: return "UP";
        case input::ButtonEvent::ShortPress: return "CLK";
        case input::ButtonEvent::LongPress: return "HLD";
        default: return "-";
    }
}

lv_obj_t* addLabel(lv_obj_t* screen, const char* text, tg::Font font,
                   int x, int y, std::uint32_t color = tg::color::TEXT) {
    lv_obj_t* label = tg::ui::label(screen, text, font, color);
    lv_obj_set_pos(label, x, y);
    return label;
}

void addPalette(lv_obj_t* screen) {
    for (int i = 0; i < 16; ++i) {
        const int x = tg::board::SAFE_X + i * (SWATCH_SIZE + SWATCH_GAP);
        lv_obj_t* swatch = tg::ui::block(screen, x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE,
                                        tg::PALETTE[i]);
        lv_obj_set_style_border_color(swatch, lv_color_hex(tg::color::LINE), 0);
        lv_obj_set_style_border_width(swatch, 1, 0);
    }
}

void updateText() {
    const auto& state = input::state();
    if (!input_ready) {
        tg::ui::text(left_total, "LEFT INPUT ERR");
        tg::ui::text(left_button, "NOT AVAILABLE");
        tg::ui::text(left_hold_event, "");
        tg::ui::text(left_errors, "");
        tg::ui::text(right_total, "RIGHT INPUT ERR");
        tg::ui::text(right_button, "NOT AVAILABLE");
        tg::ui::text(right_hold_event, "");
        tg::ui::text(right_errors, "");
    } else {
        const auto& left = state.left;
        const auto& right = state.right;
        tg::ui::text(left_total, "L T:%ld D:%+ld", static_cast<long>(left.total),
                     static_cast<long>(left.delta));
        tg::ui::text(left_button, "L B:%s C:%lu", left.button.down ? "DN" : "UP",
                     static_cast<unsigned long>(left.button.clicks));
        tg::ui::text(left_hold_event, "H:%lu E:%s",
                     static_cast<unsigned long>(left.button.held_ms),
                     shortEvent(left.button.last_event));
        tg::ui::text(left_errors, "ERR:%lu", static_cast<unsigned long>(left.invalid));
        tg::ui::text(right_total, "R T:%ld D:%+ld", static_cast<long>(right.total),
                     static_cast<long>(right.delta));
        tg::ui::text(right_button, "R B:%s C:%lu", right.button.down ? "DN" : "UP",
                     static_cast<unsigned long>(right.button.clicks));
        tg::ui::text(right_hold_event, "H:%lu E:%s",
                     static_cast<unsigned long>(right.button.held_ms),
                     shortEvent(right.button.last_event));
        tg::ui::text(right_errors, "ERR:%lu", static_cast<unsigned long>(right.invalid));
    }

    const auto& motion = imu::reading();
    const char* imu_status = !imu_ready ? "UNAVAILABLE" :
                             (imu::healthy() ? "OK" : "READ ERR");
    if (!imu_ready) {
        tg::ui::text(sensor_status, "IMU UNAVAILABLE");
        tg::ui::text(tilt_x, "T X:--");
        tg::ui::text(tilt_y, "T Y:--");
        tg::ui::text(gyro_x, "G X:--");
        tg::ui::text(gyro_y, "G Y:--");
        tg::ui::text(gyro_z, "G Z:--");
    } else {
        tg::ui::text(sensor_status, "IMU %s", imu_status);
        tg::ui::text(tilt_x, "T X:%+.2f", motion.tilt_x);
        tg::ui::text(tilt_y, "T Y:%+.2f", motion.tilt_y);
        tg::ui::text(gyro_x, "G X:%+.1f", motion.gyro_dps.x);
        tg::ui::text(gyro_y, "G Y:%+.1f", motion.gyro_dps.y);
        tg::ui::text(gyro_z, "G Z:%+.1f", motion.gyro_dps.z);
    }

    if (serialHostActive()) {
        Serial.printf(
            "[DIAG] IN=%s L=%ld,%ld,%d,%lu,%lu,%lu R=%ld,%ld,%d,%lu,%lu,%lu "
            "IMU=%s "
            "T=%+.2f,%+.2f G=%+.1f,%+.1f,%+.1f\n",
            input_ready ? "OK" : "ERROR",
            static_cast<long>(state.left.total), static_cast<long>(state.left.delta),
            state.left.button.down ? 1 : 0,
            static_cast<unsigned long>(state.left.button.clicks),
            static_cast<unsigned long>(state.left.button.held_ms),
            static_cast<unsigned long>(state.left.invalid),
            static_cast<long>(state.right.total), static_cast<long>(state.right.delta),
            state.right.button.down ? 1 : 0,
            static_cast<unsigned long>(state.right.button.clicks),
            static_cast<unsigned long>(state.right.button.held_ms),
            static_cast<unsigned long>(state.right.invalid),
            imu_status,
            static_cast<double>(motion.tilt_x), static_cast<double>(motion.tilt_y),
            static_cast<double>(motion.gyro_dps.x), static_cast<double>(motion.gyro_dps.y),
            static_cast<double>(motion.gyro_dps.z));
    }
}

void pollSerial() {
    while (Serial.available() > 0) {
        const auto event = serial_reader.feed(static_cast<char>(Serial.read()));
        if (event == devtools::LineReader::Event::None) continue;
        if (event == devtools::LineReader::Event::TooLong) {
            Serial.println("[DEV] error: command too long");
            return;
        }

        switch (diagnostics::parseCommand(serial_reader.line())) {
            case diagnostics::SerialCommand::Info: {
                const char* imu_status = !imu_ready ? "UNAVAILABLE" :
                                         (imu::healthy() ? "OK" : "READ ERR");
                Serial.printf("[DEV] profile=%s screen=%dx%d input=%s imu=%s\n",
                              tg::board::ID, tg::board::WIDTH, tg::board::HEIGHT,
                              input_ready ? "READY" : "ERROR", imu_status);
                Serial.println("[DEV] ok");
                break;
            }
            case diagnostics::SerialCommand::Counter: {
                diagnostics::counter::Reading counts{};
                if (!input_ready || !counter_ready || !diagnostics::counter::read(counts)) {
                    Serial.println("[DEV] error: pulse counter unavailable");
                    break;
                }
                const auto& state = input::state();
                Serial.printf("[DEV] counter L=%ld,%ld,%lu,%lu,%d R=%ld,%ld,%lu,%lu,%d\n",
                              static_cast<long>(counts.left), static_cast<long>(state.left.total),
                              static_cast<unsigned long>(state.left.invalid),
                              static_cast<unsigned long>(state.left.overruns), counts.left_overflow,
                              static_cast<long>(counts.right), static_cast<long>(state.right.total),
                              static_cast<unsigned long>(state.right.invalid),
                              static_cast<unsigned long>(state.right.overruns), counts.right_overflow);
                Serial.println("[DEV] ok");
                break;
            }
            case diagnostics::SerialCommand::Help:
                Serial.println("[DEV] commands: info counter");
                Serial.println("[DEV] ok");
                break;
            case diagnostics::SerialCommand::Unknown:
                Serial.println("[DEV] error: unknown diagnostic command");
                break;
        }
        return;
    }
}

}  // namespace

namespace diagnostics {

bool initialize() {
    Serial.begin(115200);
    Serial.printf("[DIAG] Compiled profile %s (%dx%d); this is not physical board identification\n",
                  tg::board::ID, tg::board::WIDTH, tg::board::HEIGHT);
    if (!power::initialize()) return false;
    if (!display::initialize()) return false;
    tg::styleInit(display::lvDisplay());

    counter_ready = diagnostics::counter::initialize();
#if TG_BOARD == 147
    if (!counter_ready) Serial.println("[DIAG] Pulse counter unavailable");
#endif
    input_ready = input::initialize(millis());
    if (!input_ready) Serial.println("[DIAG] Input initialization failed");
    imu_ready = imu::initialize();
    if (!imu_ready) Serial.println("[DIAG] IMU unavailable; display and knobs remain testable");

    lv_obj_t* screen = lv_display_get_screen_active(display::lvDisplay());
    lv_obj_set_style_bg_color(screen, lv_color_hex(tg::color::BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    char title[48];
    std::snprintf(title, sizeof(title), "PROFILE %s  %dx%d", tg::board::ID,
                  tg::board::WIDTH, tg::board::HEIGHT);
    addLabel(screen, title, tg::Font::S, tg::board::SAFE_X, 2, tg::color::ACCENT);
    addLabel(screen, "PALETTE 0-F", tg::Font::S, tg::board::SAFE_X, 18,
             tg::color::TEXT_DIM);
    addPalette(screen);

    addLabel(screen, "S 0123456789", tg::Font::S, tg::board::SAFE_X, 46);
    addLabel(screen, "M 0123", tg::Font::M, tg::board::SAFE_X, 58);
    addLabel(screen, "L 01", tg::Font::L, tg::board::SAFE_X, 80);
    addLabel(screen, "XL", tg::Font::XL, tg::board::SAFE_X, 108);

    const int row = 8;
    left_total = addLabel(screen, "", tg::Font::S, STATUS_X, 46);
    left_button = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row);
    left_hold_event = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 2);
    left_errors = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 3);
    right_total = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 4);
    right_button = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 5);
    right_hold_event = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 6);
    right_errors = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 7);
    sensor_status = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 8,
                             tg::color::WARN);
    tilt_x = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 9);
    tilt_y = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 10);
    gyro_x = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 11);
    gyro_y = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 12);
    gyro_z = addLabel(screen, "", tg::Font::S, STATUS_X, 46 + row * 13);

    last_refresh = millis();
    updateText();
    Serial.println("[DIAG] Test card ready");
    return true;
}

void update() {
    const std::uint32_t now = millis();
    pollSerial();
    if (input_ready) input::update(now);
    if (imu_ready) imu::update(now);
    if (now - last_refresh >= REFRESH_MS) {
        last_refresh = now;
        updateText();
    }
    lv_timer_handler();
    yield();
}

}  // namespace diagnostics
