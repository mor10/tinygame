#include "view.h"

#include <cmath>

#include "tinygame.h"

namespace brickbreaker {

namespace {

namespace color = tg::color;
using tg::Font;

constexpr std::uint32_t ROW_COLORS[config::BRICK_ROWS] = {color::RED, color::ORANGE,
                                                          color::YELLOW, color::LIME, color::SKY};
constexpr std::uint32_t COLOR_TOUGH = color::SILVER;
constexpr std::uint32_t COLOR_PADDLE = color::WHITE;
constexpr std::uint32_t COLOR_BALL = color::YELLOW;
constexpr std::uint32_t COLOR_BOLT = color::PEACH;
constexpr int LIFE_BLOCK = 6;

std::int32_t px(float value) {
    return static_cast<std::int32_t>(std::lround(value));
}

std::uint32_t brickColor(std::uint8_t row, std::uint8_t hits) {
    return hits > 1 ? COLOR_TOUGH : ROW_COLORS[row];
}

void setColor(lv_obj_t* obj, std::uint32_t rgb) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), LV_PART_MAIN);
}

}  // namespace

std::uint32_t powerUpColor(game::PowerUp type) {
    switch (type) {
        case game::PowerUp::Wide:
            return color::SKY;
        case game::PowerUp::Laser:
            return color::PINK;
        case game::PowerUp::Life:
            return color::LIME;
        case game::PowerUp::Multi:
            return color::ORANGE;
        case game::PowerUp::Big:
            return color::YELLOW;
        case game::PowerUp::None:
            break;
    }
    return color::WHITE;
}

void View::build(lv_obj_t* screen) {
    // HUD strip: score left, level in the middle, lives as blocks on the right.
    tg::ui::block(screen, 0, 0, tg::WIDTH, px(config::HUD_HEIGHT), color::SURFACE);
    tg::ui::block(screen, 0, px(config::HUD_HEIGHT) - 2, 32, 2, color::ORANGE);
    score_label_ = tg::ui::label(screen, "", Font::S, color::TEXT);
    lv_obj_set_pos(score_label_, tg::GAP_M, 5);
    level_label_ = tg::ui::label(screen, "", Font::S, color::TEXT_DIM);
    lv_obj_align(level_label_, LV_ALIGN_TOP_MID, 0, 5);
    for (std::uint8_t i = 0; i < config::MAX_LIVES; ++i) {
        life_blocks_[i] = tg::ui::block(screen, tg::WIDTH - tg::GAP_M - (i + 1) * (LIFE_BLOCK + 2),
                                        6, LIFE_BLOCK, LIFE_BLOCK, color::GOOD);
    }

    for (std::uint8_t row = 0; row < config::BRICK_ROWS; ++row) {
        for (std::uint8_t col = 0; col < config::BRICK_COLUMNS; ++col) {
            const physics::Rect r = game::Game::brickRect(row, col);
            const std::int32_t x1 = px(r.x);
            bricks_[row][col] = tg::ui::block(screen, x1, px(r.y), px(r.x + r.w) - x1, px(r.h),
                                              ROW_COLORS[row]);
            shown_hits_[row][col] = 1;
        }
    }

    for (Particle& p : particles_) {
        p.obj = tg::ui::block(screen, 0, 0, config::PARTICLE_SIZE, config::PARTICLE_SIZE,
                              color::WHITE);
        lv_obj_set_hidden(p.obj, true);
    }
    for (Sprite& c : capsules_) {
        c.obj = tg::ui::block(screen, 0, 0, px(config::CAPSULE_WIDTH), px(config::CAPSULE_HEIGHT),
                              color::WHITE);
        lv_obj_set_style_border_color(c.obj, lv_color_hex(color::WHITE), LV_PART_MAIN);
        lv_obj_set_style_border_width(c.obj, 1, LV_PART_MAIN);
        lv_obj_set_hidden(c.obj, true);
    }
    for (Sprite& b : bolts_) {
        b.obj = tg::ui::block(screen, 0, 0, px(config::BOLT_WIDTH), px(config::BOLT_HEIGHT),
                              COLOR_BOLT);
        lv_obj_set_hidden(b.obj, true);
    }

    paddle_ = tg::ui::block(screen, 0, 0, px(config::PADDLE_WIDTH), px(config::PADDLE_HEIGHT),
                            COLOR_PADDLE);
    paddle_color_ = COLOR_PADDLE;
    // A square ball, 8-bit style.
    const std::int32_t ball_size = px(config::BALL_RADIUS * 2.0f);
    ball_.obj = tg::ui::block(screen, 0, 0, ball_size, ball_size, COLOR_BALL);
    ball_.visible = true;
    for (Sprite& e : extra_balls_) {
        e.obj = tg::ui::block(screen, 0, 0, ball_size, ball_size, COLOR_BALL);
        lv_obj_set_hidden(e.obj, true);
    }

    banner_ = tg::ui::label(screen, "", Font::M, color::ACCENT);
    lv_obj_align(banner_, LV_ALIGN_TOP_MID, 0, 94);
    banner_hint_ = tg::ui::label(screen, "CLICK: LAUNCH   HOLD: PAUSE", Font::S, color::TEXT_DIM);
    lv_obj_align(banner_hint_, LV_ALIGN_TOP_MID, 0, 124);
    game_over_ = tg::ui::popup(screen, "GAME OVER", "");
}

