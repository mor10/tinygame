#pragma once

// New in v2. What the first firmware cut shows: the engine's scrolling demo,
// with one line drawn over it that reports the knobs, clicks and tilt as the
// game tick received them. The demo itself ignores input; the line is there
// so a person holding the board can check every control.
// Portable (engine includes only), so it is also built by the host tests.

#include <cstdint>
#include <cstdio>

#include "../engine/core/input.h"
#include "../engine/demo/scroll_demo.h"
#include "../engine/gfx/canvas.h"
#include "../engine/gfx/font.h"
#include "input_logic.h"

namespace demo_scene {

class Scene {
public:
    bool init(int screen_w, int screen_h) {
        *this = Scene{};
        return demo_.init(screen_w, screen_h, tg::View::Standard, PALETTE);
    }

    // One 60 Hz step.
    void tick(const tg::Input& in) {
        left_total_ = input::boundedAdd(left_total_, in.left.delta);
        right_total_ = input::boundedAdd(right_total_, in.right.delta);
        main_total_ = input::boundedAdd(main_total_, in.main.delta);
        if (in.click) input::increment(clicks_);
        last_ = in;
        demo_.tick();
    }

    void draw(tg::gfx::Canvas& canvas) const {
        demo_.draw(canvas);
        char line[64];
        text(line, sizeof(line));
        const int height = tg::gfx::GLYPH_H * TEXT_SCALE + 2 * MARGIN;
        const int top = canvas.height() - height;
        canvas.resetClip();
        canvas.shade(0, top, canvas.width(), height, 160);
        tg::gfx::drawText(canvas, MARGIN, top + MARGIN, line, 0xFFFF, TEXT_SCALE);
    }

    // "L+12 R-4 M-4 C3 D1 T+10 -25": left, right and main knob totals, click
    // count, held (1) or not (0), tilt x and y in hundredths.
    void text(char* out, std::size_t size) const {
        std::snprintf(out, size, "L%+ld R%+ld M%+ld C%lu D%d T%+d %+d",
                      static_cast<long>(left_total_), static_cast<long>(right_total_),
                      static_cast<long>(main_total_), static_cast<unsigned long>(clicks_),
                      last_.down ? 1 : 0, percent(last_.tilt_x), percent(last_.tilt_y));
    }

    std::uint32_t ticks() const { return demo_.ticks(); }

private:
    static constexpr int PALETTE = 1;  // the palette the golden frames use
    static constexpr int TEXT_SCALE = 2;
    static constexpr int MARGIN = 2;
    static int percent(float unit) {
        return static_cast<int>(unit * 100.0f + (unit < 0.0f ? -0.5f : 0.5f));
    }

    tg::demo::ScrollDemo demo_;
    std::int32_t left_total_ = 0;
    std::int32_t right_total_ = 0;
    std::int32_t main_total_ = 0;
    std::uint32_t clicks_ = 0;
    tg::Input last_;
};

}  // namespace demo_scene
