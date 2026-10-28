#pragma once

#include "input_logic.h"

namespace input {

struct Encoder {
    std::int32_t delta = 0;
    std::int32_t total = 0;
    std::uint32_t invalid = 0;
    std::uint32_t overruns = 0;
    std::uint8_t ab = 0;
    int last_direction = 0;
    Button button;
};

struct State {
    Encoder left;
    Encoder right;
};

bool initialize(std::uint32_t now_ms);
void update(std::uint32_t now_ms);
const State& state();
const char* eventName(ButtonEvent event);

}  // namespace input
