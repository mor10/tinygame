#include <cassert>
#include <cstdio>

#include "button_gesture.h"

using Event = button_gesture::Gesture::Event;

void shortChordPausesAfterRelease() {
    button_gesture::Gesture gesture;
    assert(gesture.update(false, false, 0) == Event::None && !gesture.blocked());
    assert(gesture.update(true, false, 100) == Event::None && !gesture.blocked());
    assert(gesture.update(true, true, 400) == Event::None && gesture.blocked());
    assert(gesture.update(true, true, 3399) == Event::None);
    assert(gesture.update(false, true, 3500) == Event::None && gesture.blocked());
    assert(gesture.update(false, false, 3600) == Event::Pause && gesture.blocked());
    assert(gesture.update(false, false, 3601) == Event::None && !gesture.blocked());
}

void threeSecondChordSleepsAndRequiresRelease() {
    button_gesture::Gesture gesture;
    assert(gesture.update(true, true, 100) == Event::None && gesture.blocked());
    assert(gesture.update(true, true, 3099) == Event::None);
    assert(gesture.update(true, true, 3100) == Event::Sleep);
    assert(gesture.update(true, true, 7000) == Event::None && gesture.blocked());
    assert(gesture.update(false, true, 7100) == Event::None && gesture.blocked());
    assert(gesture.update(false, false, 7200) == Event::None && gesture.blocked());
    assert(gesture.update(false, false, 7201) == Event::None && !gesture.blocked());
}

void soloHoldsRemainAvailableAndInterruptedChordRestarts() {
    button_gesture::Gesture gesture;
    assert(gesture.update(false, true, 100) == Event::None && !gesture.blocked());
    assert(gesture.update(false, true, 5000) == Event::None && !gesture.blocked());
    assert(gesture.update(false, false, 5100) == Event::None && !gesture.blocked());
    assert(gesture.update(true, true, 6000) == Event::None);
    assert(gesture.update(true, false, 8999) == Event::None && gesture.blocked());
    assert(gesture.update(true, true, 9000) == Event::None);
    assert(gesture.update(true, true, 11999) == Event::None);
    assert(gesture.update(true, true, 12000) == Event::Sleep);
    assert(gesture.update(false, false, 12000) == Event::None);
}

void timingHandlesWraparoundAndBoundaryRelease() {
    button_gesture::Gesture gesture;
    assert(gesture.update(true, true, UINT32_MAX - 1000) == Event::None);
    assert(gesture.update(true, true, 1998) == Event::None);
    assert(gesture.update(false, false, 1999) == Event::Sleep);

    button_gesture::Gesture boundary;
    assert(boundary.update(true, true, 10) == Event::None);
    assert(boundary.update(false, false, 3010) == Event::Sleep);
}

int main() {
    shortChordPausesAfterRelease();
    threeSecondChordSleepsAndRequiresRelease();
    soloHoldsRemainAvailableAndInterruptedChordRestarts();
    timingHandlesWraparoundAndBoundaryRelease();
    std::puts("button gesture tests passed");
}
