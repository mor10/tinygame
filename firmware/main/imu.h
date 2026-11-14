#pragma once
// QMI8658 accelerometer driver with the scope's tilt pipeline:
// raw -> board rotation (axis/sign map) -> calibration offset -> ~8 Hz low-pass ->
// angle, +-35 deg mapped to -1..1 with a 3 deg dead zone. Shake = high-passed magnitude above 1.8 g.
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    float g[3];        // raw acceleration in g, PCB axes
    float tilt_x;      // -1..1, tilt right = positive (once the map is confirmed on the bench)
    float tilt_y;      // -1..1, tilt toward the user's bottom edge = positive
    float ang_x, ang_y;// degrees after calibration and filtering
    uint8_t shake;     // event since the previous snapshot
    uint8_t ok;        // driver healthy (WHO_AM_I matched and reads succeed)
} imu_state_t;

esp_err_t imu_init(void);
void imu_snapshot(imu_state_t *out);                    // consumes the shake event
void imu_zero(void);                                    // "hold me flat": current angles become zero
// Screen axis map: screen X = sign * accel[axis]. axis 0..2 = PCB X, Y, Z; sign +1 or -1.
void imu_set_map(int sx_axis, int sx_sign, int sy_axis, int sy_sign);
void imu_get_map(int *sx_axis, int *sx_sign, int *sy_axis, int *sy_sign);
