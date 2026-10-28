#include "view.h"

#include <cmath>

#include "tinygame.h"

namespace rockrain {

namespace {

namespace color = tg::color;
using tg::Font;

constexpr std::uint32_t COLOR_SMALL_BRICK = color::ORANGE;
constexpr std::uint32_t COLOR_BIG_BRICK = color::RED;
constexpr std::uint32_t COLOR_CRACKED_BRICK = color::PEACH;
constexpr std::uint32_t COLOR_MORTAR = color::BROWN;
constexpr std::uint32_t COLOR_BOLT = color::YELLOW;
constexpr std::uint32_t COLOR_BOSS = color::PLUM;
constexpr int BOSS_BORDER = 2;
constexpr int BOSS_BAR_INSET = 2;  // inside the border
constexpr int BOSS_BAR_H = 3;

// The boss's angry frowny face, one character per 3x3 fat pixel.
constexpr const char* BOSS_FACE[] = {
    "#......#",
    ".##..##.",
    ".##..##.",
    "........",
    "..####..",
    ".#....#.",
    "#......#",
};
constexpr int BOSS_FACE_PX = 3;
constexpr int BOSS_FACE_TOP = 1;
constexpr int HEART = 6;
constexpr std::uint32_t BLINK_MS = 100;

// The robot, one character per 2x2 fat pixel: '#' body, 'c' cannon, 'e' eyes.
constexpr const char* ROBOT_ART[] = {
    "...c...",
    "...c...",
    ".#####.",
    ".#e#e#.",
    ".#####.",
    "#######",
    ".#...#.",
    ".#...#.",
};
constexpr int ROBOT_PX = 2;

std::int32_t px(float value) {
    return static_cast<std::int32_t>(std::lround(value));
}

void setColor(lv_obj_t* obj, std::uint32_t rgb) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), LV_PART_MAIN);
}

std::uint32_t robotPixelColor(char c) {
    return c == 'c' ? color::SILVER : c == 'e' ? color::YELLOW : color::MINT;
}

lv_obj_t* makeRobot(lv_obj_t* parent) {
    lv_obj_t* group = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_size(group, px(config::ROBOT_WIDTH), px(config::ROBOT_HEIGHT));
    int y = 0;
    for (const char* row : ROBOT_ART) {
        for (int x = 0; row[x] != '\0'; ++x) {
            if (row[x] != '.') {
                tg::ui::block(group, x * ROBOT_PX, y * ROBOT_PX, ROBOT_PX, ROBOT_PX,
                              robotPixelColor(row[x]));
            }
        }
        ++y;
    }
    return group;
}

}  // namespace

std::uint32_t powerUpColor(logic::PowerUp type) {
    switch (type) {
        case logic::PowerUp::Rapid:
            return color::PINK;
        case logic::PowerUp::Life:
            return color::LIME;
        case logic::PowerUp::Ammo:
            break;
    }
    return color::SKY;
}

namespace {

const char* powerUpLetter(logic::PowerUp type) {
    switch (type) {
        case logic::PowerUp::Rapid:
            return "R";
        case logic::PowerUp::Life:
            return "+";
        case logic::PowerUp::Ammo:
            break;
    }
    return "A";
}

}  // namespace

