#pragma once

// Precomputed colour fades for the cube stack. Lower layers fade toward the
// background so the block looks like it rises out of the dark. Pure C++.

#include <cstdint>

#include "framebuffer.h"

namespace tiltblock {

inline constexpr int FADE_BANDS = 8;
// Band 0 is the cube layer directly under the play surface.
inline constexpr unsigned FADE_STEP = 42;

constexpr unsigned fadeAmount(int band) {
    return band <= 0 ? 0U
        : (static_cast<unsigned>(band) * FADE_STEP > 255U ? 255U
                                                          : static_cast<unsigned>(band) * FADE_STEP);
}

struct FadeTable {
    std::uint16_t band[FADE_BANDS];
};

constexpr FadeTable makeFade(std::uint32_t color, std::uint32_t background) {
    FadeTable table{};
    for (int i = 0; i < FADE_BANDS; ++i) table.band[i] = fb::mix565(color, background, fadeAmount(i));
    return table;
}

}  // namespace tiltblock
