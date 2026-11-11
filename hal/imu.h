#pragma once

#include <cstdint>

#include "tg_types.h"

// QMI8658 6-axis motion sensor (accelerometer + gyroscope), in screen axes.
namespace imu {

struct Reading {
    tg::Vec3 accel_g;
    tg::Vec3 gyro_dps;
    float tilt_x = 0.0f;  // smoothed, -1..1, see tg::Input
    float tilt_y = 0.0f;
};

// Returns false if the sensor does not answer; reads then stay at zero.
bool initialize();
bool available();
// True while the sensor is present and its last read succeeded.
bool healthy();
// Powers the sensor down (true) or back up (false) around deep sleep.
bool setSleeping(bool sleeping);
// Samples the sensor at most every SAMPLE_INTERVAL_MS. Call every loop.
void update(std::uint32_t now_ms);
const Reading& reading();

}  // namespace imu
