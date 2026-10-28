#include "power.h"

#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"

#include "board_config.h"

namespace {

constexpr auto LEFT_SW = static_cast<gpio_num_t>(board::PIN_LEFT_BUTTON);
constexpr auto RIGHT_SW = static_cast<gpio_num_t>(board::PIN_RIGHT_BUTTON);
constexpr std::uint64_t WAKE_MASK = (1ULL << LEFT_SW) | (1ULL << RIGHT_SW);
const char* wake_name = "POWER/RESET";

bool checked(esp_err_t result, const char* operation) {
    if (result == ESP_OK) return true;
    Serial.printf("[POWER] %s failed: %s\n", operation, esp_err_to_name(result));
    return false;
}

}  // namespace

namespace power {

bool initialize() {
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
        const auto mask = esp_sleep_get_ext1_wakeup_status();
        wake_name = (mask & WAKE_MASK) == WAKE_MASK ? "BOTH" :
                    (mask & (1ULL << LEFT_SW)) ? "LEFT" : "RIGHT";
    }
    Serial.printf("[POWER] wake=%s reset=%d\n", wake_name, static_cast<int>(esp_reset_reason()));
    gpio_deep_sleep_hold_dis();
    pinMode(board::PIN_LCD_BACKLIGHT, OUTPUT);
    digitalWrite(board::PIN_LCD_BACKLIGHT, LOW);
    return checked(gpio_hold_dis(static_cast<gpio_num_t>(board::PIN_LCD_BACKLIGHT)),
                   "release backlight hold") &&
           checked(rtc_gpio_deinit(LEFT_SW), "restore left switch GPIO") &&
           checked(rtc_gpio_deinit(RIGHT_SW), "restore right switch GPIO");
}

bool buttonsPressed() {
    return digitalRead(board::PIN_LEFT_BUTTON) == LOW ||
           digitalRead(board::PIN_RIGHT_BUTTON) == LOW;
}

bool prepareWake() {
    return checked(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL), "clear wake sources") &&
           checked(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON),
                   "retain RTC pull-ups") &&
           checked(rtc_gpio_pullup_en(LEFT_SW), "left RTC pull-up") &&
           checked(rtc_gpio_pulldown_dis(LEFT_SW), "left RTC pull-down disable") &&
           checked(rtc_gpio_pullup_en(RIGHT_SW), "right RTC pull-up") &&
           checked(rtc_gpio_pulldown_dis(RIGHT_SW), "right RTC pull-down disable") &&
           checked(esp_sleep_enable_ext1_wakeup(WAKE_MASK, ESP_EXT1_WAKEUP_ANY_LOW),
                   "knob switch wake");
}

const char* wakeName() { return wake_name; }

void enterSleep() {
    if (!buttonsPressed()) {
        gpio_deep_sleep_hold_en();
        esp_deep_sleep_start();
    }
}

}  // namespace power
