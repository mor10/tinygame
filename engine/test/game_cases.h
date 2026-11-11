#pragma once

// The engine frames that are fingerprinted as golden images: the built-in
// cave_hop test bundle on both boards, at the start and after two seconds of
// a scripted run (run right, jump now and then).

#include "../core/input.h"

namespace tg::test {

struct GameCase {
    const char* name;
    int width;
    int height;
    int tick;
};

inline constexpr GameCase GAME_CASES[] = {
    {"game_a_t0", 320, 172, 0},
    {"game_a_t120", 320, 172, 120},
    {"game_b_t0", 280, 240, 0},
    {"game_b_t120", 280, 240, 120},
};

// The input fed on call number `tick` of a golden run.
inline Input goldenInput(int tick) {
    Input in;
    in.tilt_x = 1.0f;
    if (tick == 30 || tick == 90) {
        in.pressed = true;
        in.down = true;
    }
    if (tick == 45 || tick == 105) in.released = true;
    return in;
}

}  // namespace tg::test
