#include "tinygame.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

#include "fade.h"
#include "framebuffer.h"
#include "model.h"
#include "tg_random.h"

#if defined(TILTBLOCK_PROFILE) && defined(ARDUINO)
#include <esp_timer.h>
#define TILTBLOCK_PROFILING 1
#endif

namespace tiltblock {

namespace {

constexpr float SCALE = 1.25f;
constexpr float HALF_X = 9.0f * SCALE;
constexpr float HALF_Y = 4.5f * SCALE;
constexpr float CUBE_HEIGHT = HALF_X * 1.11803398875f;
constexpr float TWIST_VISUAL_SCALE = 3.0f;
constexpr float BOARD_X = 160.0f;
constexpr float BOARD_Y = 82.0f;
constexpr std::uint32_t CUBE_DARK = 0x35465B;
constexpr std::uint32_t CUBE_SIDE = 0x233145;
constexpr std::uint32_t CUBE_GRID = 0x1B2635;
constexpr std::uint32_t CUBE_WALL = 0x74849A;
constexpr std::uint32_t CUBE_HOLE = 0x101722;
constexpr std::uint32_t CUBE_TOUCHED_FRONT = 0x16877B;
constexpr std::uint32_t CUBE_TOUCHED_RIGHT = 0x12675F;
constexpr std::uint32_t SHADOW = 0x18202B;
constexpr std::uint16_t BG565 = fb::rgb565(tg::color::BG);
constexpr FadeTable FRONT_FADE = makeFade(CUBE_SIDE, tg::color::BG);
constexpr FadeTable RIGHT_FADE = makeFade(CUBE_DARK, tg::color::BG);
constexpr FadeTable GRID_FADE = makeFade(CUBE_GRID, tg::color::BG);

using Point = fb::Point;

struct Glyph {
    std::uint8_t row[5];
};

constexpr std::uint16_t glyphBits(std::uint8_t a, std::uint8_t b, std::uint8_t c,
                                  std::uint8_t d, std::uint8_t e) {
    return static_cast<std::uint16_t>(a | (b << 3) | (c << 6) | (d << 9) | (e << 12));
}

std::uint16_t glyph(char ch) {
    switch (ch) {
        case 'A': return glyphBits(2, 5, 7, 5, 5);
        case 'B': return glyphBits(6, 5, 6, 5, 6);
        case 'C': return glyphBits(3, 4, 4, 4, 3);
        case 'D': return glyphBits(6, 5, 5, 5, 6);
        case 'E': return glyphBits(7, 4, 6, 4, 7);
        case 'F': return glyphBits(7, 4, 6, 4, 4);
        case 'G': return glyphBits(3, 4, 5, 5, 3);
        case 'H': return glyphBits(5, 5, 7, 5, 5);
        case 'I': return glyphBits(7, 2, 2, 2, 7);
        case 'J': return glyphBits(1, 1, 1, 5, 2);
        case 'K': return glyphBits(5, 5, 6, 5, 5);
        case 'L': return glyphBits(4, 4, 4, 4, 7);
        case 'M': return glyphBits(5, 7, 7, 5, 5);
        case 'N': return glyphBits(5, 7, 7, 7, 5);
        case 'O': return glyphBits(2, 5, 5, 5, 2);
        case 'P': return glyphBits(6, 5, 6, 4, 4);
        case 'Q': return glyphBits(2, 5, 5, 3, 1);
        case 'R': return glyphBits(6, 5, 6, 5, 5);
        case 'S': return glyphBits(3, 4, 2, 1, 6);
        case 'T': return glyphBits(7, 2, 2, 2, 2);
        case 'U': return glyphBits(5, 5, 5, 5, 7);
        case 'V': return glyphBits(5, 5, 5, 5, 2);
        case 'W': return glyphBits(5, 5, 7, 7, 5);
        case 'X': return glyphBits(5, 5, 2, 5, 5);
        case 'Y': return glyphBits(5, 5, 2, 2, 2);
        case 'Z': return glyphBits(7, 1, 2, 4, 7);
        case '0': return glyphBits(7, 5, 5, 5, 7);
        case '1': return glyphBits(2, 6, 2, 2, 7);
        case '2': return glyphBits(6, 1, 2, 4, 7);
        case '3': return glyphBits(6, 1, 2, 1, 6);
        case '4': return glyphBits(5, 5, 7, 1, 1);
        case '5': return glyphBits(7, 4, 6, 1, 6);
        case '6': return glyphBits(3, 4, 7, 5, 7);
        case '7': return glyphBits(7, 1, 2, 2, 2);
        case '8': return glyphBits(7, 5, 7, 5, 7);
        case '9': return glyphBits(7, 5, 7, 1, 6);
        case ':': return glyphBits(0, 2, 0, 2, 0);
        case '!': return glyphBits(2, 2, 2, 0, 2);
        case '.': return glyphBits(0, 0, 0, 0, 2);
        case '-': return glyphBits(0, 0, 7, 0, 0);
        case '/': return glyphBits(1, 1, 2, 4, 4);
        case '+': return glyphBits(0, 2, 7, 2, 0);
        default: return 0;
    }
}

class CanvasView {
public:
    void build(lv_obj_t* screen) {
        buffer_ = lv_draw_buf_create(tg::WIDTH, tg::HEIGHT, LV_COLOR_FORMAT_RGB565, 0);
        if (!buffer_) {
            tg::log("tiltblock: could not allocate canvas buffer");
            return;
        }
        canvas_ = lv_canvas_create(screen);
        if (!canvas_) {
            lv_draw_buf_destroy(buffer_);
            buffer_ = nullptr;
            tg::log("tiltblock: could not create canvas");
            return;
        }
        lv_canvas_set_draw_buf(canvas_, buffer_);
        // Draw straight into the canvas memory; LVGL still presents it.
        fb_ = fb::Framebuffer(reinterpret_cast<std::uint16_t*>(buffer_->data), tg::WIDTH,
                              tg::HEIGHT, static_cast<int>(buffer_->header.stride / 2));
        lv_obj_set_pos(canvas_, 0, 0);
        lv_obj_set_size(canvas_, tg::WIDTH, tg::HEIGHT);
        fb_.fill(BG565);
        shown_scene_ = fb_.dirty();
        render(State{}, 0.0f, 0.0f, 0);
    }

