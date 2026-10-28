#include "imu.h"
#include "imu_health.h"

#include <Arduino.h>
#include <Wire.h>

#include "board_config.h"

namespace {

// QMI8658 registers (datasheet section 5).
constexpr std::uint8_t REG_WHO_AM_I = 0x00;
constexpr std::uint8_t REG_CTRL1 = 0x02;  // serial interface
constexpr std::uint8_t REG_CTRL2 = 0x03;  // accelerometer range + rate
constexpr std::uint8_t REG_CTRL3 = 0x04;  // gyroscope range + rate
constexpr std::uint8_t REG_CTRL7 = 0x08;  // sensor enable
constexpr std::uint8_t REG_AX_L = 0x35;   // AX, AY, AZ, GX, GY, GZ (12 bytes)
constexpr std::uint8_t REG_RESET = 0x60;
constexpr std::uint8_t WHO_AM_I_VALUE = 0x05;

constexpr std::uint8_t CTRL1_AUTO_INCREMENT = 0x40;
constexpr std::uint8_t CTRL2_ACCEL_4G_125HZ = 0x16;
constexpr std::uint8_t CTRL3_GYRO_512DPS_125HZ = 0x56;
constexpr std::uint8_t CTRL7_ACCEL_GYRO_ON = 0x03;
constexpr float ACCEL_LSB_PER_G = 8192.0f;
constexpr float GYRO_LSB_PER_DPS = 64.0f;

constexpr std::uint32_t SAMPLE_INTERVAL_MS = 10;
// Low-pass for tilt: higher = snappier, lower = smoother.
constexpr float TILT_FILTER_ALPHA = 0.25f;

// Chip axes -> screen axes (+x right, +y down the screen, +z toward the
// player), for the board mounted with the display in landscape.
// Verified on the board by tilting it: lowering the right side moves the
// Diagnostics tilt dot right; lowering the bottom edge moves it down.
struct AxisMap {
    std::uint8_t source;  // chip axis 0=x 1=y 2=z
    float sign;
};
constexpr AxisMap SCREEN_X = {1, 1.0f};
constexpr AxisMap SCREEN_Y = {0, 1.0f};
constexpr AxisMap SCREEN_Z = {2, 1.0f};

bool present = false;
imu::ReadHealth read_health;
std::uint32_t last_sample_at = 0;
imu::Reading current;

bool writeRegister(std::uint8_t reg, std::uint8_t value) {
    Wire.beginTransmission(board::IMU_I2C_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readRegisters(std::uint8_t reg, std::uint8_t* out, std::uint8_t count) {
    Wire.beginTransmission(board::IMU_I2C_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom(board::IMU_I2C_ADDRESS, count) != count) {
        return false;
    }
    for (std::uint8_t i = 0; i < count; ++i) {
        out[i] = static_cast<std::uint8_t>(Wire.read());
    }
    return true;
}

float axis(const float chip[3], AxisMap map) {
    return chip[map.source] * map.sign;
}

float clampUnit(float v) {
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

}  // namespace

namespace imu {

bool initialize() {
    Wire.begin(board::PIN_IMU_SDA, board::PIN_IMU_SCL, 400000);
    Wire.setTimeOut(5);
    std::uint8_t id = 0;
    if (!readRegisters(REG_WHO_AM_I, &id, 1) || id != WHO_AM_I_VALUE) {
        Serial.printf("[IMU] QMI8658 not found (id=0x%02X)\n", id);
        return false;
    }
    if (!writeRegister(REG_RESET, 0xB0)) {
        Serial.println("[IMU] QMI8658 reset failed");
        return false;
    }
    delay(15);
    present = writeRegister(REG_CTRL1, CTRL1_AUTO_INCREMENT) &&
              writeRegister(REG_CTRL2, CTRL2_ACCEL_4G_125HZ) &&
              writeRegister(REG_CTRL3, CTRL3_GYRO_512DPS_125HZ) &&
              writeRegister(REG_CTRL7, CTRL7_ACCEL_GYRO_ON);
    Serial.println(present ? "[IMU] QMI8658 ready" : "[IMU] QMI8658 setup failed");
    return present;
}

bool available() {
    return present;
}

bool healthy() {
    return present && read_health.ok;
}

bool setSleeping(bool sleeping) {
    if (!present) return true;
    const bool ok = writeRegister(REG_CTRL7, sleeping ? 0 : CTRL7_ACCEL_GYRO_ON);
    if (!ok) Serial.println("[IMU] sensor power change failed");
    return ok;
}

void update(std::uint32_t now_ms) {
    if (!present || now_ms - last_sample_at < SAMPLE_INTERVAL_MS) {
        return;
    }
    last_sample_at = now_ms;

    std::uint8_t raw[12];
    const bool success = readRegisters(REG_AX_L, raw, sizeof(raw));
    const auto notice = read_health.record(success, now_ms);
    if (notice == ReadHealth::Notice::Failed)
        Serial.println("[IMU] read failed; motion data unavailable");
    if (notice == ReadHealth::Notice::Recovered)
        Serial.println("[IMU] readings recovered");
    if (!success) {
        current.accel_g = {};
        current.gyro_dps = {};
        return;
    }
    float accel[3];
    float gyro[3];
    for (int i = 0; i < 3; ++i) {
        const auto a = static_cast<std::int16_t>(raw[2 * i] | (raw[2 * i + 1] << 8));
        const auto g = static_cast<std::int16_t>(raw[6 + 2 * i] | (raw[6 + 2 * i + 1] << 8));
        accel[i] = a / ACCEL_LSB_PER_G;
        gyro[i] = g / GYRO_LSB_PER_DPS;
    }
    current.accel_g = {axis(accel, SCREEN_X), axis(accel, SCREEN_Y), axis(accel, SCREEN_Z)};
    current.gyro_dps = {axis(gyro, SCREEN_X), axis(gyro, SCREEN_Y), axis(gyro, SCREEN_Z)};

    // At rest the accelerometer measures "up"; a marble rolls the other way.
    current.tilt_x += TILT_FILTER_ALPHA * (clampUnit(-current.accel_g.x) - current.tilt_x);
    current.tilt_y += TILT_FILTER_ALPHA * (clampUnit(-current.accel_g.y) - current.tilt_y);
}

const Reading& reading() {
    return current;
}

}  // namespace imu
