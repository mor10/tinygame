#include <cassert>
#include <cstdio>

#include "sleep_gesture.h"

void exactTimingAndReleaseGate() {
    sleep_gesture::Gesture gesture;
    assert(!gesture.update(false, false, 0) && !gesture.blocked());
    assert(!gesture.update(true, false, 100) && !gesture.blocked());
    assert(!gesture.update(true, true, 400) && gesture.blocked());
    assert(!gesture.update(true, true, 3399));
    assert(gesture.update(true, true, 3400));
    assert(!gesture.update(true, true, 7000) && gesture.blocked());
    assert(!gesture.update(false, true, 7100) && gesture.blocked());
    assert(!gesture.update(false, false, 7200) && gesture.blocked());
    assert(!gesture.update(false, false, 7201) && !gesture.blocked());
    assert(!gesture.update(true, true, 8000));
    assert(gesture.update(true, true, 11000));
}

void interruptedOverlapStartsAgain() {
    sleep_gesture::Gesture gesture;
    assert(!gesture.update(true, true, 0));
    assert(!gesture.update(true, false, 2999) && gesture.blocked());
    assert(!gesture.update(true, true, 3000));
    assert(!gesture.update(true, true, 5999));
    assert(gesture.update(true, true, 6000));
}

void wraparoundAndSingleKnobs() {
    sleep_gesture::Gesture gesture;
    assert(!gesture.update(false, true, 100));
    assert(!gesture.update(false, true, 5000) && !gesture.blocked());
    assert(!gesture.update(false, false, 5100) && !gesture.blocked());
    assert(!gesture.update(true, true, UINT32_MAX - 1000));
    assert(!gesture.update(true, true, 1998));
    assert(gesture.update(true, true, 1999));
}

int main() {
    exactTimingAndReleaseGate();
    interruptedOverlapStartsAgain();
    wraparoundAndSingleKnobs();
    std::puts("sleep gesture tests passed");
}