    ~CanvasView() {
        if (buffer_) lv_draw_buf_destroy(buffer_);
    }

    void render(const State& state, float raw_x, float raw_y, std::uint32_t best) {
        if (!canvas_ || !buffer_ || !fb_.valid()) return;
        // Erase only last frame's pixels and present only what changed: the
        // LVGL/SPI flush, not the drawing, is the slow part. The HUD strip is
        // tracked separately so its full width doesn't widen the board area,
        // and is only presented when its pixels actually change.
        fb_.clear(shown_hud_, BG565);
        fb_.clear(shown_scene_, BG565);
        fb_.resetDirty();
        hud_drawn_ = fb::Rect{};
        renderScene(state, raw_x, raw_y, best);
        const fb::Rect scene = fb_.dirty();
        const fb::Rect scene_area = fb::unite(shown_scene_, scene);
        const fb::Rect hud_area = fb::unite(shown_hud_, hud_drawn_);
        shown_scene_ = scene;
        shown_hud_ = hud_drawn_;
        const std::uint32_t hud_sum = checksum(hud_area);
        if (hud_sum != hud_sum_ || force_hud_) invalidate(hud_area);
        hud_sum_ = hud_sum;
        force_hud_ = false;
        invalidate(scene_area);
    }

private:
    void invalidate(const fb::Rect& r) {
        if (r.empty()) return;
        const lv_area_t area{r.x0, r.y0, r.x1, r.y1};
        lv_obj_invalidate_area(canvas_, &area);
    }

