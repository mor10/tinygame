#pragma once

#include <lvgl.h>

#include <cstdint>

#include "config.h"
#include "logic.h"

namespace rockrain {

// Capsule / LED colour for each power-up.
std::uint32_t powerUpColor(logic::PowerUp type);

// Draws the game with long-lived LVGL objects that are only touched when
// what they show changes.
class View {
public:
    void build(lv_obj_t* screen);
    // `events` are the logic::EVENT_* flags from this frame.
    void update(const logic::State& state, std::uint8_t events, float dt,
                std::uint32_t best, bool new_best);

private:
    // A pooled block plus what it last showed, so unchanged things aren't redrawn.
    struct Sprite {
        lv_obj_t* obj = nullptr;
        bool visible = false;
        std::int32_t x = -1;
        std::int32_t y = -1;
        std::int32_t w = -1;
        std::int32_t h = -1;
        std::uint32_t rgb = 0;
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

    void updateHud(const logic::State& state);
    void updateBoss(const logic::State& state);
    void updateRobot(const logic::State& state, float dt);
    void updateMessages(const logic::State& state, std::uint32_t best, bool new_best);
    void spawnBurst(const logic::State& state);
    void advanceParticles(float dt);
    static void place(Sprite& sprite, bool active, const logic::Rect& r, std::uint32_t rgb);

    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* power_label_ = nullptr;
    lv_obj_t* hearts_[config::MAX_LIVES] = {};
    lv_obj_t* robot_ = nullptr;
    lv_obj_t* game_over_ = nullptr;
    Sprite bricks_[config::BRICK_POOL_SIZE];
    Sprite bolts_[config::BOLT_POOL_SIZE];
    Sprite capsules_[config::CAPSULE_POOL_SIZE];
    Sprite boss_;
    lv_obj_t* boss_bar_ = nullptr;  // health bar fill inside the boss
    std::uint8_t shown_boss_hits_ = 0;
    lv_obj_t* capsule_letters_[config::CAPSULE_POOL_SIZE] = {};
    Particle particles_[config::PARTICLE_POOL_SIZE];
    std::uint8_t next_particle_ = 0;

    // Last drawn values, so labels and the robot are only touched on change.
    std::int32_t shown_robot_x_ = -1;
    bool shown_robot_visible_ = true;
    float blink_seconds_ = 0.0f;
    std::uint32_t shown_score_ = UINT32_MAX;
    std::int32_t shown_power_ = -1;
    std::uint8_t shown_lives_ = UINT8_MAX;
    bool shown_over_ = true;
};

}  // namespace rockrain
