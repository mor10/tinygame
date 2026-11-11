#include "canvas.h"

namespace tg::gfx {

bool Canvas::clip(int& x, int& y, int& w, int& h) const {
    if (s_.pixels == nullptr) return false;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > s_.width) w = s_.width - x;
    if (y + h > s_.height) h = s_.height - y;
    return w > 0 && h > 0;
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
