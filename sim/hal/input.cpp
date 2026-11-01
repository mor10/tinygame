#include "input.h"

#include "sim_hal.h"

namespace {
input::State current;
std::int32_t pending[2]{};
bool pressed[2]{};
}  // namespace

namespace input {
bool initialize(std::uint32_t now_ms) {
    current = State{};
    pending[0] = pending[1] = 0;
    current.left.button.initialize(pressed[0], now_ms);
    current.right.button.initialize(pressed[1], now_ms);
    return true;
}

void update(std::uint32_t now_ms) {
    for (int side = 0; side < 2; ++side) {
        auto& encoder = side == 0 ? current.left : current.right;
        encoder.delta = pending[side];
        pending[side] = 0;
        if (encoder.delta != 0) encoder.last_direction = encoder.delta > 0 ? 1 : -1;
        encoder.button.update(pressed[side], now_ms);
    }
}
const State& state() { return current; }
bool rawButtonDown(bool left) { return pressed[left ? 0 : 1]; }
const char* eventName(ButtonEvent event) {
    switch (event) {
        case ButtonEvent::Pressed: return "PRESS";
        case ButtonEvent::Released: return "RELEASE";
        case ButtonEvent::ShortPress: return "CLICK";
        case ButtonEvent::LongPress: return "HOLD";
        default: return "NONE";
    }
}
}  // namespace input

namespace sim {
void turn(int side, std::int32_t delta) {
    if (side < 0 || side > 1 || sleeping()) return;
    auto& encoder = side == 0 ? current.left : current.right;
    const auto next = input::boundedAdd(pending[side], delta);
    const auto total = input::boundedAdd(encoder.total, delta);
    if (static_cast<std::int64_t>(pending[side]) + delta != next ||
        static_cast<std::int64_t>(encoder.total) + delta != total) {
        input::increment(encoder.overruns);
    }
    pending[side] = next;
    encoder.total = total;
}
void button(int side, bool down) {
    if (side >= 0 && side < 2) pressed[side] = down;
}
bool buttonsPressed() { return pressed[0] || pressed[1]; }
}  // namespace sim
