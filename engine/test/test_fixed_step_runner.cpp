#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

#include "fixed_step_runner.h"

using tg::engine::FixedStepRunner;
using tg::engine::StepError;

int main() {
    FixedStepRunner runner;
    std::vector<tg::Input> ticks;
    auto update = [&](const tg::Input& input) {
        ticks.push_back(input);
        return true;
    };

    tg::Input input;
    input.main.delta = 4;
    input.main.pressed = true;
    input.main.down = true;
    input.pressed = true;
    input.down = true;
    assert(runner.advance(0, input, update).steps == 0);
    input.main.delta = 0;
    input.main.pressed = input.pressed = false;
    auto result = runner.advance(16'667, input, update);
    assert(result.error == StepError::None && result.steps == 1);
    assert(ticks[0].main.delta == 4 && ticks[0].pressed && ticks[0].down);
    result = runner.advance(16'667, input, update);
    assert(result.steps == 1);
    assert(ticks[1].main.delta == 0 && !ticks[1].pressed && ticks[1].down);

    runner.reset();
    ticks.clear();
    input = {};
    input.left.delta = 2;
    input.left.pressed = input.pressed = true;
    input.left.down = input.down = true;
    assert(runner.advance(0, input, update).steps == 0);
    input = {};
    input.left.delta = -1;
    input.left.released = input.released = true;
    assert(runner.advance(0, input, update).steps == 0);
    assert(runner.advance(16'667, {}, update).steps == 1);
    assert(ticks[0].left.delta == 1 && ticks[0].left.pressed && ticks[0].left.released);
    assert(ticks[0].pressed && ticks[0].released && !ticks[0].down);

    runner.reset();
    ticks.clear();
    tg::Input earlier;
    earlier.main.delta = 3;
    assert(runner.advance(0, earlier, update).steps == 0);
    tg::Input later;
    later.main.delta = 7;
    result = runner.advance(200'000, later, update);
    assert(result.steps == 5 && result.pending_steps == 7);
    assert(ticks[0].main.delta == 3);
    for (std::size_t i = 1; i < ticks.size(); ++i) assert(ticks[i].main.delta == 0);
    while (result.pending_steps != 0) result = runner.advance(0, tg::Input{}, update);
    assert(ticks.size() == 12);
    result = runner.advance(16'667, tg::Input{}, update);
    assert(result.steps == 1 && ticks.back().main.delta == 7);
    assert(runner.advance(16'667, tg::Input{}, update).steps == 1);
    assert(ticks.back().main.delta == 0);

    runner.reset();
    ticks.clear();
    assert(runner.advance(0, earlier, update).steps == 0);
    runner.discardInput();
    assert(runner.advance(16'667, tg::Input{}, update).steps == 1);
    assert(ticks[0].main.delta == 0);

    runner.reset();
    input = {};
    input.click = true;
    assert(runner.advance(0, input, update).error == StepError::None);
    assert(runner.advance(0, input, update).error == StepError::DuplicateEdge);
    assert(runner.advance(16'667, {}, update).error == StepError::DuplicateEdge);
    runner.reset();
    input = {};
    input.main.delta = std::numeric_limits<std::int32_t>::max();
    assert(runner.advance(0, input, update).error == StepError::None);
    input.main.delta = 1;
    assert(runner.advance(0, input, update).error == StepError::DeltaOverflow);
    runner.reset();
    assert(runner.advance(3'000'000, {}, update).error == StepError::BacklogLimit);
    runner.reset();
    auto reject = [](const tg::Input&) { return false; };
    assert(runner.advance(16'667, {}, reject).error == StepError::CallbackFailed);
    assert(runner.advance(16'667, {}, update).error == StepError::CallbackFailed);
}
