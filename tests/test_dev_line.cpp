#include <cassert>
#include <cstdio>
#include <cstring>

#include "dev_line.h"

namespace {

using devtools::LineReader;
using Event = LineReader::Event;

// Feeds a whole string and returns the last event produced.
Event feed(LineReader& reader, const char* text) {
    Event last = Event::None;
    for (const char* p = text; *p; ++p) {
        const Event event = reader.feed(*p);
        if (event != Event::None) last = event;
    }
    return last;
}

}  // namespace

int main() {
    LineReader reader;
    assert(feed(reader, "click") == Event::None);
    assert(reader.feed('\n') == Event::Line);
    assert(std::strcmp(reader.line(), "click") == 0);

    // CRLF ends the command once; blank lines are ignored.
    assert(feed(reader, "turn left 4\r") == Event::Line);
    assert(std::strcmp(reader.line(), "turn left 4") == 0);
    assert(reader.feed('\n') == Event::None);
    assert(feed(reader, "\n\n") == Event::None);

    // A 63-character line fits; a longer one is dropped, not cut short.
    char longest[64];
    std::memset(longest, 'a', 63);
    longest[63] = '\0';
    assert(feed(reader, longest) == Event::None);
    assert(reader.feed('\n') == Event::Line);
    assert(std::strcmp(reader.line(), longest) == 0);

    assert(feed(reader, "click ") == Event::None);
    assert(feed(reader, longest) == Event::None);
    assert(reader.feed('\n') == Event::TooLong);

    // The next command after a dropped line starts clean.
    assert(feed(reader, "info\n") == Event::Line);
    assert(std::strcmp(reader.line(), "info") == 0);
    std::puts("dev line tests passed");
}
