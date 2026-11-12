#include "encoder_counter.h"

#if TG_BOARD == 147

#include <Arduino.h>
#include <driver/pcnt.h>
#include <esp_intr_alloc.h>

#include "board_config.h"
#include "encoder_counter_logic.h"

namespace {

constexpr std::int16_t LIMIT = 30000;
portMUX_TYPE overflow_mux = portMUX_INITIALIZER_UNLOCKED;
bool overflowed[2] = {};
bool ready = false;

void IRAM_ATTR onLimit(void* argument) {
    auto* flag = static_cast<bool*>(argument);
    portENTER_CRITICAL_ISR(&overflow_mux);
    *flag = true;
    portEXIT_CRITICAL_ISR(&overflow_mux);
}

bool checked(esp_err_t result) {
    if (result == ESP_OK) return true;
    Serial.printf("[DIAG] PCNT error: %s\n", esp_err_to_name(result));
    return false;
}

bool configure(pcnt_unit_t unit, int a, int b) {
    for (int i = 0; i < 2; ++i) {
        const auto channel = diagnostics::COUNTER_CHANNELS[i];
        pcnt_config_t config{};
        config.pulse_gpio_num = channel.pulse_is_a ? a : b;
        config.ctrl_gpio_num = channel.pulse_is_a ? b : a;
        config.lctrl_mode = PCNT_MODE_KEEP;
        config.hctrl_mode = PCNT_MODE_REVERSE;
        config.pos_mode = channel.rising_when_control_low > 0 ? PCNT_COUNT_INC : PCNT_COUNT_DEC;
        config.neg_mode = channel.rising_when_control_low > 0 ? PCNT_COUNT_DEC : PCNT_COUNT_INC;
        config.counter_h_lim = LIMIT;
        config.counter_l_lim = -LIMIT;
        config.unit = unit;
        config.channel = static_cast<pcnt_channel_t>(i);
        if (!checked(pcnt_unit_config(&config))) return false;
    }
    return checked(pcnt_counter_pause(unit)) &&
           checked(pcnt_filter_disable(unit)) &&
           checked(pcnt_counter_clear(unit));
}

bool start(pcnt_unit_t unit, bool* flag) {
    return checked(pcnt_isr_handler_add(unit, onLimit, flag)) &&
           checked(pcnt_event_enable(unit, PCNT_EVT_H_LIM)) &&
           checked(pcnt_event_enable(unit, PCNT_EVT_L_LIM)) &&
           checked(pcnt_intr_enable(unit)) &&
           checked(pcnt_counter_resume(unit));
}

}  // namespace

namespace diagnostics::counter {

bool initialize() {
    if (!configure(PCNT_UNIT_0, board::PIN_LEFT_A, board::PIN_LEFT_B) ||
        !configure(PCNT_UNIT_1, board::PIN_RIGHT_A, board::PIN_RIGHT_B) ||
        !checked(pcnt_isr_service_install(ESP_INTR_FLAG_IRAM)) ||
        !start(PCNT_UNIT_0, &overflowed[0]) ||
        !start(PCNT_UNIT_1, &overflowed[1])) return false;
    ready = true;
    return true;
}

bool read(Reading& reading) {
    if (!ready) return false;
    std::int16_t left = 0;
    std::int16_t right = 0;
    if (!checked(pcnt_get_counter_value(PCNT_UNIT_0, &left)) ||
        !checked(pcnt_get_counter_value(PCNT_UNIT_1, &right))) return false;
    portENTER_CRITICAL(&overflow_mux);
    const bool left_overflow = overflowed[0];
    const bool right_overflow = overflowed[1];
    portEXIT_CRITICAL(&overflow_mux);
    reading = {left * board::LEFT_DIRECTION, right * board::RIGHT_DIRECTION,
               left_overflow, right_overflow};
    return true;
}

}  // namespace diagnostics::counter

#else

namespace diagnostics::counter {
bool initialize() { return false; }
bool read(Reading&) { return false; }
}  // namespace diagnostics::counter

#endif
