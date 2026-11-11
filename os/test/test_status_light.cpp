// Carried over from v1.0 tests/test_status_light.cpp. Converted to the v2 harness (engine/test/tg_test.h):
// assert() became CHECK(), so one failure no longer stops the others.
#include "../../engine/test/tg_test.h"

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

TG_TEST(percentScalesAndCapsOutput) {
    StatusLight light;
    light.setBase(WHITE);
    const Rgb full = light.output(0, 100);
    CHECK(full.r == status_light::MAX_CHANNEL);
    const Rgb dim = light.output(0, 10);
    CHECK(dim.r < full.r && !off(dim));
    CHECK(off(light.output(0, 0)));
}

TG_TEST(blinkTurnsOffHalfTheTime) {
    StatusLight light;
    light.setBase(RED, Pattern::Blink);
    CHECK(!off(light.output(100, 100)));
    CHECK(off(light.output(status_light::BLINK_PERIOD_MS / 2 + 100, 100)));
}

TG_TEST(breatheVariesBrightness) {
    StatusLight light;
    light.setBase(RED, Pattern::Breathe);
    const Rgb low = light.output(0, 100);
    const Rgb high = light.output(status_light::BREATHE_PERIOD_MS / 2, 100);
    CHECK(low.r < high.r);
}

TG_TEST(flashOverridesBaseThenEnds) {
    StatusLight light;
    light.setBase(RED);
    light.flash(WHITE, 100, 1000);
    CHECK(light.output(1050, 100).g > 0);
    CHECK(light.output(1100, 100).g == 0);
}

TG_TEST(shortFlashDoesNotCutOffLongerFlash) {
    StatusLight light;
    light.flash(RED, 400, 0);
    light.flash(WHITE, 60, 10);
    CHECK(light.output(20, 100).g == 0);  // still red
    light.flash(WHITE, 600, 30);           // longer flash replaces it
    CHECK(light.output(40, 100).g > 0);
}

}  // namespace

int main() { return tg::test::run(); }
