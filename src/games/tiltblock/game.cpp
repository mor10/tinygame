#include "tinygame.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

#include "model.h"
#include "tg_random.h"

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

struct Point {
    float x;
    float y;
};

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
        lv_obj_set_pos(canvas_, 0, 0);
        lv_obj_set_size(canvas_, tg::WIDTH, tg::HEIGHT);
        render(State{}, 0.0f, 0.0f, 0);
    }

    ~CanvasView() {
        if (buffer_) lv_draw_buf_destroy(buffer_);
    }

    void render(const State& state, float raw_x, float raw_y, std::uint32_t best) {
        if (!canvas_ || !buffer_) return;
        state_ = &state;
        camera_x_ = camera_y_ = 0;
        if (state.stage == Stage::Playing && state.earthquake > 0.0f) {
            const int tick = static_cast<int>(state.earthquake * 24.0f);
            camera_x_ = (tick % 3 - 1) * 2;
            camera_y_ = ((tick + 1) % 3 - 1) * 2;
        }
        lv_canvas_fill_bg(canvas_, lv_color_hex(tg::color::BG), LV_OPA_COVER);
        if (state.stage == Stage::ModeSelect) {
            drawModeSelect(state);
            return;
        }
        if (state.stage == Stage::Calibration) {
            drawCalibration(raw_x, raw_y);
            return;
        }
        drawHud(state);
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

private:
    void pixel(int x, int y, std::uint32_t color) {
        x += camera_x_;
        y += camera_y_;
        if (x < 0 || y < 0 || x >= tg::WIDTH || y >= tg::HEIGHT) return;
        lv_canvas_set_px(canvas_, x, y, lv_color_hex(color), LV_OPA_COVER);
    }

    void box(int x, int y, int w, int h, std::uint32_t color) {
        for (int py = y; py < y + h; ++py) {
            for (int px = x; px < x + w; ++px) pixel(px, py, color);
        }
    }

    void boxOutline(int x, int y, int w, int h, std::uint32_t color) {
        for (int px = x; px < x + w; ++px) {
            pixel(px, y, color);
            pixel(px, y + h - 1, color);
        }
        for (int py = y; py < y + h; ++py) {
            pixel(x, py, color);
            pixel(x + w - 1, py, color);
        }
    }

    void line(Point a, Point b, std::uint32_t color, int width = 1) {
        int x0 = static_cast<int>(std::lround(a.x));
        int y0 = static_cast<int>(std::lround(a.y));
        const int x1 = static_cast<int>(std::lround(b.x));
        const int y1 = static_cast<int>(std::lround(b.y));
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            box(x0 - width / 2, y0 - width / 2, width, width, color);
            if (x0 == x1 && y0 == y1) break;
            const int twice = 2 * error;
            if (twice >= dy) error += dy, x0 += sx;
            if (twice <= dx) error += dx, y0 += sy;
        }
    }

    void polygon(const Point* points, int count, std::uint32_t color) {
        float min_y = points[0].y;
        float max_y = points[0].y;
        for (int i = 1; i < count; ++i) {
            min_y = std::min(min_y, points[i].y);
            max_y = std::max(max_y, points[i].y);
        }
        const int top = static_cast<int>(std::floor(min_y));
        const int bottom = static_cast<int>(std::ceil(max_y));
        for (int y = top; y <= bottom; ++y) {
            float intersections[8];
            int n = 0;
            for (int i = 0; i < count; ++i) {
                const Point a = points[i];
                const Point b = points[(i + 1) % count];
                if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                    intersections[n++] = a.x + (static_cast<float>(y) - a.y) *
                                         (b.x - a.x) / (b.y - a.y);
                }
            }
            if (n < 2) continue;
            std::sort(intersections, intersections + n);
            const int left = static_cast<int>(std::ceil(intersections[0]));
            const int right = static_cast<int>(std::floor(intersections[n - 1]));
            for (int x = left; x <= right; ++x) pixel(x, y, color);
        }
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

    void drawStackSide(Point a, Point b, float depth, std::uint32_t color,
                       std::uint32_t touched_color, bool touched) {
        if (depth <= 0.0f) return;
        const Point face[] = {a, b, lower(b, depth), lower(a, depth)};
        polygon(face, 4, color);
        if (touched) {
            const float top_cube_depth = std::min(depth, CUBE_HEIGHT);
            const Point top_cube[] = {a, b, lower(b, top_cube_depth),
                                      lower(a, top_cube_depth)};
            polygon(top_cube, 4, touched_color);
        }

        for (int layer = 0; layer <= static_cast<int>(depth / CUBE_HEIGHT); ++layer) {
            const float offset = std::min(static_cast<float>(layer) * CUBE_HEIGHT, depth);
            line(lower(a, offset), lower(b, offset), CUBE_GRID);
        }
        const float last_grid = std::floor(depth / CUBE_HEIGHT) * CUBE_HEIGHT;
        if (depth - last_grid > 0.01f) {
            line(lower(a, depth), lower(b, depth), CUBE_GRID);
        }
        line(a, lower(a, depth), CUBE_GRID);
        line(b, lower(b, depth), CUBE_GRID);
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
                    drawStackSide(left, bottom, depth, CUBE_SIDE, CUBE_TOUCHED_FRONT,
                                  state.touched[index]);
                }
                if (col == SIDE - 1) {
                    const Point right = boardPoint(static_cast<float>(col + 1),
                                                   static_cast<float>(row));
                    const Point bottom = boardPoint(static_cast<float>(col + 1),
                                                    static_cast<float>(row + 1));
                    drawStackSide(right, bottom, depth, CUBE_DARK, CUBE_TOUCHED_RIGHT,
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
        const State before = model_.state();
        const unsigned events = model_.update(input, dt);
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
        }
    }

private:
    CanvasView view_;
    Model model_;
    std::uint32_t seed_ = 1;
    float draw_time_ = 0.0f;
};

}  // namespace tiltblock

TINYGAME(tiltblock::Tiltblock, "tiltblock", "TILTBLOCK", tg::color::MINT)
