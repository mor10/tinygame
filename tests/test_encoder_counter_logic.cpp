#include <cassert>
#include <initializer_list>

#include "../src/diagnostics/encoder_counter_logic.h"
#include "../src/hal/input_logic.h"

int counterStep(int previous, int next) {
    int result = 0;
    for (const auto& channel : diagnostics::COUNTER_CHANNELS) {
        const int pulse = channel.pulse_is_a ? 2 : 1;
        const int control = channel.pulse_is_a ? 1 : 2;
        if ((previous & pulse) == (next & pulse)) continue;
        const int edge = (next & pulse) ? 1 : -1;
        const int level = (next & control) ? -1 : 1;
        result += edge * level * channel.rising_when_control_low;
    }
    return result;
}

int main() {
    constexpr int cycle[] = {3, 2, 0, 1};
    for (int direction : {-1, 1}) {
        for (int phase = 0; phase < 4; ++phase) {
            input::Quadrature decoder;
            int previous = cycle[phase];
            decoder.initialize(previous);
            int total = 0;
            for (int step = 1; step <= 4; ++step) {
                const int next = cycle[(phase + direction * step + 8) % 4];
                const int hardware = counterStep(previous, next);
                assert(hardware == decoder.sample(next));
                total += hardware;
                previous = next;
            }
            assert(total == 4 * direction);
        }
    }
    // Both possible intermediate states of a two-bit jump have opposite signs.
    input::Quadrature decoder;
    decoder.initialize(3);
    assert(decoder.sample(0) == 0 && decoder.invalid == 1);
    assert(counterStep(3, 2) + counterStep(2, 0) == 2);
    assert(counterStep(3, 1) + counterStep(1, 0) == -2);
}