void View::build(lv_obj_t* screen) {
    for (Sprite& b : bricks_) {
        b.obj = tg::ui::block(screen, 0, 0, 1, 1, COLOR_SMALL_BRICK);
        lv_obj_set_style_border_color(b.obj, lv_color_hex(COLOR_MORTAR), LV_PART_MAIN);
        lv_obj_set_style_border_width(b.obj, 2, LV_PART_MAIN);
        lv_obj_set_hidden(b.obj, true);
    }
    for (std::uint8_t i = 0; i < config::CAPSULE_POOL_SIZE; ++i) {
        Sprite& c = capsules_[i];
        c.obj = tg::ui::block(screen, 0, 0, px(config::CAPSULE_SIZE), px(config::CAPSULE_SIZE),
                              color::SKY);
        lv_obj_set_style_border_color(c.obj, lv_color_hex(color::WHITE), LV_PART_MAIN);
        lv_obj_set_style_border_width(c.obj, 1, LV_PART_MAIN);
        capsule_letters_[i] = tg::ui::label(c.obj, "A", Font::S, color::WHITE);
        lv_obj_center(capsule_letters_[i]);
        lv_obj_set_hidden(c.obj, true);
    }
    // Boss: a framed square with a frowny face and a health bar along the
    // bottom. Child positions start inside the border.
    const int boss_w = px(config::BOSS_WIDTH);
    const int boss_h = px(config::BOSS_HEIGHT);
    const int inner_w = boss_w - 2 * BOSS_BORDER;
    const int inner_h = boss_h - 2 * BOSS_BORDER;
    boss_.obj = tg::ui::block(screen, 0, 0, boss_w, boss_h, COLOR_BOSS);
    lv_obj_set_style_border_color(boss_.obj, lv_color_hex(color::SILVER), LV_PART_MAIN);
    lv_obj_set_style_border_width(boss_.obj, BOSS_BORDER, LV_PART_MAIN);
    const int face_left = (inner_w - 8 * BOSS_FACE_PX) / 2;
    int face_y = 0;
    for (const char* row : BOSS_FACE) {
        for (int x = 0; row[x] != '\0'; ++x) {
            if (row[x] != '.') {
                tg::ui::block(boss_.obj, face_left + x * BOSS_FACE_PX,
                              BOSS_FACE_TOP + face_y * BOSS_FACE_PX, BOSS_FACE_PX, BOSS_FACE_PX,
                              color::WHITE);
            }
        }
        ++face_y;
    }
    const int bar_w = inner_w - 2 * BOSS_BAR_INSET;
    lv_obj_t* track = tg::ui::block(boss_.obj, BOSS_BAR_INSET, inner_h - BOSS_BAR_H - 1, bar_w,
                                    BOSS_BAR_H, color::BLACK);
    boss_bar_ = tg::ui::block(track, 0, 0, bar_w, BOSS_BAR_H, color::LIME);
    shown_boss_hits_ = config::BOSS_HITS;
    lv_obj_set_hidden(boss_.obj, true);

    for (Sprite& b : bolts_) {
        b.obj = tg::ui::block(screen, 0, 0, px(config::BOLT_WIDTH), px(config::BOLT_HEIGHT),
                              COLOR_BOLT);
        lv_obj_set_hidden(b.obj, true);
    }
    for (Particle& p : particles_) {
        p.obj = tg::ui::block(screen, 0, 0, config::PARTICLE_SIZE, config::PARTICLE_SIZE,
                              COLOR_SMALL_BRICK);
        lv_obj_set_hidden(p.obj, true);
    }
    robot_ = makeRobot(screen);
    lv_obj_set_y(robot_, px(config::ROBOT_Y));

    // HUD strip on top of everything that falls from behind it.
    tg::ui::block(screen, 0, 0, tg::WIDTH, px(config::HUD_HEIGHT), color::SURFACE);
    tg::ui::block(screen, 0, px(config::HUD_HEIGHT) - 2, 32, 2, color::ORANGE);
    score_label_ = tg::ui::label(screen, "", Font::S, color::TEXT);
    lv_obj_set_pos(score_label_, tg::GAP_M, 3);
    power_label_ = tg::ui::label(screen, "", Font::S, color::SKY);
    lv_obj_align(power_label_, LV_ALIGN_TOP_MID, 0, 3);
    for (std::uint8_t i = 0; i < config::MAX_LIVES; ++i) {
        hearts_[i] = tg::ui::block(screen, tg::WIDTH - tg::GAP_M - (i + 1) * (HEART + 2), 4,
                                   HEART, HEART, color::BAD);
    }

    game_over_ = tg::ui::popup(screen, "GAME OVER", "CLICK: PLAY AGAIN");
    lv_obj_set_hidden(game_over_, true);
    shown_over_ = false;
}

void View::update(const logic::State& state, std::uint8_t events, float dt,
                  std::uint32_t best, bool new_best) {
    for (std::uint8_t i = 0; i < config::BRICK_POOL_SIZE; ++i) {
        const logic::Brick& b = state.bricks[i];
        const std::uint32_t rgb = !b.big ? COLOR_SMALL_BRICK
                                  : b.hits_left > 1 ? COLOR_BIG_BRICK
                                                    : COLOR_CRACKED_BRICK;
        place(bricks_[i], b.active, b.rect, rgb);
    }
    for (std::uint8_t i = 0; i < config::CAPSULE_POOL_SIZE; ++i) {
        const logic::Capsule& c = state.capsules[i];
        const std::uint32_t rgb = powerUpColor(c.type);
        if (c.active && rgb != capsules_[i].rgb) {
            lv_label_set_text(capsule_letters_[i], powerUpLetter(c.type));
            lv_obj_center(capsule_letters_[i]);
        }
        place(capsules_[i], c.active, c.rect, rgb);
    }
    updateBoss(state);
    for (std::uint8_t i = 0; i < config::BOLT_POOL_SIZE; ++i) {
        place(bolts_[i], state.bolts[i].active, state.bolts[i].rect, COLOR_BOLT);
    }
    if (events & logic::EVENT_BRICK_BROKEN) {
        spawnBurst(state);
    }
    advanceParticles(dt);
    updateRobot(state, dt);
    updateHud(state);
    updateMessages(state, best, new_best);
}