void View::update(const game::State& state, std::uint8_t events, float dt,
                  std::uint32_t best, bool new_best) {
    updateHud(state);
    updateBricks(state);
    if ((events & game::EVENT_BRICK_BROKEN) != 0) {
        spawnBurst(state.last_broken_row, state.last_broken_col);
    }
    advanceParticles(dt);

    for (std::uint8_t i = 0; i < config::CAPSULE_POOL_SIZE; ++i) {
        const game::Capsule& c = state.capsules[i];
        if (c.active && c.type != capsules_[i].type) {
            capsules_[i].type = c.type;
            setColor(capsules_[i].obj, powerUpColor(c.type));
        }
        place(capsules_[i], c.active, c.position, config::CAPSULE_WIDTH, config::CAPSULE_HEIGHT);
    }
    for (std::uint8_t i = 0; i < config::BOLT_POOL_SIZE; ++i) {
        place(bolts_[i], state.bolts[i].active, state.bolts[i].position, config::BOLT_WIDTH,
              config::BOLT_HEIGHT);
    }
    updatePaddleAndBall(state);
    updateMessages(state, best, new_best);
}

void View::updateHud(const game::State& state) {
    if (state.score != shown_score_) {
        shown_score_ = state.score;
        tg::ui::text(score_label_, "S %06lu", static_cast<unsigned long>(state.score));
    }
    if (state.level != shown_level_) {
        shown_level_ = state.level;
        tg::ui::text(level_label_, "L %lu", static_cast<unsigned long>(state.level));
        tg::ui::text(banner_, "LEVEL %lu", static_cast<unsigned long>(state.level));
        lv_obj_align(banner_, LV_ALIGN_TOP_MID, 0, 94);
    }
    if (state.lives != shown_lives_) {
        shown_lives_ = state.lives;
        for (std::uint8_t i = 0; i < config::MAX_LIVES; ++i) {
            lv_obj_set_hidden(life_blocks_[i], i >= state.lives);
        }
    }
}

void View::updateBricks(const game::State& state) {
    for (std::uint8_t row = 0; row < config::BRICK_ROWS; ++row) {
        for (std::uint8_t col = 0; col < config::BRICK_COLUMNS; ++col) {
            const std::uint8_t hits = state.bricks[row][col];
            const std::uint8_t shown = shown_hits_[row][col];
            if (hits == shown) {
                continue;
            }
            shown_hits_[row][col] = hits;
            lv_obj_set_hidden(bricks_[row][col], hits == 0);
            if (hits > 0) {
                setColor(bricks_[row][col], brickColor(row, hits));
            }
        }
    }
}

