#pragma once

// The frames that are fingerprinted as golden images: both boards, both views,
// at the start, mid-scroll and after the character has turned round.

#include "../world/stage.h"

namespace tg::demo {

struct ScrollCase {
    const char* name;
    int width;
    int height;
    View view;
    int tick;
};

inline constexpr ScrollCase SCROLL_CASES[] = {
    {"a_standard_t0", 320, 172, View::Standard, 0},
    {"a_standard_t200", 320, 172, View::Standard, 200},
    {"a_standard_t600", 320, 172, View::Standard, 600},
    {"b_standard_t0", 280, 240, View::Standard, 0},
    {"b_standard_t200", 280, 240, View::Standard, 200},
    {"b_fill_t200", 280, 240, View::Fill, 200},
    {"b_fill_t600", 280, 240, View::Fill, 600},
};

}  // namespace tg::demo
