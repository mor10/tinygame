#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "fade.h"
#include "framebuffer.h"

using tiltblock::fb::Framebuffer;
using tiltblock::fb::Point;
using tiltblock::fb::mix565;
using tiltblock::fb::rgb565;

namespace {

constexpr std::uint16_t GUARD = 0xBEEF;
constexpr std::uint16_t INK = 0x1234;

// A 10x6 view inside a buffer with a 13-pixel stride and guard rows, so any
// write outside the view or into stride padding is detected.
struct Canvas {
    static constexpr int W = 10, H = 6, STRIDE = 13, PAD_ROWS = 2;
    std::vector<std::uint16_t> memory = std::vector<std::uint16_t>(STRIDE * (H + 2 * PAD_ROWS), GUARD);
    Framebuffer fb{memory.data() + STRIDE * PAD_ROWS, W, H, STRIDE};

    int count(std::uint16_t color) const {
        int n = 0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) n += fb.at(x, y) == color;
        return n;
    }
    void assertGuardsIntact() const {
        for (int i = 0; i < STRIDE * PAD_ROWS; ++i) {
            assert(memory[i] == GUARD);
            assert(memory[memory.size() - 1 - i] == GUARD);
        }
        for (int y = 0; y < H; ++y)
            for (int x = W; x < STRIDE; ++x) assert(memory[STRIDE * (PAD_ROWS + y) + x] == GUARD);
    }
};

void colourPacking() {
    assert(rgb565(0x000000) == 0x0000);
    assert(rgb565(0xFFFFFF) == 0xFFFF);
    assert(rgb565(0xFF0000) == 0xF800);
    assert(rgb565(0x00FF00) == 0x07E0);
    assert(rgb565(0x0000FF) == 0x001F);
    assert(rgb565(0x123456) == (((0x12 >> 3) << 11) | ((0x34 >> 2) << 5) | (0x56 >> 3)));
    assert(mix565(0xFFFFFF, 0x000000, 0) == 0xFFFF);
    assert(mix565(0xFFFFFF, 0x000000, 255) == 0x0000);
    assert(mix565(0xFF0000, 0x0000FF, 128) == rgb565(0x7F0080));
    constexpr auto ramp = tiltblock::makeRamp(0x86F8FF, 0x0B0E1A);
    static_assert(ramp.step[0] == rgb565(0x86F8FF), "Ramp starts at full glow");
    static_assert(ramp.step[tiltblock::EFFECT_STEPS - 1] == rgb565(0x0B0E1A),
                  "Ramp ends at the background");
    for (int i = 1; i < tiltblock::EFFECT_STEPS; ++i) {
        assert((ramp.step[i] >> 11) <= (ramp.step[i - 1] >> 11));
        assert(((ramp.step[i] >> 5) & 63) <= ((ramp.step[i - 1] >> 5) & 63));
        assert((ramp.step[i] & 31) <= (ramp.step[i - 1] & 31));
    }
}

void fillAndStride() {
    Canvas c;
    c.fb.fill(INK);
    assert(c.count(INK) == Canvas::W * Canvas::H);
    c.assertGuardsIntact();
}

void clippedRectAndSpan() {
    Canvas c;
    c.fb.fill(0);
    c.fb.rect(-5, -5, 8, 8, INK);     // covers x 0..2, y 0..2
    assert(c.count(INK) == 9);
    c.fb.rect(8, 4, 100, 100, INK);   // x 8..9, y 4..5
    assert(c.count(INK) == 13);
    c.fb.span(-100, 100, 3, INK);     // full row
    assert(c.fb.at(0, 3) == INK && c.fb.at(9, 3) == INK);
    c.fb.span(5, 2, 1, 0x7777);       // empty span draws nothing
    c.fb.span(0, 9, -1, 0x7777);
    c.fb.span(0, 9, 6, 0x7777);
    assert(c.count(0x7777) == 0);
    c.fb.rect(0, 0, 0, 5, 0x7777);
    assert(c.count(0x7777) == 0);
    c.assertGuardsIntact();
}

void offsetShiftsButStillClips() {
    Canvas c;
    c.fb.fill(0);
    c.fb.setOffset(2, 1);
    c.fb.pixel(0, 0, INK);
    assert(c.fb.at(2, 1) == INK);
    c.fb.rect(5, 2, 20, 20, INK);
    c.fb.pixel(-3, -2, INK);
    c.assertGuardsIntact();
    c.fb.setOffset(0, 0);
    assert(c.fb.at(9, 5) == INK);
}

