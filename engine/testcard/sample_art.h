#pragma once

// Sample art shared by the test card and the scrolling demo. It is written in
// the text form a game would use: '.' is transparent, a hex digit is a palette
// index (see gfx/palette.h for the roles).

#include "../gfx/art.h"

namespace tg::sample {

inline constexpr gfx::Art GROUND{8, 8,
    "77777777"
    "77877787"
    "dd7dd7dd"
    "dddddddd"
    "ddeddddd"
    "dddddded"
    "dedddddd"
    "dddddddd"};

inline constexpr gfx::Art BRICK{8, 8,
    "55535555"
    "55535555"
    "33333333"
    "55555355"
    "55555355"
    "33333333"
    "55535555"
    "55535555"};

inline constexpr gfx::Art COIN{8, 8,
    "..6666.."
    ".626665."
    "62666665"
    "62666665"
    "66666665"
    "66666655"
    ".666655."
    "..5555.."};

inline constexpr gfx::Art SLIME{8, 8,
    "........"
    "........"
    "..7777.."
    ".777777."
    "72177217"
    "77777777"
    "77777777"
    ".8.88.8."};

inline constexpr gfx::Art HERO{16, 16,
    "................"
    "....44444444...."
    "...4444444444..."
    "..444444444444.."
    "..444444444444.."
    "..442214422144.."
    "..442214422144.."
    "..444444444444.."
    "..444444444444.."
    "..444411114444.."
    "..444444444444.."
    "..444444444444.."
    "...4444444444..."
    "...44......44..."
    "..fff......fff.."
    "..fff......fff.."};

inline constexpr gfx::Art CLOUD{8, 8,
    "........"
    "..2222.."
    ".222222."
    "22222222"
    "22222222"
    ".333333."
    "........"
    "........"};

}  // namespace tg::sample