    std::uint32_t checksum(const fb::Rect& r) const {
        std::uint32_t sum = 2166136261U;
        for (int y = std::max(r.y0, 0); y <= std::min(r.y1, fb_.height() - 1); ++y) {
            for (int x = std::max(r.x0, 0); x <= std::min(r.x1, fb_.width() - 1); ++x) {
                sum = (sum ^ fb_.at(x, y)) * 16777619U;
            }
        }
        return sum ^ static_cast<std::uint32_t>(r.x0 + r.y0 * 7 + r.x1 * 131 + r.y1 * 1031);
    }

    void renderScene(const State& state, float raw_x, float raw_y, std::uint32_t best) {
        state_ = &state;
        camera_x_ = camera_y_ = 0;
        if (state.stage == Stage::Playing && state.earthquake > 0.0f) {
            const int tick = static_cast<int>(state.earthquake * 24.0f);
            camera_x_ = (tick % 3 - 1) * 2;
            camera_y_ = ((tick + 1) % 3 - 1) * 2;
        }
        fb_.setOffset(camera_x_, camera_y_);
        if (state.stage == Stage::ModeSelect) {
            drawModeSelect(state);
            return;
        }
        if (state.stage == Stage::Calibration) {
            drawCalibration(raw_x, raw_y);
            return;
        }
        drawHud(state);
        hud_drawn_ = fb_.dirty();
        fb_.resetDirty();
        drawBoard(state);
        if (state.stage == Stage::TractorSave) drawTractor(state);
        drawBall(state);
        if (state.monster_alive) drawMonster(state);
        if (state.laser_flash > 0.0f) {
            line(boardPoint(state.laser_x1, state.laser_y1),
                 boardPoint(state.laser_x2, state.laser_y2), tg::color::ORANGE, 2);
        }
        if (state.stage == Stage::LayerDrop || state.stage == Stage::LevelClear) {
            box(100, 79, 120, 25, tg::color::SURFACE);
            boxOutline(100, 79, 120, 25, tg::color::LINE);
            text(116, 86, state.stage == Stage::LevelClear ? "LEVEL CLEAR" : "LAYER CLEAR",
                 tg::color::GOOD, 2);
        } else if (state.stage == Stage::Over) {
            drawGameOver(state, best);
        }
    }

    // Colours are 0xRRGGBB at call sites and packed once per primitive.
    void pixel(int x, int y, std::uint32_t color) { fb_.pixel(x, y, fb::rgb565(color)); }

    void box(int x, int y, int w, int h, std::uint32_t color) {
        fb_.rect(x, y, w, h, fb::rgb565(color));
    }

    void boxOutline(int x, int y, int w, int h, std::uint32_t color) {
        fb_.rectOutline(x, y, w, h, fb::rgb565(color));
    }

    void line(Point a, Point b, std::uint32_t color, int width = 1) {
        fb_.line(a, b, fb::rgb565(color), width);
    }

    void polygon(const Point* points, int count, std::uint32_t color) {
        fb_.polygon(points, count, fb::rgb565(color));
    }

    void diamond(Point center, float hx, float hy, std::uint32_t color) {
        const Point points[] = {{center.x - hx, center.y}, {center.x, center.y - hy},
                                {center.x + hx, center.y}, {center.x, center.y + hy}};
        polygon(points, 4, color);
    }

    void outlineDiamond(Point center, float hx, float hy, std::uint32_t color) {
        const Point left{center.x - hx, center.y};
        const Point top{center.x, center.y - hy};
        const Point right{center.x + hx, center.y};
        const Point bottom{center.x, center.y + hy};
        line(left, top, color);
        line(top, right, color);
        line(right, bottom, color);
        line(bottom, left, color);
    }

