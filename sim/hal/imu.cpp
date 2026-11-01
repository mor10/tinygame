#include "imu.h"

#include <algorithm>
#include <cmath>
#include "sim_hal.h"

namespace {
imu::Reading current;
bool asleep = false;
}  // namespace

namespace imu {
bool initialize() {
    current = Reading{};
    current.accel_g.z = 1.0f;
    asleep = false;
    return true;
}
bool available() { return true; }
bool healthy() { return !asleep; }
bool setSleeping(bool value) {
    asleep = value;
    return true;
}
void update(std::uint32_t) {}
const Reading& reading() { return current; }
}  // namespace imu

namespace sim {
void tilt(float x, float y) {
    if (sleeping() || !std::isfinite(x) || !std::isfinite(y)) return;
    current.tilt_x = std::clamp(x, -1.0f, 1.0f);
    current.tilt_y = std::clamp(y, -1.0f, 1.0f);
    current.accel_g.x = current.tilt_x;
    current.accel_g.y = current.tilt_y;
    current.accel_g.z = std::sqrt(std::max(
        0.0f, 1.0f - current.tilt_x * current.tilt_x - current.tilt_y * current.tilt_y));
}
}  // namespace sim
