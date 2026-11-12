#include "fixed_step_clock.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    using tg::engine::FixedStepClock;
    using tg::engine::TickBatch;

    FixedStepClock clock;
    TickBatch batch;
    assert(clock.advance(16'666, batch));
    assert(batch.steps_to_run == 0);
    assert(clock.advance(16'667, batch));
    assert(batch.steps_to_run == 1);
    assert(!batch.overloaded);
    assert(clock.advance(16'667, batch));
    assert(batch.steps_to_run == 2);

    clock.reset();
    assert(clock.advance(1'000'000, batch));
    assert(batch.steps_to_run == FixedStepClock::CANDIDATE_MAX_STEPS_PER_ADVANCE);
    assert(batch.pending_steps == 55);
    assert(batch.overloaded);

    std::uint64_t ran = batch.steps_to_run;
    while (batch.pending_steps != 0) {
        assert(clock.advance(0, batch));
        ran += batch.steps_to_run;
    }
    assert(ran == 60);
    assert(!batch.overloaded);

    clock.reset();
    assert(!clock.advance(std::numeric_limits<std::uint64_t>::max(), batch));
    assert(clock.advance(1'000'000, batch));
    assert(batch.steps_to_run == FixedStepClock::CANDIDATE_MAX_STEPS_PER_ADVANCE);
    assert(batch.pending_steps == 55);

    clock.reset();
    ran = 0;
    for (int i = 0; i < 60; ++i) {
        assert(clock.advance(16'667, batch));
        assert(batch.steps_to_run <= FixedStepClock::CANDIDATE_MAX_STEPS_PER_ADVANCE);
        ran += batch.steps_to_run;
    }
    while (batch.pending_steps != 0) {
        assert(clock.advance(0, batch));
        ran += batch.steps_to_run;
    }
    assert(ran == 60);
}
