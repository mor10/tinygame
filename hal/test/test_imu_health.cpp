// Carried over from v1.0 tests/test_imu_health.cpp. Converted to the v2 harness (engine/test/tg_test.h):
// assert() became CHECK(), so one failure no longer stops the others.
#include "../../engine/test/tg_test.h"

#include "imu_health.h"

TG_TEST(imu_read_health_notices) {
    using Notice = imu::ReadHealth::Notice;
    imu::ReadHealth health;
    CHECK(health.record(true, 10) == Notice::None && health.ok);
    CHECK(health.record(false, 20) == Notice::Failed && !health.ok);
    CHECK(health.record(true, 30) == Notice::Recovered && health.ok);
    CHECK(health.record(false, 40) == Notice::None && !health.ok);
    CHECK(health.record(true, 50) == Notice::None && health.ok);
    CHECK(health.record(false, 5019) == Notice::None && !health.ok);
    CHECK(health.record(false, 5020) == Notice::Failed && !health.ok);
    CHECK(health.record(true, 5030) == Notice::Recovered && health.ok);

    health = {};
    CHECK(health.record(false, UINT32_MAX - 100) == Notice::Failed);
    CHECK(health.record(false, 4898) == Notice::None);
    CHECK(health.record(false, 4899) == Notice::Failed);
    CHECK(health.record(true, 4900) == Notice::Recovered);
}

int main() { return tg::test::run(); }
