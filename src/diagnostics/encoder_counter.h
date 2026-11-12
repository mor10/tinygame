#pragma once

#include <cstdint>

namespace diagnostics::counter {

struct Reading {
    std::int32_t left;
    std::int32_t right;
    bool left_overflow;
    bool right_overflow;
};

bool initialize();
bool read(Reading& reading);

}  // namespace diagnostics::counter
