#pragma once

// A clipped drawing surface over RGB565 memory the caller owns. No allocation,
// no platform includes: the same code runs on the console, in WebAssembly and
// in host tests.

#include "color.h"

namespace tg::gfx {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Surface {
    Color* pixels = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;  // pixels per row, >= width
};

class Canvas {
public:
    explicit Canvas(Surface surface)
        : s_(surface), clip_{0, 0, surface.width, surface.height} {}

    // Limits all drawing to a rectangle (trimmed to the surface). Used to keep
    // the stage from drawing over the status area.
    void setClip(const Rect& area);
    void resetClip() { clip_ = {0, 0, s_.width, s_.height}; }
    Rect clipRect() const { return clip_; }

    int width() const { return s_.width; }
    int height() const { return s_.height; }

    void fill(Color color);
    void rect(int x, int y, int w, int h, Color color);
    // Darkens what is already drawn. amount: 0 = unchanged, 256 = black.
    void shade(int x, int y, int w, int h, int amount);
    // Copies a block of pixels. `mask` has one byte per source pixel (0 =
    // skip); pass nullptr when every pixel is solid.
    void blit(const Color* source, const std::uint8_t* mask, int source_w, int source_h, int x,
              int y);
    void pixel(int x, int y, Color color) {
        if (x >= clip_.x && y >= clip_.y && x < clip_.x + clip_.w && y < clip_.y + clip_.h)
            s_.pixels[y * s_.stride + x] = color;
    }
    // Reads a pixel; out-of-range reads return 0.
    Color at(int x, int y) const {
        return x >= 0 && y >= 0 && x < s_.width && y < s_.height ? s_.pixels[y * s_.stride + x] : 0;
    }

private:
    // Trims a rectangle to the clip area. False when nothing is left.
    bool clip(int& x, int& y, int& w, int& h) const;
    Surface s_;
    Rect clip_;
};

}  // namespace tg::gfx