void View::updatePaddleAndBall(const game::State& state) {
    const std::int32_t w = px(state.paddle.w);
    if (w != paddle_w_) {
        paddle_w_ = w;
        lv_obj_set_width(paddle_, w);
    }
    const std::uint32_t paddle_color = state.laser_seconds_left > 0.0f  ? color::PINK
                                       : state.wide_seconds_left > 0.0f ? color::SKY
                                                                        : COLOR_PADDLE;
    if (paddle_color != paddle_color_) {
        paddle_color_ = paddle_color;
        setColor(paddle_, paddle_color);
    }
    const std::int32_t x = px(state.paddle.x);
    if (x != paddle_x_) {
        paddle_x_ = x;
        lv_obj_set_pos(paddle_, x, px(state.paddle.y));
    }

    // The Big power-up changes the ball size; resize the sprites only when it does.
    const float ball_size = state.ball.radius * 2.0f;
    if (px(ball_size) != ball_size_) {
        ball_size_ = px(ball_size);
        lv_obj_set_size(ball_.obj, ball_size_, ball_size_);
        for (Sprite& e : extra_balls_) {
            lv_obj_set_size(e.obj, ball_size_, ball_size_);
        }
        // Force a re-position so the bigger sprite stays centred.
        ball_.x = -1;
        for (Sprite& e : extra_balls_) {
            e.x = -1;
        }
    }
    place(ball_, !state.game_over, state.ball.position, ball_size, ball_size);
    for (std::uint8_t i = 0; i < config::EXTRA_BALL_COUNT; ++i) {
        const game::ExtraBall& e = state.extra_balls[i];
        place(extra_balls_[i], e.active, e.ball.position, ball_size, ball_size);
    }
}

void View::updateMessages(const game::State& state, std::uint32_t best, bool new_best) {
    const bool show_banner = !state.launched && !state.game_over;
    if (show_banner != banner_visible_) {
        banner_visible_ = show_banner;
        lv_obj_set_hidden(banner_, !show_banner);
        lv_obj_set_hidden(banner_hint_, !show_banner);
    }
    if (state.game_over != game_over_visible_) {
        game_over_visible_ = state.game_over;
        lv_obj_set_hidden(game_over_, !state.game_over);
        if (state.game_over) {
            tg::ui::result(game_over_, state.score, best, new_best);
        }
    }
}

void View::place(Sprite& sprite, bool active, physics::Vec2 centre, float w, float h) {
    if (active != sprite.visible) {
        sprite.visible = active;
        lv_obj_set_hidden(sprite.obj, !active);
    }
    if (!active) {
        return;
    }
    const std::int32_t x = px(centre.x - w / 2.0f);
    const std::int32_t y = px(centre.y - h / 2.0f);
    if (x != sprite.x || y != sprite.y) {
        sprite.x = x;
        sprite.y = y;
        lv_obj_set_pos(sprite.obj, x, y);
    }
}

void View::spawnBurst(std::uint8_t row, std::uint8_t col) {
    // Eight fixed directions (45 degree steps), alternating speeds.
    constexpr float DIRS[8][2] = {{1.0f, 0.0f},   {0.707f, 0.707f},   {0.0f, 1.0f},
                                  {-0.707f, 0.707f}, {-1.0f, 0.0f},   {-0.707f, -0.707f},
                                  {0.0f, -1.0f},  {0.707f, -0.707f}};
    const physics::Rect r = game::Game::brickRect(row, col);
    for (std::uint8_t i = 0; i < config::PARTICLES_PER_BURST; ++i) {
        Particle& p = particles_[next_particle_];
        next_particle_ = static_cast<std::uint8_t>((next_particle_ + 1) % config::PARTICLE_POOL_SIZE);
        const float speed = i % 2 == 0 ? config::PARTICLE_SPEED_FAST : config::PARTICLE_SPEED_SLOW;
        p.x = r.x + r.w / 2.0f;
        p.y = r.y + r.h / 2.0f;
        p.vx = DIRS[i % 8][0] * speed;
        p.vy = DIRS[i % 8][1] * speed;
        p.age = 0.0f;
        p.active = true;
        setColor(p.obj, ROW_COLORS[row]);
        lv_obj_set_hidden(p.obj, false);
    }
}

void View::advanceParticles(float dt) {
    for (Particle& p : particles_) {
        if (!p.active) {
            continue;
        }
        p.age += dt;
        p.vy += config::PARTICLE_GRAVITY * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (p.age >= config::PARTICLE_LIFETIME_MS / 1000.0f || p.y > tg::HEIGHT) {
            p.active = false;
            lv_obj_set_hidden(p.obj, true);
            continue;
        }
        lv_obj_set_pos(p.obj, px(p.x) - config::PARTICLE_SIZE / 2,
                       px(p.y) - config::PARTICLE_SIZE / 2);
    }
}

}  // namespace brickbreaker
