#pragma once

#include <cstdint>

#include "config.h"

namespace rockrain::logic {

inline constexpr std::uint8_t EVENT_ROBOT_HIT = 1U << 0;
inline constexpr std::uint8_t EVENT_GAME_OVER = 1U << 1;
// A bolt hit a big brick but didn't break it.
inline constexpr std::uint8_t EVENT_BRICK_HIT = 1U << 2;
// A brick was destroyed; see State::last_broken_*.
inline constexpr std::uint8_t EVENT_BRICK_BROKEN = 1U << 3;
// A capsule was caught; see State::last_powerup.
inline constexpr std::uint8_t EVENT_POWERUP = 1U << 4;
// A bolt hit the boss but didn't break it.
inline constexpr std::uint8_t EVENT_BOSS_HIT = 1U << 5;

enum class PowerUp : std::uint8_t { Ammo, Rapid, Life };

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

bool overlaps(const Rect& a, const Rect& b);

struct Brick {
    bool active = false;
    bool big = false;
    std::uint8_t hits_left = 0;
    Rect rect;
    float speed = 0.0f;
};

struct Bolt {
    bool active = false;
    Rect rect;
};

struct Capsule {
    bool active = false;
    PowerUp type = PowerUp::Ammo;
    Rect rect;
};

struct Boss {
    bool active = false;
    std::uint8_t hits_left = 0;
    Rect rect;
    float vx = 0.0f;  // sideways speed while hovering (+ = right)
    float drop_timer = 0.0f;
};

struct State {
    float robot_x = 0.0f;  // left edge
    std::uint8_t lives = 0;
    std::uint32_t score = 0;
    std::uint8_t ammo = 0;
    float rapid_seconds_left = 0.0f;
    float invulnerable_seconds_left = 0.0f;
    float fire_cooldown = 0.0f;
    float elapsed = 0.0f;
    float survive_timer = 0.0f;
    float spawn_timer = 0.0f;
    float capsule_timer = 0.0f;
    float boss_timer = 0.0f;  // counts down only while no boss is out
    std::uint8_t capsules_since_life = 0;
    std::uint8_t wave_rows_left = 0;  // > 0 while a wave is falling
    std::uint32_t waves_done = 0;
    float wave_timer = 0.0f;
    bool game_over = false;
    Brick bricks[config::BRICK_POOL_SIZE];
    Bolt bolts[config::BOLT_POOL_SIZE];
    Capsule capsules[config::CAPSULE_POOL_SIZE];
    Boss boss;
    // Centre and colour hint of the most recently destroyed brick.
    float last_broken_x = 0.0f;
    float last_broken_y = 0.0f;
    bool last_broken_big = false;
    bool last_broken_boss = false;
    PowerUp last_powerup = PowerUp::Ammo;
    std::uint32_t rng = 1;
};

// Pure Rock Rain rules in screen pixels. No LVGL or Arduino.
class Game {
public:
    void newGame(std::uint32_t seed, float robot_target);
    // Shoots one bolt if there is ammo or rapid fire and the cooldown is over.
    bool fire();
    bool canShoot() const;
    // Advances by `seconds` in fixed steps; returns the OR of all events.
    std::uint8_t update(float seconds, float robot_target);
    std::uint8_t step(float robot_target);

    float fallSpeed() const;
    float spawnInterval() const;
    float waveSpeed() const;
    Rect robotHitbox() const;

    const State& state() const { return state_; }
    State& mutableState() { return state_; }

private:
    // robot_target: 0 = left end, 1 = right end. Glides there unless `snap`.
    void setRobot(float robot_target, bool snap = false);
    void spawnBrick();
    // Uses a free pooled brick, centred on `centre_x`, top edge at `top`.
    void spawnBrickAt(float centre_x, float top, bool big);
    // Uses a free pooled brick; false if the pool is full.
    bool addBrick(const Rect& r, bool big, float speed);
    void startWave(float first_row_delay);
    void spawnWaveRow();
    // Knocks the brick-sized piece under `x` out of a row stretch.
    bool knockOutPiece(Brick& stretch, float x);
    void spawnCapsule();
    void spawnBoss();
    // Costs a heart and starts the blinking; returns the events it causes.
    std::uint8_t hurtRobot();
    void breakThing(const Rect& r, bool big, bool boss, std::uint16_t points);
    std::uint8_t stepBolts(float dt);
    std::uint8_t stepBricks(float dt);
    std::uint8_t stepCapsules(float dt);
    void stepBoss(float dt);
    float randomRange(float lo, float hi);
    std::uint32_t nextRandom();
    float random01();

    State state_;
    float accumulator_ = 0.0f;
};

}  // namespace rockrain::logic