void View::updateHud(const logic::State& state) {
    if (state.score != shown_score_) {
        shown_score_ = state.score;
        tg::ui::text(score_label_, "S %lu", static_cast<unsigned long>(state.score));
    }
    // Rapid shows whole seconds left (+100 so it can't clash with an ammo count).
    const std::int32_t power =
        state.rapid_seconds_left > 0.0f
            ? 100 + static_cast<std::int32_t>(std::ceil(state.rapid_seconds_left))
            : state.ammo;
    if (power != shown_power_) {
        shown_power_ = power;
        if (power >= 100) {
            tg::ui::text(power_label_, "RAPID %ld", static_cast<long>(power - 100));
            lv_obj_set_style_text_color(power_label_, lv_color_hex(color::PINK), LV_PART_MAIN);
        } else if (power > 0) {
            tg::ui::text(power_label_, "AMMO %ld", static_cast<long>(power));
            lv_obj_set_style_text_color(power_label_, lv_color_hex(color::SKY), LV_PART_MAIN);
        } else {
            lv_label_set_text(power_label_, "");
        }
        lv_obj_align(power_label_, LV_ALIGN_TOP_MID, 0, 3);
    }
    if (state.lives != shown_lives_) {
        shown_lives_ = state.lives;
        // The first 3 slots always show (grey when lost); extra hearts only
        // appear while you have them.
        for (std::uint8_t i = 0; i < config::MAX_LIVES; ++i) {
            setColor(hearts_[i], i < state.lives ? color::BAD : color::LINE);
            lv_obj_set_hidden(hearts_[i], i >= state.lives && i >= config::STARTING_LIVES);
        }
    }
}

void View::updateBoss(const logic::State& state) {
    const logic::Boss& boss = state.boss;
    place(boss_, boss.active, boss.rect, COLOR_BOSS);
    if (boss.active && boss.hits_left != shown_boss_hits_) {
        shown_boss_hits_ = boss.hits_left;
        const int full = px(config::BOSS_WIDTH) - 2 * BOSS_BORDER - 2 * BOSS_BAR_INSET;
        lv_obj_set_width(boss_bar_, full * boss.hits_left / config::BOSS_HITS);
    }
}

void View::updateRobot(const logic::State& state, float dt) {
    const std::int32_t x = px(state.robot_x);
    if (x != shown_robot_x_) {
        shown_robot_x_ = x;
        lv_obj_set_x(robot_, x);
    }
    // Blink while safe after a hit.
    bool visible = true;
    if (state.invulnerable_seconds_left > 0.0f && !state.game_over) {
        blink_seconds_ += dt;
        visible = (static_cast<std::uint32_t>(blink_seconds_ * 1000.0f) / BLINK_MS) % 2 == 0;
    } else {
        blink_seconds_ = 0.0f;
    }
    if (visible != shown_robot_visible_) {
        shown_robot_visible_ = visible;
        lv_obj_set_hidden(robot_, !visible);
    }
}

void View::updateMessages(const logic::State& state, std::uint32_t best, bool new_best) {
    if (state.game_over == shown_over_) {
        return;
    }
    shown_over_ = state.game_over;
    if (state.game_over) {
        tg::ui::result(game_over_, state.score, best, new_best);
        lv_obj_move_foreground(game_over_);
    }
    lv_obj_set_hidden(game_over_, !state.game_over);
}

void View::spawnBurst(const logic::State& state) {
    constexpr float DIRS[8][2] = {{1.0f, 0.0f},      {0.707f, 0.707f},  {0.0f, 1.0f},
                                  {-0.707f, 0.707f}, {-1.0f, 0.0f},     {-0.707f, -0.707f},
                                  {0.0f, -1.0f},     {0.707f, -0.707f}};
    const std::uint32_t rgb = state.last_broken_boss  ? COLOR_BOSS
                              : state.last_broken_big ? COLOR_BIG_BRICK
                                                      : COLOR_SMALL_BRICK;
    // A boss explodes with every particle, in two rings.
    const std::uint8_t count =
        state.last_broken_boss ? config::PARTICLE_POOL_SIZE : config::PARTICLES_PER_BURST;
    for (std::uint8_t i = 0; i < count; ++i) {
        Particle& p = particles_[next_particle_];
        next_particle_ = static_cast<std::uint8_t>((next_particle_ + 1) % config::PARTICLE_POOL_SIZE);
        p.x = state.last_broken_x;
        p.y = state.last_broken_y;
        const float speed = i < 8 ? config::PARTICLE_SPEED : config::PARTICLE_SPEED * 1.6f;
        p.vx = DIRS[i % 8][0] * speed;
        p.vy = DIRS[i % 8][1] * speed;
        p.age = 0.0f;
        p.active = true;
        setColor(p.obj, rgb);
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
        lv_obj_set_pos(p.obj, px(p.x), px(p.y));
    }
}

void View::place(Sprite& sprite, bool active, const logic::Rect& r, std::uint32_t rgb) {
    if (active != sprite.visible) {
        sprite.visible = active;
        lv_obj_set_hidden(sprite.obj, !active);
    }
    if (!active) {
        return;
    }
    const std::int32_t w = px(r.w);
    const std::int32_t h = px(r.h);
    if (w != sprite.w || h != sprite.h) {
        sprite.w = w;
        sprite.h = h;
        lv_obj_set_size(sprite.obj, w, h);
    }
    if (rgb != sprite.rgb) {
        sprite.rgb = rgb;
        setColor(sprite.obj, rgb);
    }
    const std::int32_t x = px(r.x);
    const std::int32_t y = px(r.y);
    if (x != sprite.x || y != sprite.y) {
        sprite.x = x;
        sprite.y = y;
        lv_obj_set_pos(sprite.obj, x, y);
    }
}

}  // namespace rockrain