void polygonCoverageAndClipping() {
    Canvas c;
    c.fb.fill(0);
    const Point square[] = {{2, 1}, {6, 1}, {6, 4}, {2, 4}};
    c.fb.polygon(square, 4, INK);
    // Rows 1..3 (half-open in y), columns 2..6.
    assert(c.count(INK) == 3 * 5);
    // A huge triangle far off screen and one covering everything.
    c.fb.fill(0);
    const Point off[] = {{-500, -500}, {-400, -500}, {-450, -300}};
    c.fb.polygon(off, 3, INK);
    assert(c.count(INK) == 0);
    const Point huge[] = {{-1000, -1000}, {1000, -1000}, {1000, 1000}, {-1000, 1000}};
    c.fb.polygon(huge, 4, INK);
    assert(c.count(INK) == Canvas::W * Canvas::H);
    c.assertGuardsIntact();
    // Degenerate input is ignored.
    c.fb.polygon(huge, 2, 0);
    assert(c.count(INK) == Canvas::W * Canvas::H);
}

void linesClipAndEndpoints() {
    Canvas c;
    c.fb.fill(0);
    c.fb.line({0, 0}, {9, 5}, INK);
    assert(c.fb.at(0, 0) == INK && c.fb.at(9, 5) == INK);
    c.fb.line({-50, 2}, {50, 2}, INK, 3);
    for (int x = 0; x < Canvas::W; ++x) assert(c.fb.at(x, 1) == INK && c.fb.at(x, 3) == INK);
    c.fb.line({-200, -200}, {-100, -150}, 0x7777);
    assert(c.count(0x7777) == 0);
    c.assertGuardsIntact();
}

void blitClipsAndKeys() {
    Canvas c;
    c.fb.fill(0);
    const std::uint16_t sprite[] = {1, 2, 3,
                                    4, 0, 6};
    c.fb.blit(sprite, 3, 2, 3, -1, 4, true, 0);  // left column and transparent pixel skipped
    assert(c.fb.at(0, 4) == 2 && c.fb.at(1, 4) == 3);
    assert(c.fb.at(0, 5) == 0 && c.fb.at(1, 5) == 6);
    c.fb.blit(sprite, 3, 2, 3, 8, 5);            // only top-left 2x1 visible
    assert(c.fb.at(8, 5) == 1 && c.fb.at(9, 5) == 2);
    c.assertGuardsIntact();
}

void fadeDarkensTowardBackground() {
    constexpr auto table = tiltblock::makeFade(0x8090A0, 0x000000);
    assert(table.band[0] == rgb565(0x8090A0));
    for (int i = 1; i < tiltblock::FADE_BANDS; ++i) {
        const int prev = (table.band[i - 1] >> 11) + ((table.band[i - 1] >> 5) & 63) + (table.band[i - 1] & 31);
        const int next = (table.band[i] >> 11) + ((table.band[i] >> 5) & 63) + (table.band[i] & 31);
        assert(next <= prev);
    }
    assert(table.band[tiltblock::FADE_BANDS - 1] != table.band[0]);
    assert(table.band[1] != table.band[0]);
    assert(tiltblock::fadeAmount(-1) == 0);
    assert(tiltblock::fadeAmount(100) == 255);
}

void dirtyBoundsTrackClippedWrites() {
    Canvas c;
    c.fb.fill(0);
    c.fb.resetDirty();
    assert(c.fb.dirty().empty());
    c.fb.span(-5, 2, 1, INK);
    c.fb.pixel(7, 4, INK);
    c.fb.pixel(50, 50, INK);  // clipped: must not grow the bounds
    const tiltblock::fb::Rect d = c.fb.dirty();
    assert(d.x0 == 0 && d.y0 == 1 && d.x1 == 7 && d.y1 == 4);
    c.fb.setOffset(1, 1);
    c.fb.pixel(8, 4, INK);
    assert(c.fb.dirty().x1 == 9 && c.fb.dirty().y1 == 5);

    // clear() erases the previous bounds without marking anything.
    c.fb.resetDirty();
    c.fb.clear(d, 0);
    assert(c.count(INK) == 1);  // only the offset pixel outside d is left
    c.fb.clear({-3, -3, 99, 99}, 0);
    assert(c.fb.dirty().empty());
    assert(c.count(INK) == 0);
    c.assertGuardsIntact();

    const tiltblock::fb::Rect u = tiltblock::fb::unite({1, 2, 3, 4}, {0, 5, 2, 6});
    assert(u.x0 == 0 && u.y0 == 2 && u.x1 == 3 && u.y1 == 6);
    assert(tiltblock::fb::unite({}, {1, 1, 1, 1}).x0 == 1);
}

}  // namespace

int main() {
    colourPacking();
    fillAndStride();
    clippedRectAndSpan();
    offsetShiftsButStillClips();
    polygonCoverageAndClipping();
    linesClipAndEndpoints();
    blitClipsAndKeys();
    fadeDarkensTowardBackground();
    dirtyBoundsTrackClippedWrites();
    std::puts("framebuffer tests passed");
    return 0;
}
