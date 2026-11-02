#include "framebuffer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace tiltblock {
namespace fb {

std::uint16_t Framebuffer::at(int x, int y) const {
    if (!valid() || x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
    return pixels_[y * stride_ + x];
}

void Framebuffer::fill(std::uint16_t color) {
    if (!valid()) return;
    for (int y = 0; y < height_; ++y) {
        std::fill_n(pixels_ + y * stride_, width_, color);
    }
    mark(0, 0, width_ - 1, height_ - 1);
}

void Framebuffer::clear(const Rect& area, std::uint16_t color) {
    if (!valid()) return;
    const int x0 = std::max(area.x0, 0);
    const int y0 = std::max(area.y0, 0);
    const int x1 = std::min(area.x1, width_ - 1);
    const int y1 = std::min(area.y1, height_ - 1);
    if (x0 > x1) return;
    for (int y = y0; y <= y1; ++y) std::fill_n(pixels_ + y * stride_ + x0, x1 - x0 + 1, color);
}

void Framebuffer::pixel(int x, int y, std::uint16_t color) {
    x += offset_x_;
    y += offset_y_;
    if (!valid() || x < 0 || y < 0 || x >= width_ || y >= height_) return;
    pixels_[y * stride_ + x] = color;
    mark(x, y, x, y);
}

void Framebuffer::span(int x0, int x1, int y, std::uint16_t color) {
    y += offset_y_;
    if (!valid() || y < 0 || y >= height_) return;
    x0 = std::max(x0 + offset_x_, 0);
    x1 = std::min(x1 + offset_x_, width_ - 1);
    if (x0 > x1) return;
    std::fill_n(pixels_ + y * stride_ + x0, x1 - x0 + 1, color);
    mark(x0, y, x1, y);
}

void Framebuffer::rect(int x, int y, int w, int h, std::uint16_t color) {
    if (w <= 0 || h <= 0) return;
    const int top = std::max(y + offset_y_, 0) - offset_y_;
    const int bottom = std::min(y + h + offset_y_, height_) - offset_y_;
    for (int row = top; row < bottom; ++row) span(x, x + w - 1, row, color);
}

void Framebuffer::rectOutline(int x, int y, int w, int h, std::uint16_t color) {
    if (w <= 0 || h <= 0) return;
    span(x, x + w - 1, y, color);
    span(x, x + w - 1, y + h - 1, color);
    rect(x, y, 1, h, color);
    rect(x + w - 1, y, 1, h, color);
}

void Framebuffer::line(Point a, Point b, std::uint16_t color, int width) {
    int x0 = static_cast<int>(std::lround(a.x));
    int y0 = static_cast<int>(std::lround(a.y));
    const int x1 = static_cast<int>(std::lround(b.x));
    const int y1 = static_cast<int>(std::lround(b.y));
    // Skip lines entirely outside the buffer (with pen margin).
    const int margin = width;
    if ((std::max(x0, x1) + offset_x_ < -margin) || (std::min(x0, x1) + offset_x_ >= width_ + margin) ||
        (std::max(y0, y1) + offset_y_ < -margin) || (std::min(y0, y1) + offset_y_ >= height_ + margin)) {
        return;
    }
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        if (width == 1) pixel(x0, y0, color);
        else rect(x0 - width / 2, y0 - width / 2, width, width, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice = 2 * error;
        if (twice >= dy) error += dy, x0 += sx;
        if (twice <= dx) error += dx, y0 += sy;
    }
}

void Framebuffer::polygon(const Point* points, int count, std::uint16_t color) {
    if (!valid() || count < 3 || count > 8) return;
    float min_y = points[0].y;
    float max_y = points[0].y;
    float min_x = points[0].x;
    float max_x = points[0].x;
    for (int i = 1; i < count; ++i) {
        min_y = std::min(min_y, points[i].y);
        max_y = std::max(max_y, points[i].y);
        min_x = std::min(min_x, points[i].x);
        max_x = std::max(max_x, points[i].x);
    }
    if (max_x + offset_x_ < 0.0f || min_x + offset_x_ >= width_) return;
    const int top = std::max(static_cast<int>(std::floor(min_y)), -offset_y_);
    const int bottom = std::min(static_cast<int>(std::ceil(max_y)), height_ - 1 - offset_y_);
    for (int y = top; y <= bottom; ++y) {
        const float fy = static_cast<float>(y);
        float intersections[8];
        int n = 0;
        for (int i = 0; i < count; ++i) {
            const Point a = points[i];
            const Point b = points[(i + 1) % count];
            if ((a.y <= fy && b.y > fy) || (b.y <= fy && a.y > fy)) {
                intersections[n++] = a.x + (fy - a.y) * (b.x - a.x) / (b.y - a.y);
            }
        }
        if (n < 2) continue;
        std::sort(intersections, intersections + n);
        for (int i = 0; i + 1 < n; i += 2) {
            span(static_cast<int>(std::ceil(intersections[i])),
                 static_cast<int>(std::floor(intersections[i + 1])), y, color);
        }
    }
}

void Framebuffer::blit(const std::uint16_t* src, int w, int h, int src_stride, int x, int y,
                       bool use_key, std::uint16_t transparent) {
    if (!valid() || src == nullptr) return;
    x += offset_x_;
    y += offset_y_;
    const int x0 = std::max(x, 0);
    const int y0 = std::max(y, 0);
    const int x1 = std::min(x + w, width_);
    const int y1 = std::min(y + h, height_);
    if (x0 >= x1 || y0 >= y1) return;
    mark(x0, y0, x1 - 1, y1 - 1);
    for (int row = y0; row < y1; ++row) {
        const std::uint16_t* from = src + (row - y) * src_stride + (x0 - x);
        std::uint16_t* to = pixels_ + row * stride_ + x0;
        if (!use_key) {
            std::copy(from, from + (x1 - x0), to);
            continue;
        }
        for (int col = x0; col < x1; ++col, ++from, ++to) {
            if (*from != transparent) *to = *from;
        }
    }
}

}  // namespace fb
}  // namespace tiltblock
