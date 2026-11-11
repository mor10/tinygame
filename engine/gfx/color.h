#pragma once

// Colour helpers. The engine draws RGB565 everywhere; palettes are written as
// 0xRRGGBB and packed once.

#include <cstdint>

namespace tg::gfx {

using Color = std::uint16_t;  // RGB565

constexpr Color rgb(std::uint32_t hex) {
    return static_cast<Color>(((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex >> 3) & 0x001F));
}

// Blends `a` toward `b`. amount: 0 = a, 256 = b.
constexpr Color mix(Color a, Color b, int amount) {
    const int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
    const int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    const int r = ar + (br - ar) * amount / 256;
    const int g = ag + (bg - ag) * amount / 256;
    const int bl = ab + (bb - ab) * amount / 256;
    return static_cast<Color>((r << 11) | (g << 5) | bl);
}

constexpr Color lighten(Color c, int amount) { return mix(c, 0xFFFF, amount); }
constexpr Color darken(Color c, int amount) { return mix(c, 0x0000, amount); }

}  // namespace tg::gfx
