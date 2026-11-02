#pragma once

// Small software renderer for one RGB565 pixel buffer (for example an LVGL
// canvas). Pure C++ with no LVGL or Arduino includes, so it is host-tested.
// Every primitive clips to the buffer before touching memory and writes whole
// spans, so drawing a full 320x172 scene costs a few milliseconds instead of
// one LVGL call per pixel. Colours are converted to RGB565 once, by the
// caller, outside the inner loops.

#include <cstdint>

namespace tiltblock {
namespace fb {

// 0xRRGGBB -> native RGB565, matching LVGL's lv_color_to_u16().
constexpr std::uint16_t rgb565(std::uint32_t rgb) {
    return static_cast<std::uint16_t>(((rgb >> 8) & 0xF800U) | ((rgb >> 5) & 0x07E0U) |
                                      ((rgb >> 3) & 0x001FU));
}

// Mix two 0xRRGGBB colours (amount 0 = from, 255 = to) and pack as RGB565.
// Use it to precompute fade tables, not per pixel.
constexpr std::uint16_t mix565(std::uint32_t from, std::uint32_t to, unsigned amount) {
    const unsigned keep = 255U - amount;
    const std::uint32_t r = (((from >> 16) & 0xFFU) * keep + ((to >> 16) & 0xFFU) * amount + 127U) / 255U;
    const std::uint32_t g = (((from >> 8) & 0xFFU) * keep + ((to >> 8) & 0xFFU) * amount + 127U) / 255U;
    const std::uint32_t b = ((from & 0xFFU) * keep + (to & 0xFFU) * amount + 127U) / 255U;
    return rgb565((r << 16) | (g << 8) | b);
}

struct Point {
    float x;
    float y;
};

// Inclusive pixel rectangle in buffer coordinates.
struct Rect {
    int x0 = 0;
    int y0 = 0;
    int x1 = -1;
    int y1 = -1;
    bool empty() const { return x0 > x1 || y0 > y1; }
};

inline Rect unite(const Rect& a, const Rect& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return {a.x0 < b.x0 ? a.x0 : b.x0, a.y0 < b.y0 ? a.y0 : b.y0,
            a.x1 > b.x1 ? a.x1 : b.x1, a.y1 > b.y1 ? a.y1 : b.y1};
}

class Framebuffer {
public:
    Framebuffer() = default;
    // stride_pixels is the row pitch in pixels (LVGL reports it in bytes).
    Framebuffer(std::uint16_t* pixels, int width, int height, int stride_pixels)
        : pixels_(pixels), width_(width), height_(height), stride_(stride_pixels) {}

    bool valid() const { return pixels_ != nullptr && width_ > 0 && height_ > 0; }
    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return stride_; }

    // Shifts all later drawing, e.g. for screen shake. Clipping still uses the
    // real buffer edges.
    void setOffset(int x, int y) {
        offset_x_ = x;
        offset_y_ = y;
    }

    std::uint16_t at(int x, int y) const;

    // Bounds of every pixel written since resetDirty(), after clipping and
    // offset. Lets the caller redraw and present only what changed.
    Rect dirty() const { return dirty_; }
    void resetDirty() { dirty_ = Rect{}; }
    // Fills a rectangle (buffer coordinates, no offset) without marking it
    // dirty; used to erase the previous frame.
    void clear(const Rect& area, std::uint16_t color);

    void fill(std::uint16_t color);
    void pixel(int x, int y, std::uint16_t color);
    // Inclusive span x0..x1 on row y.
    void span(int x0, int x1, int y, std::uint16_t color);
    void rect(int x, int y, int w, int h, std::uint16_t color);
    void rectOutline(int x, int y, int w, int h, std::uint16_t color);
    void line(Point a, Point b, std::uint16_t color, int width = 1);
    // Simple polygon (up to 8 vertices), even-odd filled. Rows outside the
    // buffer are skipped before any edge maths runs.
    void polygon(const Point* points, int count, std::uint16_t color);
    // Copies a w x h RGB565 image. Pixels equal to `transparent` are skipped
    // when use_key is true (for sprites).
    void blit(const std::uint16_t* src, int w, int h, int src_stride, int x, int y,
              bool use_key = false, std::uint16_t transparent = 0);

private:
    std::uint16_t* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
    int offset_x_ = 0;
    int offset_y_ = 0;
    Rect dirty_;

    void mark(int x0, int y0, int x1, int y1) {
        dirty_ = unite(dirty_, Rect{x0, y0, x1, y1});
    }
};

}  // namespace fb
}  // namespace tiltblock
