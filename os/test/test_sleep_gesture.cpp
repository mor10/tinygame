// Carried over from v1.0 tests/test_sleep_gesture.cpp. Converted to the v2 harness (engine/test/tg_test.h):
// assert() became CHECK(), so one failure no longer stops the others.
#include "../../engine/test/tg_test.h"

#include "sleep_gesture.h"

TG_TEST(exactTimingAndReleaseGate) {
    sleep_gesture::Gesture gesture;
    CHECK(!gesture.update(false, false, 0) && !gesture.blocked());
    CHECK(!gesture.update(true, false, 100) && !gesture.blocked());
    CHECK(!gesture.update(true, true, 400) && gesture.blocked());
    CHECK(!gesture.update(true, true, 3399));
    CHECK(gesture.update(true, true, 3400));
    CHECK(!gesture.update(true, true, 7000) && gesture.blocked());
    CHECK(!gesture.update(false, true, 7100) && gesture.blocked());
    CHECK(!gesture.update(false, false, 7200) && gesture.blocked());
    CHECK(!gesture.update(false, false, 7201) && !gesture.blocked());
    CHECK(!gesture.update(true, true, 8000));
    CHECK(gesture.update(true, true, 11000));
}

TG_TEST(interruptedOverlapStartsAgain) {
    sleep_gesture::Gesture gesture;
    CHECK(!gesture.update(true, true, 0));
    CHECK(!gesture.update(true, false, 2999) && gesture.blocked());
    CHECK(!gesture.update(true, true, 3000));
    CHECK(!gesture.update(true, true, 5999));
    CHECK(gesture.update(true, true, 6000));
}

TG_TEST(wraparoundAndSingleKnobs) {
    sleep_gesture::Gesture gesture;
    CHECK(!gesture.update(false, true, 100));
    CHECK(!gesture.update(false, true, 5000) && !gesture.blocked());
    CHECK(!gesture.update(false, false, 5100) && !gesture.blocked());
    CHECK(!gesture.update(true, true, UINT32_MAX - 1000));
    CHECK(!gesture.update(true, true, 1998));
    CHECK(gesture.update(true, true, 1999));
}

int main() { return tg::test::run(); }
