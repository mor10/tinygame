#pragma once

namespace diagnostics {

// PCNT counts each signal's edges, reversing their sign when the other is high.
struct CounterChannel {
    bool pulse_is_a;
    int rising_when_control_low;
};

inline constexpr CounterChannel COUNTER_CHANNELS[] = {
    {true, -1},
    {false, 1},
};

}  // namespace diagnostics
