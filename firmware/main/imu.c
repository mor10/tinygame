// QMI8658 accelerometer driver: 224 Hz, +-4 g, polled by a 200 Hz task on core 0.
#include <math.h>
#include <string.h>
#include "imu.h"
#include "board.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "imu";

#define QMI_ADDR      0x6B
#define REG_WHO_AM_I  0x00   // reads 0x05
#define REG_CTRL1     0x02   // 0x40: address auto-increment
#define REG_CTRL2     0x03   // accel: range bits 6:4, ODR bits 3:0
#define REG_CTRL7     0x08   // bit 0 enables the accelerometer
#define REG_AX_L      0x35
#define LSB_PER_G     8192.f // +-4 g

static i2c_master_dev_handle_t dev;
static imu_state_t st;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static int map_axis[2] = { BOARD_IMU_SX_AXIS, BOARD_IMU_SY_AXIS };
static int map_sign[2] = { BOARD_IMU_SX_SIGN, BOARD_IMU_SY_SIGN };
static float off_x, off_y;          // calibration offsets, degrees
static float raw_ang[2];            // filtered angles before calibration

static esp_err_t wr(uint8_t reg, uint8_t v)
{
    uint8_t b[2] = { reg, v };
    return i2c_master_transmit(dev, b, 2, 20);
}

static esp_err_t rd(uint8_t reg, uint8_t *out, int n)
{
    return i2c_master_transmit_receive(dev, &reg, 1, out, n, 20);
}

static float apply_dead(float ang_deg)
{
    float a = fabsf(ang_deg);
    if (a < 3.f) return 0.f;
    float v = (a - 3.f) / (35.f - 3.f);
    if (v > 1.f) v = 1.f;
    return ang_deg < 0 ? -v : v;
}

static void imu_task(void *arg)
{
    const float alpha = 1.f - expf(-2.f * (float)M_PI * 8.f / 200.f);   // ~8 Hz low-pass at 200 Hz
    float mag_lp = 1.f;
    int64_t last_shake = 0;
    int fail = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5));
        uint8_t b[6];
        if (rd(REG_AX_L, b, 6) != ESP_OK) {
            if (++fail > 20) { portENTER_CRITICAL(&lock); st.ok = 0; portEXIT_CRITICAL(&lock); }
            continue;
        }
        fail = 0;
        float g[3];
        for (int i = 0; i < 3; i++) g[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) / LSB_PER_G;

        float sx = map_sign[0] * g[map_axis[0]];
        float sy = map_sign[1] * g[map_axis[1]];
        // The third axis is whichever is neither mapped axis; it carries gravity when the board lies flat.
        int za = 3 - map_axis[0] - map_axis[1];
        float sz = g[za];
        float ax = atan2f(sx, sqrtf(sy * sy + sz * sz)) * 180.f / (float)M_PI;
        float ay = atan2f(sy, sqrtf(sx * sx + sz * sz)) * 180.f / (float)M_PI;

        float mag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
        mag_lp += (mag - mag_lp) * 0.05f;
        int64_t now = esp_timer_get_time();

        portENTER_CRITICAL(&lock);
        memcpy(st.g, g, sizeof g);
        raw_ang[0] += (ax - raw_ang[0]) * alpha;
        raw_ang[1] += (ay - raw_ang[1]) * alpha;
        st.ang_x = raw_ang[0] - off_x;
        st.ang_y = raw_ang[1] - off_y;
        st.tilt_x = apply_dead(st.ang_x);
        st.tilt_y = apply_dead(st.ang_y);
        if (fabsf(mag - mag_lp) > 1.8f && now - last_shake > 300000) { st.shake = 1; last_shake = now; }
        st.ok = 1;
        portEXIT_CRITICAL(&lock);
    }
}

esp_err_t imu_init(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0, .sda_io_num = BOARD_IMU_SDA, .scl_io_num = BOARD_IMU_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bc, &bus), TAG, "bus");
    i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = QMI_ADDR, .scl_speed_hz = 400000 };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dc, &dev), TAG, "dev");

    uint8_t who = 0;
    ESP_RETURN_ON_ERROR(rd(REG_WHO_AM_I, &who, 1), TAG, "who");
    ESP_RETURN_ON_FALSE(who == 0x05, ESP_ERR_NOT_FOUND, TAG, "WHO_AM_I 0x%02X", who);
    ESP_RETURN_ON_ERROR(wr(REG_CTRL1, 0x40), TAG, "ctrl1");
    ESP_RETURN_ON_ERROR(wr(REG_CTRL2, 0x15), TAG, "ctrl2");   // +-4 g (001), ODR 0101 = about 224 Hz
    ESP_RETURN_ON_ERROR(wr(REG_CTRL7, 0x01), TAG, "ctrl7");   // accelerometer on
    vTaskDelay(pdMS_TO_TICKS(20));
    xTaskCreatePinnedToCore(imu_task, "imu", 3072, NULL, 5, NULL, 0);
    return ESP_OK;
}

void imu_snapshot(imu_state_t *out)
{
    portENTER_CRITICAL(&lock);
    *out = st;
    st.shake = 0;
    portEXIT_CRITICAL(&lock);
}

void imu_zero(void)
{
    portENTER_CRITICAL(&lock);
    off_x = raw_ang[0];
    off_y = raw_ang[1];
    portEXIT_CRITICAL(&lock);
}

void imu_set_map(int sx_axis, int sx_sign, int sy_axis, int sy_sign)
{
    if (sx_axis < 0 || sx_axis > 2 || sy_axis < 0 || sy_axis > 2 || sx_axis == sy_axis) return;
    portENTER_CRITICAL(&lock);
    map_axis[0] = sx_axis; map_sign[0] = sx_sign < 0 ? -1 : 1;
    map_axis[1] = sy_axis; map_sign[1] = sy_sign < 0 ? -1 : 1;
    off_x = off_y = 0;
    portEXIT_CRITICAL(&lock);
}

void imu_get_map(int *sx_axis, int *sx_sign, int *sy_axis, int *sy_sign)
{
    *sx_axis = map_axis[0]; *sx_sign = map_sign[0]; *sy_axis = map_axis[1]; *sy_sign = map_sign[1];
}
