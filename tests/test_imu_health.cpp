#include <cassert>

#include "../src/hal/imu_health.h"

int main() {
    using Notice = imu::ReadHealth::Notice;
    imu::ReadHealth health;
    assert(health.record(true, 10) == Notice::None && health.ok);
    assert(health.record(false, 20) == Notice::Failed && !health.ok);
    assert(health.record(true, 30) == Notice::Recovered && health.ok);
    assert(health.record(false, 40) == Notice::None && !health.ok);
    assert(health.record(true, 50) == Notice::None && health.ok);
    assert(health.record(false, 5019) == Notice::None && !health.ok);
    assert(health.record(false, 5020) == Notice::Failed && !health.ok);
    assert(health.record(true, 5030) == Notice::Recovered && health.ok);

    health = {};
    assert(health.record(false, UINT32_MAX - 100) == Notice::Failed);
    assert(health.record(false, 4898) == Notice::None);
    assert(health.record(false, 4899) == Notice::Failed);
    assert(health.record(true, 4900) == Notice::Recovered);
}
