#pragma once

#include <cstdint>

#include "config.h"
#include "physics.h"

namespace brickbreaker::game {

inline constexpr std::uint8_t EVENT_BRICK_HIT = 1U << 0;
inline constexpr std::uint8_t EVENT_LIFE_LOST = 1U << 1;
inline constexpr std::uint8_t EVENT_LEVEL_CLEARED = 1U << 2;
inline constexpr std::uint8_t EVENT_GAME_OVER = 1U << 3;
// A brick was destroyed (a hit on a tough brick only raises BRICK_HIT).
inline constexpr std::uint8_t EVENT_BRICK_BROKEN = 1U << 4;
// A falling capsule was caught; see State::last_powerup.
inline constexpr std::uint8_t EVENT_POWERUP_CAUGHT = 1U << 5;

enum class PowerUp : std::uint8_t { None, Wide, Laser, Life, Multi, Big };

struct ExtraBall {
    bool active = false;
    physics::Ball ball;
};

struct Capsule {
    bool active = false;
    PowerUp type = PowerUp::None;
    // Centre of the capsule.
    physics::Vec2 position;
};

struct Bolt {
    bool active = false;
    // Centre of the bolt.
    physics::Vec2 position;
};

// Simulation options, also used to set up deterministic tests.
struct Options {
    std::uint8_t lives = config::STARTING_LIVES;
    float speed_scale = 1.0f;
    // Seed for power-up drops; any value (0 is remapped).
    std::uint32_t seed = 1;
};

struct State {
    physics::Ball ball;
    physics::Rect paddle;
    bool launched = false;
    bool game_over = false;
    std::uint32_t score = 0;
    std::uint8_t lives = 0;
    std::uint32_t level = 1;
    float ball_speed = 0.0f;
    // Hits left per brick; 0 = no brick.
    std::uint8_t bricks[config::BRICK_ROWS][config::BRICK_COLUMNS] = {};
    std::uint16_t bricks_remaining = 0;
    // Most recently destroyed brick (for visual effects).
    std::uint8_t last_broken_row = 0;
    std::uint8_t last_broken_col = 0;
    // Additional balls from the Multi power-up. `ball` is always the main
    // ball; a life is lost only when it and every extra ball are gone.
    ExtraBall extra_balls[config::EXTRA_BALL_COUNT];
    Capsule capsules[config::CAPSULE_POOL_SIZE];
    Bolt bolts[config::BOLT_POOL_SIZE];
    PowerUp last_powerup = PowerUp::None;
    float wide_seconds_left = 0.0f;
    float laser_seconds_left = 0.0f;
    float big_seconds_left = 0.0f;
    float fire_cooldown = 0.0f;
    std::uint32_t rng = 1;
};

// Pure brick-breaker simulation in logical screen coordinates. No LVGL or
// Arduino dependencies.
class Game {
public:
    void newGame(float paddle_target, const Options& options = Options{});
    void launch();
    // Fires laser bolts if Laser is active, the ball is in play and the
    // cooldown has elapsed. Returns true if bolts were fired.
    bool fire();
    // Advances the simulation by `elapsed_seconds` using fixed steps.
    // Returns the OR of all events that occurred.
    std::uint8_t update(float elapsed_seconds, float paddle_target);
    // Advances exactly one fixed step.
    std::uint8_t step(float paddle_target);

    const State& state() const;
    State& mutableState();

    static physics::Rect playField();
    static physics::Rect brickRect(std::uint8_t row, std::uint8_t column);

private:
    void loadLevel();
    // paddle_target: 0 = left end, 1 = right end. Glides there unless `snap`.
    void setPaddle(float paddle_target, bool snap = false);
    void attachBall();
    void clearPowerUps();
    void setPaddleWidth(float width);
    // Sets the size of the main ball and every extra ball.
    void setBallRadius(float radius);
    std::uint32_t nextRandom();
    std::uint8_t hitBrick(std::uint8_t row, std::uint8_t col);
    void maybeDropCapsule(std::uint8_t row, std::uint8_t col);
    std::uint8_t stepCapsules(float dt);
    std::uint8_t stepBolts(float dt);
    void tickEffects(float dt);
    std::uint8_t stepBall(physics::Ball& ball);
    void splitBall();

    State state_;
    Options options_;
    float accumulator_ = 0.0f;
};

}  // namespace brickbreaker::game
