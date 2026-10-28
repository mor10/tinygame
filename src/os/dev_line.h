#pragma once

#include <cstddef>

namespace devtools {

// Collects serial bytes into command lines. A line that does not fit is
// dropped whole, so a cut-off command is never run by mistake.
class LineReader {
public:
    enum class Event { None, Line, TooLong };

    // Feed one byte. `Line` means line() now holds a complete command.
    Event feed(char c) {
        if (c != '\n' && c != '\r') {
            if (length_ < CAPACITY) {
                text_[length_++] = c;
            } else {
                too_long_ = true;
            }
            return Event::None;
        }
        const Event event = too_long_ ? Event::TooLong : (length_ > 0 ? Event::Line : Event::None);
        text_[length_] = '\0';
        length_ = 0;
        too_long_ = false;
        return event;
    }

    char* line() { return text_; }

private:
    static constexpr std::size_t CAPACITY = 63;
    char text_[CAPACITY + 1] = {};
    std::size_t length_ = 0;
    bool too_long_ = false;
};

}  // namespace devtools
