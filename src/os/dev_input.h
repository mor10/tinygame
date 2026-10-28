#pragma once

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "devtools.h"
#include "input_logic.h"

// Parses the dev commands that fake controls: turn, click, hold, down, up and
// tilt. Pure logic, so the host tests can run it.
namespace devtools {

enum class CommandResult { NotInput, Ok, Error };

// Reads one number from -1 to 1 and moves `p` past it.
inline bool parseUnit(const char*& p, float& value) {
    char* end = nullptr;
    value = std::strtof(p, &end);
    if (end == p || !std::isfinite(value) || value < -1.0f || value > 1.0f) return false;
    p = end;
    return true;
}

// tilt <x> <y> (each -1..1) or tilt off.
inline CommandResult tiltCommand(const char* arg, Overrides& state) {
    if (arg == nullptr) return CommandResult::Error;
    if (std::strcmp(arg, "off") == 0) {
        state.tilt_active = false;
        return CommandResult::Ok;
    }
    float x = 0.0f;
    float y = 0.0f;
    const char* p = arg;
    if (!parseUnit(p, x) || !std::isspace(static_cast<unsigned char>(*p))) {
        return CommandResult::Error;
    }
    while (std::isspace(static_cast<unsigned char>(*p))) ++p;
    if (!parseUnit(p, y) || *p != '\0') return CommandResult::Error;
    state.tilt_active = true;
    state.tilt_x = x;
    state.tilt_y = y;
    return CommandResult::Ok;
}

// turn <left|right> <n>, click|hold|down|up [left|right], tilt ...
// Nothing changes unless the whole command is valid.
inline CommandResult inputCommand(const char* command, const char* arg, Overrides& state) {
    if (std::strcmp(command, "tilt") == 0) return tiltCommand(arg, state);

    const bool turn = std::strcmp(command, "turn") == 0;
    std::uint8_t event = 0;
    if (std::strcmp(command, "click") == 0) event = input::EVENT_SHORT_PRESS;
    else if (std::strcmp(command, "hold") == 0) event = input::EVENT_LONG_PRESS;
    else if (std::strcmp(command, "down") == 0) event = input::EVENT_PRESSED;
    else if (std::strcmp(command, "up") == 0) event = input::EVENT_RELEASED;
    else if (!turn) return CommandResult::NotInput;

    EncoderOverride* target = nullptr;
    if (turn) {
        if (arg == nullptr) return CommandResult::Error;
        if (std::strncmp(arg, "left ", 5) == 0) {
            target = &state.left;
            arg += 5;
        } else if (std::strncmp(arg, "right ", 6) == 0) {
            target = &state.right;
            arg += 6;
        } else return CommandResult::Error;
        errno = 0;
        char* end = nullptr;
        const long delta = std::strtol(arg, &end, 10);
        if (errno != 0 || end == arg || *end != '\0' ||
            delta < -1000000 || delta > 1000000) return CommandResult::Error;
        const auto total = static_cast<std::int64_t>(target->delta) + delta;
        if (total < INT32_MIN || total > INT32_MAX) return CommandResult::Error;
        target->delta = static_cast<std::int32_t>(total);
    } else {
        if (arg == nullptr || std::strcmp(arg, "left") == 0) target = &state.left;
        else if (std::strcmp(arg, "right") == 0) target = &state.right;
        else return CommandResult::Error;
        target->events |= event;
        if (event == input::EVENT_PRESSED) target->down = true;
        if (event == input::EVENT_RELEASED) target->down = false;
    }
    return CommandResult::Ok;
}

}  // namespace devtools