    void glyphAt(int x, int y, char ch, std::uint32_t color, int scale = 1) {
        const std::uint16_t bits = glyph(ch);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (bits & (1U << (row * 3 + 2 - col))) {
                    box(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }

    void text(int x, int y, const char* value, std::uint32_t color, int scale = 1) {
        for (int i = 0; value[i]; ++i) {
            glyphAt(x + i * 4 * scale, y, value[i], color, scale);
        }
    }

    Point boardPoint(float x, float y, float z = 0.0f) const {
        const float col = x - 4.0f;
        const float row = y - 4.0f;
        const float left = state_ ? state_->left_angle / 20.0f : 0.0f;
        const float right = state_ ? state_->right_angle / 20.0f : 0.0f;
        const float tilt_height = (-right * col + left * row) * TWIST_VISUAL_SCALE;
        return {BOARD_X + (col - row) * HALF_X,
                BOARD_Y + (col + row) * HALF_Y - tilt_height - z};
    }

    static Point lower(Point point, float depth) {
        point.y += depth;
        return point;
    }

    // One side of the cube stack, drawn as one band per cube layer. Lower
    // bands fade toward the background so the stack rises out of the dark.
    void drawStackSide(Point a, Point b, float depth, const FadeTable& face,
                       std::uint32_t touched_color, bool touched) {
        if (depth <= 0.0f) return;
        const std::uint16_t touched565 = fb::rgb565(touched_color);
        for (int band = 0; band < FADE_BANDS; ++band) {
            const float top = static_cast<float>(band) * CUBE_HEIGHT;
            if (top >= depth - 0.01f) break;
            const float bottom = std::min(top + CUBE_HEIGHT, depth);
            // Nothing below the screen edge can be seen.
            if (std::min(a.y, b.y) + top + camera_y_ >= tg::HEIGHT) break;
            const std::uint16_t fill = band == 0 && touched ? touched565 : face.band[band];
            const std::uint16_t seam = GRID_FADE.band[band];
            if (fill != BG565) {
                const Point quad[] = {lower(a, top), lower(b, top), lower(b, bottom),
                                      lower(a, bottom)};
                fb_.polygon(quad, 4, fill);
            }
            if (seam != BG565) {
                fb_.line(lower(a, top), lower(b, top), seam);
                fb_.line(lower(a, top), lower(a, bottom), seam);
                fb_.line(lower(b, top), lower(b, bottom), seam);
                if (bottom >= depth - 0.01f) fb_.line(lower(a, bottom), lower(b, bottom), seam);
            }
        }
    }

    void drawModeSelect(const State& state) {
        box(0, 0, tg::WIDTH, 18, tg::color::SURFACE);
        box(0, 16, 40, 2, tg::color::MINT);
        text(12, 4, "TILTBLOCK", tg::color::TEXT, 2);
        text(108, 34, "ROLL THE CUBE", tg::color::ACCENT, 2);
        box(76, 62, 168, 22, state.mode == Mode::Tilt ? tg::color::ACCENT : tg::color::SURFACE);
        box(76, 91, 168, 22, state.mode == Mode::Twist ? tg::color::ACCENT : tg::color::SURFACE);
        text(144, 68, "TILT", state.mode == Mode::Tilt ? tg::color::BG : tg::color::TEXT, 2);
        text(140, 97, "TWIST", state.mode == Mode::Twist ? tg::color::BG : tg::color::TEXT, 2);
        text(74, 128, "TURN: MODE  CLICK: SELECT", tg::color::TEXT_DIM);
        text(84, 146, "CLEAR EVERY CUBE", tg::color::TEXT_DIM);
    }

    void drawCalibration(float raw_x, float raw_y) {
        box(0, 0, tg::WIDTH, 18, tg::color::SURFACE);
        box(0, 16, 40, 2, tg::color::MINT);
        text(12, 4, "TILT CALIBRATION", tg::color::TEXT, 2);
        const int x = 96;
        const int y = 42;
        const int size = 128;
        box(x, y, size, size, tg::color::SURFACE);
        boxOutline(x, y, size, size, tg::color::LINE);
        line({x + size / 2.0f, static_cast<float>(y + 8)},
             {x + size / 2.0f, static_cast<float>(y + size - 8)}, tg::color::LINE);
        line({static_cast<float>(x + 8), y + size / 2.0f},
             {static_cast<float>(x + size - 8), y + size / 2.0f}, tg::color::LINE);
        const int dot_x = x + size / 2 + static_cast<int>(std::clamp(raw_x, -1.0f, 1.0f) * 48);
        const int dot_y = y + size / 2 + static_cast<int>(std::clamp(raw_y, -1.0f, 1.0f) * 48);
        box(dot_x - 3, dot_y - 3, 7, 7, tg::color::YELLOW);
        text(60, 148, "CENTER DOT CLICK EITHER KNOB", tg::color::TEXT_DIM);
    }

    void drawHud(const State& state) {
        box(0, 0, tg::WIDTH, 18, tg::color::SURFACE);
        box(0, 16, 32, 2, tg::color::MINT);
        char score[16];
        char level[16];
        std::snprintf(score, sizeof(score), "S %lu", static_cast<unsigned long>(state.score));
        std::snprintf(level, sizeof(level), "L%d-%d", state.level, state.layer);
        text(6, 6, score, tg::color::TEXT);
        text(154, 6, level, tg::color::TEXT_DIM);
        int x = 252;
        if (state.tractor) {
            box(x, 6, 5, 5, tg::color::PLUM);
            pixel(x + 2, 5, tg::color::PINK);
            x += 8;
        }
        if (state.laser_armed) {
            box(x, 6, 5, 5, tg::color::ORANGE);
            pixel(x + 2, 4, tg::color::YELLOW);
            x += 8;
        }
        for (int i = 0; i < state.lives && i < MAX_LIVES; ++i) {
            box(x + i * 7, 6, 5, 6, tg::color::GOOD);
        }
    }

    void drawBoard(const State& state) {
        const float stack_layers = Model::stackDepthLayersForView(state);
        const float depth = stack_layers * CUBE_HEIGHT;
        for (int sum = 0; sum <= 2 * (SIDE - 1); ++sum) {
            for (int row = 0; row < SIDE; ++row) {
                const int col = sum - row;
                if (col < 0 || col >= SIDE) continue;
                const int index = Model::cell(col, row);
                const Point p = boardPoint(col + 0.5f, row + 0.5f);
                if (row == SIDE - 1) {
                    const Point left = boardPoint(static_cast<float>(col),
                                                  static_cast<float>(row + 1));
                    const Point bottom = boardPoint(static_cast<float>(col + 1),
                                                    static_cast<float>(row + 1));
                    drawStackSide(left, bottom, depth, FRONT_FADE, CUBE_TOUCHED_FRONT,
                                  state.touched[index]);
                }
                if (col == SIDE - 1) {
                    const Point right = boardPoint(static_cast<float>(col + 1),
                                                   static_cast<float>(row));
                    const Point bottom = boardPoint(static_cast<float>(col + 1),
                                                    static_cast<float>(row + 1));
                    drawStackSide(right, bottom, depth, RIGHT_FADE, CUBE_TOUCHED_RIGHT,
                                  state.touched[index]);
                }
                if (state.hole[index]) {
                    diamond(p, HALF_X, HALF_Y, CUBE_HOLE);
                    outlineDiamond(p, HALF_X, HALF_Y, CUBE_GRID);
                    line({p.x - HALF_X + 2, p.y}, {p.x, p.y - HALF_Y + 1}, tg::color::LINE);
                    line({p.x + HALF_X - 2, p.y}, {p.x, p.y - HALF_Y + 1}, tg::color::LINE);
                    continue;
                }
                if (state.wall[index]) {
                    const Point raised = boardPoint(col + 0.5f, row + 0.5f, CUBE_HEIGHT);
                    const Point left = boardPoint(static_cast<float>(col),
                                                  static_cast<float>(row + 1));
                    const Point bottom = boardPoint(static_cast<float>(col + 1),
                                                    static_cast<float>(row + 1));
                    const Point right = boardPoint(static_cast<float>(col + 1),
                                                   static_cast<float>(row));
                    const Point raised_left = boardPoint(static_cast<float>(col),
                                                         static_cast<float>(row + 1),
                                                         CUBE_HEIGHT);
                    const Point raised_bottom = boardPoint(static_cast<float>(col + 1),
                                                           static_cast<float>(row + 1),
                                                           CUBE_HEIGHT);
                    const Point raised_right = boardPoint(static_cast<float>(col + 1),
                                                          static_cast<float>(row),
                                                          CUBE_HEIGHT);
                    const Point left_side[] = {left, bottom, raised_bottom, raised_left};
                    const Point right_side[] = {right, bottom, raised_bottom, raised_right};
                    const std::uint32_t touched_front = state.touched[index]
                        ? CUBE_TOUCHED_FRONT : CUBE_SIDE;
                    const std::uint32_t touched_right = state.touched[index]
                        ? CUBE_TOUCHED_RIGHT : CUBE_DARK;
                    polygon(left_side, 4, touched_front);
                    polygon(right_side, 4, touched_right);
                    const std::uint32_t top = state.touched[index] ? tg::color::MINT : CUBE_WALL;
                    diamond(raised, HALF_X, HALF_Y, top);
                    outlineDiamond(raised, HALF_X, HALF_Y, CUBE_GRID);
                    line({raised.x - HALF_X, raised.y}, {raised.x, raised.y - HALF_Y}, tg::color::SILVER);
                    continue;
                }
                std::uint32_t color = state.touched[index] ? tg::color::MINT : CUBE_DARK;
                switch (state.bonus[index]) {
                    case Bonus::Life: color = tg::color::LIME; break;
                    case Bonus::Zone: color = tg::color::SKY; break;
                    case Bonus::Boing: color = tg::color::YELLOW; break;
                    case Bonus::Quake: color = tg::color::RED; break;
                    case Bonus::Tractor: color = tg::color::PLUM; break;
                    case Bonus::Laser: color = tg::color::ORANGE; break;
                    case Bonus::None: break;
                }
                diamond(p, HALF_X, HALF_Y, color);
                outlineDiamond(p, HALF_X, HALF_Y, CUBE_GRID);
                const char icon = state.bonus[index] == Bonus::Life ? '+'
                    : state.bonus[index] == Bonus::Zone ? 'Z'
                    : state.bonus[index] == Bonus::Boing ? 'B'
                    : state.bonus[index] == Bonus::Quake ? 'Q'
                    : state.bonus[index] == Bonus::Tractor ? 'T'
                    : state.bonus[index] == Bonus::Laser ? '!' : '\0';
                if (icon) glyphAt(static_cast<int>(p.x) - 1, static_cast<int>(p.y) - 2,
                                  icon, tg::color::BG);
            }
        }
    }

    void drawBall(const State& state) {
        Point p = boardPoint(state.x, state.y);
        if (state.stage == Stage::TractorSave) {
            p = boardPoint(4.0f, 4.0f);
        }
        if (state.safety > 0.0f && static_cast<int>(state.safety * 12.0f) % 2 == 0) return;
        const int x = static_cast<int>(std::lround(p.x));
        const int y = static_cast<int>(std::lround(p.y));
        box(x - 5, y + 4, 10, 3, SHADOW);
        box(x - 4, y - 3, 8, 6, tg::color::WHITE);
        box(x - 3, y - 4, 6, 9, tg::color::WHITE);
        if (state.laser_armed) pixel(x, y - 4, tg::color::ORANGE);
        if (state.airborne > 0.0f) {
            box(x - 6, y + 6, 12, 1, tg::color::YELLOW);
        }
    }

    void drawMonster(const State& state) {
        const Point p = boardPoint(state.monster_x, state.monster_y, 2.0f);
        const int x = static_cast<int>(std::lround(p.x));
        const int y = static_cast<int>(std::lround(p.y));
        box(x - 5, y - 4, 10, 7, tg::color::FOREST);
        box(x - 4, y - 6, 8, 3, tg::color::LIME);
        box(x - 3, y - 2, 2, 2, tg::color::WHITE);
        box(x + 2, y - 2, 2, 2, tg::color::WHITE);
        pixel(x - 2, y - 2, tg::color::BG);
        pixel(x + 3, y - 2, tg::color::BG);
        box(x - 3, y + 3, 2, 2, tg::color::LIME);
        box(x + 2, y + 3, 2, 2, tg::color::LIME);
    }

    void drawTractor(const State& state) {
        const Point p = boardPoint(4.0f, 4.0f, 18.0f);
        const int x = static_cast<int>(std::lround(p.x));
        const int y = static_cast<int>(std::lround(p.y));
        box(x - 9, y - 3, 18, 5, tg::color::PLUM);
        box(x - 5, y - 6, 10, 3, tg::color::PINK);
        line({static_cast<float>(x), static_cast<float>(y + 2)},
             {static_cast<float>(x), y + 22.0f}, tg::color::SKY, 2);
    }

    void drawGameOver(const State& state, std::uint32_t best) {
        box(64, 54, 192, 66, tg::color::SURFACE);
        boxOutline(64, 54, 192, 66, tg::color::BAD);
        text(124, 61, "GAME OVER", tg::color::BAD, 2);
        char score[20];
        char best_text[20];
        std::snprintf(score, sizeof(score), "SCORE %lu", static_cast<unsigned long>(state.score));
        std::snprintf(best_text, sizeof(best_text), "BEST %lu", static_cast<unsigned long>(best));
        text(100, 80, score, tg::color::TEXT);
        text(104, 91, best_text, tg::color::WARN);
        text(72, 131, "CLICK: MODE   HOLD: PAUSE", tg::color::TEXT_DIM);
    }

    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* buffer_ = nullptr;
    fb::Framebuffer fb_;
    fb::Rect shown_scene_;
    fb::Rect shown_hud_;
    fb::Rect hud_drawn_;
    std::uint32_t hud_sum_ = 0;
    bool force_hud_ = true;
    const State* state_ = nullptr;
    int camera_x_ = 0;
    int camera_y_ = 0;
};

}  // namespace

class Tiltblock : public tg::Game {
public:
    void start(lv_obj_t* screen) override {
        seed_ = lv_tick_get();
        model_ = Model(seed_);
        model_.mutableState().best = tg::bestScore();
        view_.build(screen);
        view_.render(model_.state(), 0.0f, 0.0f, model_.state().best);
        tg::led(tg::color::MINT);
    }

