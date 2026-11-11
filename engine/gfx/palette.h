#pragma once

// A game palette: 16 entries, index 0 is transparent. The entries have fixed
// roles so the same art reads correctly under any palette.
//
//   1 ink      5 orange   9 sky     d brown
//   2 white    6 yellow   a blue    e sand
//   3 grey     7 green    b purple  f shade
//   4 red      8 forest   c pink

#include "color.h"

namespace tg::gfx {

struct Palette {
    const char* name;
    Color color[16];
    Color background;  // default sky / backdrop
    Color ink;         // text drawn on the background
};

}  // namespace tg::gfx
