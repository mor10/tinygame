#include <cassert>
#include <cstdio>

#include "status_light.h"

using status_light::Pattern;
using status_light::Rgb;
using status_light::StatusLight;

namespace {

constexpr Rgb RED{255, 0, 0};
constexpr Rgb WHITE{255, 255, 255};

bool off(Rgb c) {
    return c.r == 0 && c.g == 0 && c.b == 0;
}

void percentScalesAndCapsOutput() {
    StatusLight light;
    light.setBase(WHITE);
    const Rgb full = light.output(0, 100);
    assert(full.r == status_light::MAX_CHANNEL);
    const Rgb dim = light.output(0, 10);
    assert(dim.r < full.r && !off(dim));
    assert(off(light.output(0, 0)));
}

void blinkTurnsOffHalfTheTime() {
    StatusLight light;
    light.setBase(RED, Pattern::Blink);
    assert(!off(light.output(100, 100)));
    assert(off(light.output(status_light::BLINK_PERIOD_MS / 2 + 100, 100)));
}

void breatheVariesBrightness() {
    StatusLight light;
    light.setBase(RED, Pattern::Breathe);
    const Rgb low = light.output(0, 100);
    const Rgb high = light.output(status_light::BREATHE_PERIOD_MS / 2, 100);
    assert(low.r < high.r);
}

void flashOverridesBaseThenEnds() {
    StatusLight light;
    light.setBase(RED);
    light.flash(WHITE, 100, 1000);
    assert(light.output(1050, 100).g > 0);
    assert(light.output(1100, 100).g == 0);
}

void shortFlashDoesNotCutOffLongerFlash() {
    StatusLight light;
    light.flash(RED, 400, 0);
    light.flash(WHITE, 60, 10);
    assert(light.output(20, 100).g == 0);  // still red
    light.flash(WHITE, 600, 30);           // longer flash replaces it
    assert(light.output(40, 100).g > 0);
}

}  // namespace

int main() {
    percentScalesAndCapsOutput();
    blinkTurnsOffHalfTheTime();
    breatheVariesBrightness();
    flashOverridesBaseThenEnds();
    shortFlashDoesNotCutOffLongerFlash();
    std::puts("status_light tests passed");
    return 0;
}