    void update(const tg::Input& input, float dt) override {
#ifdef TILTBLOCK_PROFILING
        const std::int64_t t0 = esp_timer_get_time();
        bool rendered = false;
#endif
        const State before = model_.state();
        const unsigned events = model_.update(input, dt);
#ifdef TILTBLOCK_PROFILING
        const std::int64_t t1 = esp_timer_get_time();
#endif
        State& state = model_.mutableState();
        if ((events & GAME_OVER) != 0) {
            if (tg::submitScore(state.score)) state.best = state.score;
        }
        if ((events & LIFE_LOST) != 0) tg::flashLed(tg::color::BAD, 300);
        else if ((events & LEVEL_CLEARED) != 0) tg::flashLed(tg::color::GOOD, 500);
        else if ((events & LAYER_CLEARED) != 0) tg::flashLed(tg::color::MINT, 180);
        else if (state.score > before.score) tg::flashLed(tg::color::YELLOW, 90);
        if (state.stage != before.stage && state.stage == Stage::ModeSelect) {
            tg::led(tg::color::MINT);
        }

        draw_time_ += std::clamp(dt, 0.0f, 0.1f);
        if (draw_time_ >= 1.0f / 30.0f || events != NO_EVENT) {
            draw_time_ = std::fmod(draw_time_, 1.0f / 30.0f);
            view_.render(state, input.tilt_x, input.tilt_y, state.best);
#ifdef TILTBLOCK_PROFILING
            rendered = true;
#endif
        }
#ifdef TILTBLOCK_PROFILING
        profile(t0, t1, esp_timer_get_time(), rendered, dt);
#endif
    }

private:
#ifdef TILTBLOCK_PROFILING
    // Opt-in device timing: build with PLATFORMIO_BUILD_FLAGS=-DTILTBLOCK_PROFILE.
    // Aggregates are logged every 5 s; nothing is printed per frame.
    struct Profile {
        std::int64_t last = 0;
        std::int64_t window_start = 0;
        std::int64_t model = 0, render = 0, interval = 0, other_on_render = 0;
        std::int64_t max_model = 0, max_render = 0, max_interval = 0;
        std::int64_t lost = 0;
        std::uint32_t updates = 0, renders = 0, over33 = 0, over100 = 0;
        double sim = 0.0;
        std::int64_t pending_render = -1;
    } prof_;

