#include "canvas.h"

namespace tg::gfx {

bool Canvas::clip(int& x, int& y, int& w, int& h) const {
    if (s_.pixels == nullptr) return false;
    if (x < clip_.x) { w -= clip_.x - x; x = clip_.x; }
    if (y < clip_.y) { h -= clip_.y - y; y = clip_.y; }
    if (x + w > clip_.x + clip_.w) w = clip_.x + clip_.w - x;
    if (y + h > clip_.y + clip_.h) h = clip_.y + clip_.h - y;
    return w > 0 && h > 0;
}

void Canvas::setClip(const Rect& area) {
    int x0 = area.x < 0 ? 0 : area.x;
    int y0 = area.y < 0 ? 0 : area.y;
    int x1 = area.x + area.w > s_.width ? s_.width : area.x + area.w;
    int y1 = area.y + area.h > s_.height ? s_.height : area.y + area.h;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    clip_ = {x0, y0, x1 - x0, y1 - y0};
}

void Canvas::blit(const Color* source, const std::uint8_t* mask, int source_w, int source_h,
                  int x, int y) {
    int cx = x, cy = y, cw = source_w, ch = source_h;
    if (source == nullptr || !clip(cx, cy, cw, ch)) return;
    for (int row = 0; row < ch; ++row) {
        const int source_index = (cy - y + row) * source_w + (cx - x);
        Color* out = s_.pixels + (cy + row) * s_.stride + cx;
        if (mask == nullptr) {
            for (int i = 0; i < cw; ++i) out[i] = source[source_index + i];
        } else {
            for (int i = 0; i < cw; ++i)
                if (mask[source_index + i] != 0) out[i] = source[source_index + i];
        }
    }
}

void Canvas::fill(Color color) { rect(0, 0, s_.width, s_.height, color); }

void Canvas::rect(int x, int y, int w, int h, Color color) {
    if (!clip(x, y, w, h)) return;
    for (int row = y; row < y + h; ++row) {
        Color* p = s_.pixels + row * s_.stride + x;
        for (int i = 0; i < w; ++i) p[i] = color;
    }
}

void Canvas::shade(int x, int y, int w, int h, int amount) {
    if (!clip(x, y, w, h)) return;
    for (int row = y; row < y + h; ++row) {
        Color* p = s_.pixels + row * s_.stride + x;
        for (int i = 0; i < w; ++i) p[i] = darken(p[i], amount);
    }
}

}  // namespace tg::gfx
