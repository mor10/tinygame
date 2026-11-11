#pragma once

// A clipped drawing surface over RGB565 memory the caller owns. No allocation,
// no platform includes: the same code runs on the console, in WebAssembly and
// in host tests.

#include "color.h"

namespace tg::gfx {

struct Surface {
    Color* pixels = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;  // pixels per row, >= width
};

class Canvas {
public:
    explicit Canvas(Surface surface) : s_(surface) {}

    int width() const { return s_.width; }
    int height() const { return s_.height; }

    void fill(Color color);
    void rect(int x, int y, int w, int h, Color color);
    // Darkens what is already drawn. amount: 0 = unchanged, 256 = black.
    void shade(int x, int y, int w, int h, int amount);
    void pixel(int x, int y, Color color) {
        if (x >= 0 && y >= 0 && x < s_.width && y < s_.height) s_.pixels[y * s_.stride + x] = color;
    }
    // Reads a pixel; out-of-range reads return 0.
    Color at(int x, int y) const {
        return x >= 0 && y >= 0 && x < s_.width && y < s_.height ? s_.pixels[y * s_.stride + x] : 0;
    }

private:
    // Trims a rectangle to the surface. False when nothing is left.
    bool clip(int& x, int& y, int& w, int& h) const;
    Surface s_;
};

}  // namespace tg::gfx