    void profile(std::int64_t t0, std::int64_t t1, std::int64_t t2, bool rendered, float dt) {
        Profile& p = prof_;
        if (p.last != 0) {
            const std::int64_t interval = t0 - p.last;
            p.interval += interval;
            p.max_interval = std::max(p.max_interval, interval);
            if (interval > 33333) ++p.over33;
            if (interval > 100000) { ++p.over100; p.lost += interval - 100000; }
            // Time outside the game after a rendered frame = LVGL compose + SPI flush.
            if (p.pending_render >= 0) p.other_on_render += t0 - p.pending_render;
        } else {
            p.window_start = t0;
        }
        p.pending_render = rendered ? t2 : -1;
        p.last = t0;
        p.sim += dt;
        p.model += t1 - t0;
        p.max_model = std::max(p.max_model, t1 - t0);
        ++p.updates;
        if (rendered) {
            ++p.renders;
            p.render += t2 - t1;
            p.max_render = std::max(p.max_render, t2 - t1);
        }
        const std::int64_t wall = t0 - p.window_start;
        if (wall >= 5000000 && p.updates > 1) {
            const unsigned r = std::max<std::uint32_t>(p.renders, 1);
            auto L = [](std::int64_t v) { return static_cast<long>(v); };
            tg::log("[TB] wall=%ldms sim=%ldms upd=%lu fps=%ld model=%ld/%ldus raster=%ld/%ldus",
                    L(wall / 1000), L(static_cast<std::int64_t>(p.sim * 1000.0)),
                    static_cast<unsigned long>(p.updates),
                    L(static_cast<std::int64_t>(p.renders) * 1000000 / wall),
                    L(p.model / p.updates), L(p.max_model), L(p.render / r), L(p.max_render));
            tg::log("[TB] lvgl+flush=%ldus interval=%ld/%ldus >33ms=%lu >100ms=%lu lost=%ldms "
                    "stage=%d layer=%d",
                    L(p.other_on_render / r), L(p.interval / (p.updates - 1)), L(p.max_interval),
                    static_cast<unsigned long>(p.over33), static_cast<unsigned long>(p.over100),
                    L(p.lost / 1000), static_cast<int>(model_.state().stage),
                    model_.state().layer);
            const std::int64_t keep = p.last;
            p = Profile{};
            p.last = keep;
            p.window_start = keep;
        }
    }
#endif
    CanvasView view_;
    Model model_;
    std::uint32_t seed_ = 1;
    float draw_time_ = 0.0f;
};

}  // namespace tiltblock

TINYGAME(tiltblock::Tiltblock, "tiltblock", "TILTBLOCK", tg::color::MINT)
