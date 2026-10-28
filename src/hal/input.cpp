#include "input.h"

#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "soc/gpio_struct.h"

#include "board_config.h"

namespace {

struct Capture {
    int a;
    int b;
    int direction;
    input::Quadrature decoder;
    std::int32_t pending = 0;
    std::int32_t total = 0;
    std::uint32_t overruns = 0;
    int last_direction = 0;
};

Capture left{board::PIN_LEFT_A, board::PIN_LEFT_B, board::LEFT_DIRECTION};
Capture right{board::PIN_RIGHT_A, board::PIN_RIGHT_B, board::RIGHT_DIRECTION};
portMUX_TYPE capture_mux = portMUX_INITIALIZER_UNLOCKED;
input::State current;

__attribute__((always_inline)) inline std::uint8_t readAB(const Capture& c) {
    const std::uint32_t levels = GPIO.in;
    return static_cast<std::uint8_t>((((levels >> c.a) & 1U) << 1U) |
                                     ((levels >> c.b) & 1U));
}

void IRAM_ATTR capture(void* argument) {
    auto& c = *static_cast<Capture*>(argument);
    portENTER_CRITICAL_ISR(&capture_mux);
    const int movement = c.decoder.sample(readAB(c)) * c.direction;
    if (movement != 0) {
        c.last_direction = movement;
        if ((movement > 0 && (c.pending == INT32_MAX || c.total == INT32_MAX)) ||
            (movement < 0 && (c.pending == INT32_MIN || c.total == INT32_MIN))) {
            if (c.overruns != UINT32_MAX) ++c.overruns;
        }
        if (!((movement > 0 && c.pending == INT32_MAX) ||
              (movement < 0 && c.pending == INT32_MIN))) c.pending += movement;
        if (!((movement > 0 && c.total == INT32_MAX) ||
              (movement < 0 && c.total == INT32_MIN))) c.total += movement;
    }
    portEXIT_CRITICAL_ISR(&capture_mux);
}

bool checked(esp_err_t result) {
    if (result == ESP_OK) return true;
    Serial.printf("[INPUT] knob interrupt setup failed: %s\n", esp_err_to_name(result));
    return false;
}

bool startCapture(Capture& c) {
    c.decoder.initialize(readAB(c));
    c.pending = c.total = 0;
    c.overruns = 0;
    c.last_direction = 0;
    return checked(gpio_set_intr_type(static_cast<gpio_num_t>(c.a), GPIO_INTR_ANYEDGE)) &&
           checked(gpio_set_intr_type(static_cast<gpio_num_t>(c.b), GPIO_INTR_ANYEDGE)) &&
           checked(gpio_isr_handler_add(static_cast<gpio_num_t>(c.a), capture, &c)) &&
           checked(gpio_isr_handler_add(static_cast<gpio_num_t>(c.b), capture, &c));
}

void drain(Capture& c, input::Encoder& out) {
    portENTER_CRITICAL(&capture_mux);
    out.delta = c.pending;
    c.pending = 0;
    out.total = c.total;
    out.invalid = c.decoder.invalid;
    out.overruns = c.overruns;
    out.ab = c.decoder.ab;
    out.last_direction = c.last_direction;
    portEXIT_CRITICAL(&capture_mux);
}

}  // namespace

namespace input {

bool initialize(std::uint32_t now_ms) {
    for (const int pin : {board::PIN_LEFT_A, board::PIN_LEFT_B, board::PIN_LEFT_BUTTON,
                          board::PIN_RIGHT_A, board::PIN_RIGHT_B, board::PIN_RIGHT_BUTTON}) {
        pinMode(pin, INPUT_PULLUP);
    }
    current = State{};
    current.left.button.initialize(digitalRead(board::PIN_LEFT_BUTTON) == LOW, now_ms);
    current.right.button.initialize(digitalRead(board::PIN_RIGHT_BUTTON) == LOW, now_ms);
    return checked(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)) &&
           startCapture(left) && startCapture(right);
}

void update(std::uint32_t now_ms) {
    drain(left, current.left);
    drain(right, current.right);
    current.left.button.update(digitalRead(board::PIN_LEFT_BUTTON) == LOW, now_ms);
    current.right.button.update(digitalRead(board::PIN_RIGHT_BUTTON) == LOW, now_ms);
}

const State& state() { return current; }

const char* eventName(ButtonEvent event) {
    switch (event) {
        case ButtonEvent::Pressed: return "PRESS";
        case ButtonEvent::Released: return "RELEASE";
        case ButtonEvent::ShortPress: return "CLICK";
        case ButtonEvent::LongPress: return "HOLD";
        default: return "NONE";
    }
}

}  // namespace input
