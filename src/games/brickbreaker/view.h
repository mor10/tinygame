#pragma once

#include <lvgl.h>

#include <cstdint>

#include "config.h"
#include "game.h"

namespace brickbreaker {

// Capsule / LED colour for each power-up.
std::uint32_t powerUpColor(game::PowerUp type);

// Draws the simulation with long-lived LVGL objects that are only touched
// when what they show changes.
class View {
public:
    void build(lv_obj_t* screen);
    // `events` are the game::EVENT_* flags from this frame.
    void update(const game::State& state, std::uint8_t events, float dt,
                std::uint32_t best, bool new_best);

private:
    struct Sprite {
        lv_obj_t* obj = nullptr;
        bool visible = false;
        std::int32_t x = -1;
        std::int32_t y = -1;
        game::PowerUp type = game::PowerUp::None;
    };
    struct Particle {
        lv_obj_t* obj = nullptr;
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float age = 0.0f;
        bool active = false;
    };

    void updateHud(const game::State& state);
    void updateBricks(const game::State& state);
    void updatePaddleAndBall(const game::State& state);
    void updateMessages(const game::State& state, std::uint32_t best, bool new_best);
    void spawnBurst(std::uint8_t row, std::uint8_t col);
    void advanceParticles(float dt);
    static void place(Sprite& sprite, bool active, physics::Vec2 centre, float w, float h);

    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* level_label_ = nullptr;
    lv_obj_t* life_blocks_[config::MAX_LIVES] = {};
    lv_obj_t* bricks_[config::BRICK_ROWS][config::BRICK_COLUMNS] = {};
    lv_obj_t* paddle_ = nullptr;
    Sprite ball_;
    Sprite extra_balls_[config::EXTRA_BALL_COUNT];
    Sprite capsules_[config::CAPSULE_POOL_SIZE];
    Sprite bolts_[config::BOLT_POOL_SIZE];
    Particle particles_[config::PARTICLE_POOL_SIZE];
    std::uint8_t next_particle_ = 0;
    lv_obj_t* banner_ = nullptr;       // "LEVEL n" before each launch
    lv_obj_t* banner_hint_ = nullptr;
    lv_obj_t* game_over_ = nullptr;    // popup

    // Last drawn values.
    std::uint32_t shown_score_ = UINT32_MAX;
    std::uint8_t shown_lives_ = UINT8_MAX;
    std::uint32_t shown_level_ = 0;
    std::uint8_t shown_hits_[config::BRICK_ROWS][config::BRICK_COLUMNS] = {};
    std::int32_t paddle_x_ = -1;
    std::int32_t paddle_w_ = -1;
    std::uint32_t paddle_color_ = 0;
    std::int32_t ball_size_ = -1;
    bool banner_visible_ = true;
    bool game_over_visible_ = true;
};

}  // namespace brickbreaker
