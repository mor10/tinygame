#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "power_state.h"

int main() {
    power_state::State s;
    s.initialize(0);
    s.update(1000, false, true);
    assert(!s.acceptingInput());
    s.update(1010, false, false);
    s.update(1034, false, false);
    assert(!s.acceptingInput());
    s.update(1035, false, false);
    assert(s.acceptingInput());
    s.update(301034, false, false);
    assert(!s.pending());
    s.update(301035, false, false);
    assert(s.pending() && s.canPrepare());
    s.begin(301035);
    s.update(301154, false, false);
    assert(!s.ready());
    s.update(301155, false, false);
    assert(s.ready());

    s.initialize(0);
    s.update(0, false, false);
    s.update(25, false, false);
    s.update(170000, true, false);
    s.update(469999, false, false);
    assert(!s.pending());
    s.update(470000, false, false);
    assert(s.pending());
    s.cancel(470000);
    assert(!s.pending());
    s.request();
    s.update(470001, false, true);
    assert(!s.canPrepare());
    s.update(470010, false, false);
    s.update(470035, false, false);
    assert(s.canPrepare());
    s.begin(470035);
    s.update(470200, false, true);
    assert(!s.ready());
    s.update(470210, false, false);
    s.update(470235, false, false);
    assert(s.ready());

    s.initialize(UINT32_MAX - 100);
    s.update(UINT32_MAX - 100, false, false);
    s.update(UINT32_MAX - 75, false, false);
    s.update(299923, false, false);
    assert(!s.pending());
    s.update(299924, false, false);
    assert(s.pending());

    settings::Settings options;
    assert(options.autoOff() && options.autoOffTimeoutMs() == 300000);
    const std::uint32_t timeouts[] = {60000, 300000, 600000, 900000, 1800000};
    assert(settings::valueCount(settings::AUTO_OFF_TIME) == 5);
    for (std::uint8_t index = 0; index < 5; ++index) {
        settings::setValueIndex(options, settings::AUTO_OFF_TIME, index);
        assert(options.autoOffTimeoutMs() == timeouts[index]);
        assert(settings::valueIndex(options, settings::AUTO_OFF_TIME) == index);
        s.initialize(UINT32_MAX - 100);
        s.update(UINT32_MAX - 75, false, false, options);
        const std::uint32_t threshold = (UINT32_MAX - 75) + timeouts[index];
        s.update(threshold - 1, false, false, options);
        assert(!s.pending());
        s.update(threshold, false, false, options);
        assert(s.pending());
    }

    options = {};
    settings::setValueIndex(options, settings::AUTO_OFF, 0);
    s.initialize(0);
    s.update(25, false, false, options);
    s.update(2000000, false, false, options);
    assert(!s.pending());
    settings::setValueIndex(options, settings::AUTO_OFF, 1);
    s.update(2299999, false, false, options);
    assert(!s.pending());
    s.update(2300000, false, false, options);
    assert(s.pending());

    for (bool enabled : {false, true}) {
        options.value[settings::AUTO_OFF] = enabled;
        s.initialize(0);
        s.update(25, false, false, options, true);
        s.update(2000000, false, false, options, true);
        assert(!s.pending());
        s.request(); // Manual Sleep survives USB suppression and disabled auto-off.
        s.update(2000001, false, false, options, true);
        assert(s.pending() && s.canPrepare());
        s.begin(2000001);
        s.update(2000121, false, false, options, true);
        assert(s.ready());
    }
    options = {};
    s.initialize(0);
    s.update(25, false, false, options, true);
    s.update(2000000, false, false, options, true);
    s.update(2299999, false, false, options, false);
    assert(!s.pending());
    s.update(2300000, false, false, options, false);
    assert(s.pending());
    s.update(2300001, false, false, options, true);
    assert(s.pending()); // A pending request is never canceled by a connection.

    options.value[settings::AUTO_OFF] = 255;
    options.value[settings::AUTO_OFF_TIME] = 255;
    assert(settings::sanitize(options) == settings::Settings{});
    std::puts("power state tests passed");
}
