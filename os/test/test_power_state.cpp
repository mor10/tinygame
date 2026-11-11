// Carried over from v1.0 tests/test_power_state.cpp. Converted to the v2 harness (engine/test/tg_test.h):
// assert() became CHECK(), so one failure no longer stops the others.
#include "../../engine/test/tg_test.h"
#include <initializer_list>
#include "power_state.h"

TG_TEST(power_state_rules) {
    power_state::State s;
    s.initialize(0);
    s.update(1000, false, true);
    CHECK(!s.acceptingInput());
    s.update(1010, false, false);
    s.update(1034, false, false);
    CHECK(!s.acceptingInput());
    s.update(1035, false, false);
    CHECK(s.acceptingInput());
    s.update(301034, false, false);
    CHECK(!s.pending());
    s.update(301035, false, false);
    CHECK(s.pending() && s.canPrepare());
    s.begin(301035);
    s.update(301154, false, false);
    CHECK(!s.ready());
    s.update(301155, false, false);
    CHECK(s.ready());

    s.initialize(0);
    s.update(0, false, false);
    s.update(25, false, false);
    s.update(170000, true, false);
    s.update(469999, false, false);
    CHECK(!s.pending());
    s.update(470000, false, false);
    CHECK(s.pending());
    s.cancel(470000);
    CHECK(!s.pending());
    s.request();
    s.update(470001, false, true);
    CHECK(!s.canPrepare());
    s.update(470010, false, false);
    s.update(470035, false, false);
    CHECK(s.canPrepare());
    s.begin(470035);
    s.update(470200, false, true);
    CHECK(!s.ready());
    s.update(470210, false, false);
    s.update(470235, false, false);
    CHECK(s.ready());

    s.initialize(UINT32_MAX - 100);
    s.update(UINT32_MAX - 100, false, false);
    s.update(UINT32_MAX - 75, false, false);
    s.update(299923, false, false);
    CHECK(!s.pending());
    s.update(299924, false, false);
    CHECK(s.pending());

    settings::Settings options;
    CHECK(options.autoOff() && options.autoOffTimeoutMs() == 300000);
    const std::uint32_t timeouts[] = {60000, 300000, 600000, 900000, 1800000};
    CHECK(settings::valueCount(settings::AUTO_OFF_TIME) == 5);
    for (std::uint8_t index = 0; index < 5; ++index) {
        settings::setValueIndex(options, settings::AUTO_OFF_TIME, index);
        CHECK(options.autoOffTimeoutMs() == timeouts[index]);
        CHECK(settings::valueIndex(options, settings::AUTO_OFF_TIME) == index);
        s.initialize(UINT32_MAX - 100);
        s.update(UINT32_MAX - 75, false, false, options);
        const std::uint32_t threshold = (UINT32_MAX - 75) + timeouts[index];
        s.update(threshold - 1, false, false, options);
        CHECK(!s.pending());
        s.update(threshold, false, false, options);
        CHECK(s.pending());
    }

    options = {};
    settings::setValueIndex(options, settings::AUTO_OFF, 0);
    s.initialize(0);
    s.update(25, false, false, options);
    s.update(2000000, false, false, options);
    CHECK(!s.pending());
    settings::setValueIndex(options, settings::AUTO_OFF, 1);
    s.update(2299999, false, false, options);
    CHECK(!s.pending());
    s.update(2300000, false, false, options);
    CHECK(s.pending());

    for (bool enabled : {false, true}) {
        options.value[settings::AUTO_OFF] = enabled;
        s.initialize(0);
        s.update(25, false, false, options, true);
        s.update(2000000, false, false, options, true);
        CHECK(!s.pending());
        s.request(); // Manual Sleep survives USB suppression and disabled auto-off.
        s.update(2000001, false, false, options, true);
        CHECK(s.pending() && s.canPrepare());
        s.begin(2000001);
        s.update(2000121, false, false, options, true);
        CHECK(s.ready());
    }
    options = {};
    s.initialize(0);
    s.update(25, false, false, options, true);
    s.update(2000000, false, false, options, true);
    s.update(2299999, false, false, options, false);
    CHECK(!s.pending());
    s.update(2300000, false, false, options, false);
    CHECK(s.pending());
    s.update(2300001, false, false, options, true);
    CHECK(s.pending()); // A pending request is never canceled by a connection.

    options.value[settings::AUTO_OFF] = 255;
    options.value[settings::AUTO_OFF_TIME] = 255;
    CHECK(settings::sanitize(options) == settings::Settings{});
}

int main() { return tg::test::run(); }
