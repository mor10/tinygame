// Input driver: PCNT quadrature decoding for both EC11 encoders, debounced clicks, chords.
#include <string.h>
#include "input.h"
#include "board.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "input";

typedef struct {
    int a, b, click, dir;
    pcnt_unit_handle_t unit;
    int last_count;
    // click debounce
    int raw_prev, stable_ms, down;
    int64_t down_since_us;
    int long_fired;
} knob_t;

static knob_t k[2] = {
    { .a = BOARD_ENC_L_A, .b = BOARD_ENC_L_B, .click = BOARD_ENC_L_CLICK, .dir = BOARD_ENC_L_DIR },
    { .a = BOARD_ENC_R_A, .b = BOARD_ENC_R_B, .click = BOARD_ENC_R_CLICK, .dir = BOARD_ENC_R_DIR },
};

static input_state_t st;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t press_us[2];
static int chord_armed, exit_fired;

#define PCNT_LIMIT 30000   // count is recentred long before this

static esp_err_t knob_init(knob_t *n)
{
    gpio_config_t g = { .pin_bit_mask = (1ULL << n->a) | (1ULL << n->b) | (1ULL << n->click),
                        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_RETURN_ON_ERROR(gpio_config(&g), TAG, "gpio");

    pcnt_unit_config_t uc = { .low_limit = -PCNT_LIMIT, .high_limit = PCNT_LIMIT };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&uc, &n->unit), TAG, "unit");
    pcnt_glitch_filter_config_t fc = { .max_glitch_ns = 1000 };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(n->unit, &fc), TAG, "filter");

    pcnt_chan_config_t ca = { .edge_gpio_num = n->a, .level_gpio_num = n->b };
    pcnt_chan_config_t cb = { .edge_gpio_num = n->b, .level_gpio_num = n->a };
    pcnt_channel_handle_t chan_a, chan_b;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(n->unit, &ca, &chan_a), TAG, "chan a");
    ESP_RETURN_ON_ERROR(pcnt_new_channel(n->unit, &cb, &chan_b), TAG, "chan b");
    // Standard 4x quadrature: both edges of both channels count, level of the other channel sets direction.
    pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(n->unit), TAG, "enable");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(n->unit), TAG, "clear");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(n->unit), TAG, "start");
    n->raw_prev = gpio_get_level(n->click);
    return ESP_OK;
}

static void input_task(void *arg)
{
    int64_t last_us = esp_timer_get_time();
    for (;;) {
        vTaskDelay(1);
        int64_t now = esp_timer_get_time();
        float dt = (now - last_us) / 1e6f;
        last_us = now;

        portENTER_CRITICAL(&lock);
        for (int i = 0; i < 2; i++) {
            knob_t *n = &k[i];
            int c = 0;
            pcnt_unit_get_count(n->unit, &c);
            int d = (c - n->last_count) * n->dir;
            n->last_count = c;
            st.pos[i] += d;
            st.delta[i] += d;
            float inst = dt > 0 ? d / dt : 0;
            st.speed[i] += (inst - st.speed[i]) * 0.1f;
            if (c > PCNT_LIMIT / 2 || c < -PCNT_LIMIT / 2) {   // recentre; a few edges lost only after ~190 turns
                pcnt_unit_clear_count(n->unit);
                n->last_count = 0;
            }

            // Click: 5 ms debounce, active low.
            int raw = !gpio_get_level(n->click);
            if (raw == n->raw_prev) { if (n->stable_ms < 1000) n->stable_ms++; }
            else { n->raw_prev = raw; n->stable_ms = 0; }
            if (n->stable_ms >= INPUT_DEBOUNCE_MS && raw != n->down) {
                n->down = raw;
                st.down[i] = raw;
                if (raw) { st.pressed[i] = 1; n->down_since_us = now; n->long_fired = 0; press_us[i] = now; }
                else      { st.released[i] = 1; }
            }
            if (n->down && !n->long_fired && now - n->down_since_us >= INPUT_LONG_MS * 1000LL) {
                st.long_press[i] = 1;
                n->long_fired = 1;
            }
        }
        // Chords: pause when the second click lands within 150 ms of the first; exit when both stay down 3 s.
        if (k[0].down && k[1].down) {
            if (!chord_armed) {
                chord_armed = 1; exit_fired = 0;
                int64_t gap = press_us[0] > press_us[1] ? press_us[0] - press_us[1] : press_us[1] - press_us[0];
                if (gap <= INPUT_CHORD_MS * 1000LL) st.chord_pause = 1;
            }
            int64_t both_since = press_us[0] > press_us[1] ? press_us[0] : press_us[1];
            if (!exit_fired && now - both_since >= INPUT_EXIT_MS * 1000LL) { st.chord_exit = 1; exit_fired = 1; }
        } else {
            chord_armed = 0;
        }
        portEXIT_CRITICAL(&lock);
    }
}

esp_err_t input_init(void)
{
    for (int i = 0; i < 2; i++) ESP_RETURN_ON_ERROR(knob_init(&k[i]), TAG, "knob");
    xTaskCreatePinnedToCore(input_task, "input", 3072, NULL, 6, NULL, 0);
    return ESP_OK;
}

void input_snapshot(input_state_t *out)
{
    portENTER_CRITICAL(&lock);
    *out = st;
    for (int i = 0; i < 2; i++) { st.delta[i] = 0; st.pressed[i] = st.released[i] = st.long_press[i] = 0; }
    st.chord_pause = st.chord_exit = 0;
    portEXIT_CRITICAL(&lock);
}

void input_zero(int side)
{
    portENTER_CRITICAL(&lock);
    st.pos[side] = 0;
    portEXIT_CRITICAL(&lock);
}
