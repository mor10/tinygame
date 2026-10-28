#include "game.h"

#include <algorithm>
#include <cmath>

#include "tg_random.h"
#include "tg_types.h"
#include "level.h"

namespace brickbreaker::game {

namespace {

constexpr float DEGREES_TO_RADIANS = 3.14159265f / 180.0f;
constexpr float SCREEN_WIDTH = static_cast<float>(tg::WIDTH);
constexpr float SCREEN_HEIGHT = static_cast<float>(tg::HEIGHT);
constexpr float BRICK_WIDTH =
    (SCREEN_WIDTH - 2.0f * config::BRICK_SIDE_MARGIN -
     (config::BRICK_COLUMNS - 1) * config::BRICK_GAP) /
    config::BRICK_COLUMNS;

float levelSpeed(std::uint32_t level, float scale) {
    const float speed = config::BALL_INITIAL_SPEED +
                        (level - 1) * config::BALL_SPEED_PER_LEVEL;
    return (speed < config::BALL_MAX_SPEED ? speed
                                                : config::BALL_MAX_SPEED) *
           scale;
}

bool overlaps(const physics::Rect& a, const physics::Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

physics::Rect centredRect(physics::Vec2 centre, float w, float h) {
    return {centre.x - w / 2.0f, centre.y - h / 2.0f, w, h};
}

}  // namespace

physics::Rect Game::playField() {
    return {0.0f, config::HUD_HEIGHT, SCREEN_WIDTH,
            SCREEN_HEIGHT - config::HUD_HEIGHT};
}

physics::Rect Game::brickRect(std::uint8_t row, std::uint8_t column) {
    return {config::BRICK_SIDE_MARGIN +
                column * (BRICK_WIDTH + config::BRICK_GAP),
            config::HUD_HEIGHT + config::BRICK_TOP_MARGIN +
                row * (config::BRICK_HEIGHT + config::BRICK_GAP),
            BRICK_WIDTH, config::BRICK_HEIGHT};
}

void Game::newGame(float paddle_target, const Options& options) {
    options_ = options;
    state_ = State{};
    state_.lives = options.lives;
    state_.rng = options.seed != 0 ? options.seed : 1U;
    state_.level = 1;
    state_.ball.radius = config::BALL_RADIUS;
    state_.paddle = {0.0f,
                     SCREEN_HEIGHT - config::PADDLE_BOTTOM_MARGIN -
                         config::PADDLE_HEIGHT,
                     config::PADDLE_WIDTH, config::PADDLE_HEIGHT};
    accumulator_ = 0.0f;
    loadLevel();
    setPaddle(paddle_target, true);
    attachBall();
}

void Game::loadLevel() {
    clearPowerUps();
    state_.bricks_remaining = 0;
    for (std::uint8_t row = 0; row < config::BRICK_ROWS; ++row) {
        for (std::uint8_t col = 0; col < config::BRICK_COLUMNS; ++col) {
            const std::uint8_t hits = level::brickHits(state_.level - 1, row, col);
            state_.bricks[row][col] = hits;
            if (hits > 0) {
                ++state_.bricks_remaining;
            }
        }
    }
    state_.ball_speed = levelSpeed(state_.level, options_.speed_scale);
    state_.launched = false;
}

void Game::launch() {
    if (state_.launched || state_.game_over) {
        return;
    }
    const float angle = config::BALL_LAUNCH_DEGREES * DEGREES_TO_RADIANS;
    state_.ball.velocity = {state_.ball_speed * std::sin(angle),
                            -state_.ball_speed * std::cos(angle)};
    state_.launched = true;
}

bool Game::fire() {
    if (!state_.launched || state_.game_over || state_.laser_seconds_left <= 0.0f ||
        state_.fire_cooldown > 0.0f) {
        return false;
    }
    const float y = state_.paddle.y - config::BOLT_HEIGHT / 2.0f;
    const float xs[2] = {state_.paddle.x + config::BOLT_EDGE_INSET,
                         state_.paddle.x + state_.paddle.w - config::BOLT_EDGE_INSET};
    std::uint8_t fired = 0;
    for (Bolt& bolt : state_.bolts) {
        if (fired == 2) {
            break;
        }
        if (!bolt.active) {
            bolt.active = true;
            bolt.position = {xs[fired], y};
            ++fired;
        }
    }
    if (fired == 0) {
        return false;
    }
    state_.fire_cooldown = config::BOLT_COOLDOWN_SECONDS;
    return true;
}

void Game::clearPowerUps() {
    for (Capsule& capsule : state_.capsules) {
        capsule.active = false;
    }
    for (Bolt& bolt : state_.bolts) {
        bolt.active = false;
    }
    for (ExtraBall& extra : state_.extra_balls) {
        extra.active = false;
    }
    state_.wide_seconds_left = 0.0f;
    state_.laser_seconds_left = 0.0f;
    state_.big_seconds_left = 0.0f;
    state_.fire_cooldown = 0.0f;
    setPaddleWidth(config::PADDLE_WIDTH);
    setBallRadius(config::BALL_RADIUS);
}

void Game::setBallRadius(float radius) {
    state_.ball.radius = radius;
    for (ExtraBall& extra : state_.extra_balls) {
        extra.ball.radius = radius;
    }
}

void Game::setPaddleWidth(float width) {
    // Keep the paddle centred where it was.
    const float centre = state_.paddle.x + state_.paddle.w / 2.0f;
    state_.paddle.w = width;
    float x = centre - width / 2.0f;
    if (x < 0.0f) {
        x = 0.0f;
    }
    if (x + width > SCREEN_WIDTH) {
        x = SCREEN_WIDTH - width;
    }
    state_.paddle.x = x;
}

std::uint32_t Game::nextRandom() {
    return tg::randomNext(state_.rng);
}

std::uint8_t Game::hitBrick(std::uint8_t row, std::uint8_t col) {
    --state_.bricks[row][col];
    state_.score += config::SCORE_PER_BRICK;
    std::uint8_t events = EVENT_BRICK_HIT;
    if (state_.bricks[row][col] == 0) {
        --state_.bricks_remaining;
        state_.last_broken_row = row;
        state_.last_broken_col = col;
        events |= EVENT_BRICK_BROKEN;
        maybeDropCapsule(row, col);
    }
    return events;
}

void Game::maybeDropCapsule(std::uint8_t row, std::uint8_t col) {
    if (nextRandom() % 100U >= config::POWERUP_DROP_PERCENT) {
        return;
    }
    for (Capsule& capsule : state_.capsules) {
        if (capsule.active) {
            continue;
        }
        const std::uint32_t roll = nextRandom() % 100U;
        if (roll < config::POWERUP_WEIGHT_WIDE) {
            capsule.type = PowerUp::Wide;
        } else if (roll < config::POWERUP_WEIGHT_WIDE + config::POWERUP_WEIGHT_LASER) {
            capsule.type = PowerUp::Laser;
        } else if (roll < config::POWERUP_WEIGHT_WIDE + config::POWERUP_WEIGHT_LASER +
                              config::POWERUP_WEIGHT_MULTI) {
            capsule.type = PowerUp::Multi;
        } else if (roll < config::POWERUP_WEIGHT_WIDE + config::POWERUP_WEIGHT_LASER +
                              config::POWERUP_WEIGHT_MULTI + config::POWERUP_WEIGHT_BIG) {
            capsule.type = PowerUp::Big;
        } else {
            capsule.type = PowerUp::Life;
        }
        const physics::Rect brick = brickRect(row, col);
        capsule.position = {brick.x + brick.w / 2.0f, brick.y + brick.h / 2.0f};
        capsule.active = true;
        return;
    }
}

std::uint8_t Game::stepCapsules(float dt) {
    std::uint8_t events = 0;
    for (Capsule& capsule : state_.capsules) {
        if (!capsule.active) {
            continue;
        }
        capsule.position.y += config::CAPSULE_FALL_SPEED * dt;
        const physics::Rect box = centredRect(capsule.position, config::CAPSULE_WIDTH,
                                              config::CAPSULE_HEIGHT);
        if (overlaps(box, state_.paddle)) {
            capsule.active = false;
            state_.last_powerup = capsule.type;
            events |= EVENT_POWERUP_CAUGHT;
            switch (capsule.type) {
                case PowerUp::Wide:
                    state_.wide_seconds_left = config::WIDE_SECONDS;
                    setPaddleWidth(config::WIDE_PADDLE_WIDTH);
                    break;
                case PowerUp::Laser:
                    state_.laser_seconds_left = config::LASER_SECONDS;
                    break;
                case PowerUp::Multi:
                    splitBall();
                    break;
                case PowerUp::Big:
                    state_.big_seconds_left = config::BIG_SECONDS;
                    setBallRadius(config::BIG_BALL_RADIUS);
                    break;
                case PowerUp::Life:
                    if (state_.lives < config::MAX_LIVES) {
                        ++state_.lives;
                    }
                    break;
                case PowerUp::None:
                    break;
            }
        } else if (box.y > SCREEN_HEIGHT) {
            capsule.active = false;
        }
    }
    return events;
}

std::uint8_t Game::stepBolts(float dt) {
    std::uint8_t events = 0;
    for (Bolt& bolt : state_.bolts) {
        if (!bolt.active) {
            continue;
        }
        bolt.position.y -= config::BOLT_SPEED * dt;
        const physics::Rect box =
            centredRect(bolt.position, config::BOLT_WIDTH, config::BOLT_HEIGHT);
        if (box.y + box.h < config::HUD_HEIGHT) {
            bolt.active = false;
            continue;
        }
        for (std::uint8_t row = 0; row < config::BRICK_ROWS && bolt.active; ++row) {
            for (std::uint8_t col = 0; col < config::BRICK_COLUMNS; ++col) {
                if (state_.bricks[row][col] != 0 && overlaps(box, brickRect(row, col))) {
                    events |= hitBrick(row, col);
                    bolt.active = false;
                    break;
                }
            }
        }
    }
    return events;
}

void Game::splitBall() {
    const physics::Ball& source = state_.ball;
    const float angles[2] = {config::MULTI_SPLIT_DEGREES * DEGREES_TO_RADIANS,
                             -config::MULTI_SPLIT_DEGREES * DEGREES_TO_RADIANS};
    std::uint8_t spawned = 0;
    for (ExtraBall& extra : state_.extra_balls) {
        if (extra.active) {
            continue;
        }
        const float a = angles[spawned % 2];
        const float c = std::cos(a);
        const float s = std::sin(a);
        extra.ball = source;
        extra.ball.velocity = {source.velocity.x * c - source.velocity.y * s,
                               source.velocity.x * s + source.velocity.y * c};
        physics::enforceMinVerticalSpeed(extra.ball, config::BALL_MIN_VERTICAL_SPEED);
        extra.active = true;
        ++spawned;
    }
}

std::uint8_t Game::stepBall(physics::Ball& ball) {
    const float dt = config::SIMULATION_STEP_SECONDS;
    ball.position.x += ball.velocity.x * dt;
    ball.position.y += ball.velocity.y * dt;

    std::uint8_t events = 0;
    bool bounced = physics::bounceWalls(ball, playField());
    bounced |= physics::bouncePaddle(
        ball, state_.paddle,
        config::PADDLE_MAX_BOUNCE_DEGREES * DEGREES_TO_RADIANS);

    // At most one brick per ball per step keeps resolution stable.
    bool hit = false;
    for (std::uint8_t row = 0; row < config::BRICK_ROWS && !hit; ++row) {
        for (std::uint8_t col = 0; col < config::BRICK_COLUMNS; ++col) {
            if (state_.bricks[row][col] == 0) {
                continue;
            }
            if (physics::bounceRect(ball, brickRect(row, col))) {
                events |= hitBrick(row, col);
                bounced = true;
                hit = true;
                break;
            }
        }
    }

    if (bounced) {
        physics::enforceMinVerticalSpeed(ball, config::BALL_MIN_VERTICAL_SPEED);
    }
    return events;
}

void Game::tickEffects(float dt) {
    if (state_.fire_cooldown > 0.0f) {
        state_.fire_cooldown = state_.fire_cooldown > dt ? state_.fire_cooldown - dt : 0.0f;
    }
    if (state_.laser_seconds_left > 0.0f) {
        state_.laser_seconds_left =
            state_.laser_seconds_left > dt ? state_.laser_seconds_left - dt : 0.0f;
    }
    if (state_.wide_seconds_left > 0.0f) {
        state_.wide_seconds_left =
            state_.wide_seconds_left > dt ? state_.wide_seconds_left - dt : 0.0f;
        if (state_.wide_seconds_left == 0.0f) {
            setPaddleWidth(config::PADDLE_WIDTH);
        }
    }
    if (state_.big_seconds_left > 0.0f) {
        state_.big_seconds_left =
            state_.big_seconds_left > dt ? state_.big_seconds_left - dt : 0.0f;
        if (state_.big_seconds_left == 0.0f) {
            setBallRadius(config::BALL_RADIUS);
        }
    }
}

void Game::setPaddle(float paddle_target, bool snap) {
    const float travel = SCREEN_WIDTH - state_.paddle.w;
    const float target = std::clamp(paddle_target, 0.0f, 1.0f) * travel;
    const float x = snap ? target
                         : tg::glide(state_.paddle.x, target, config::SIMULATION_STEP_SECONDS);
    state_.paddle.x = x < 0.0f ? 0.0f : (x > travel ? travel : x);
}

void Game::attachBall() {
    state_.ball.position = {state_.paddle.x + state_.paddle.w / 2.0f,
                            state_.paddle.y - state_.ball.radius - 1.0f};
    state_.ball.velocity = {0.0f, 0.0f};
}

std::uint8_t Game::update(float elapsed_seconds, float paddle_target) {
    if (state_.game_over) {
        return 0;
    }
    if (elapsed_seconds > config::MAX_FRAME_SECONDS) {
        elapsed_seconds = config::MAX_FRAME_SECONDS;
    }
    if (elapsed_seconds > 0.0f) {
        accumulator_ += elapsed_seconds;
    }

    std::uint8_t events = 0;
    // Small epsilon so accumulated float error doesn't drop a step.
    while (accumulator_ + 1e-6f >= config::SIMULATION_STEP_SECONDS) {
        accumulator_ -= config::SIMULATION_STEP_SECONDS;
        events |= step(paddle_target);
        if (state_.game_over) {
            break;
        }
    }
    if (accumulator_ < 0.0f || state_.game_over) {
        accumulator_ = 0.0f;
    }
    return events;
}

std::uint8_t Game::step(float paddle_target) {
    if (state_.game_over) {
        return 0;
    }
    setPaddle(paddle_target);
    if (!state_.launched) {
        attachBall();
        return 0;
    }

    const float dt = config::SIMULATION_STEP_SECONDS;
    std::uint8_t events = stepBall(state_.ball);
    for (ExtraBall& extra : state_.extra_balls) {
        if (extra.active) {
            events |= stepBall(extra.ball);
        }
    }

    events |= stepBolts(dt);
    events |= stepCapsules(dt);
    tickEffects(dt);

    for (ExtraBall& extra : state_.extra_balls) {
        if (extra.active && extra.ball.position.y - extra.ball.radius > SCREEN_HEIGHT) {
            extra.active = false;
        }
    }

    if (state_.bricks_remaining == 0) {
        if (state_.level < UINT32_MAX) ++state_.level;
        loadLevel();
        attachBall();
        return events | EVENT_LEVEL_CLEARED;
    }

    physics::Ball& ball = state_.ball;
    if (ball.position.y - ball.radius > SCREEN_HEIGHT) {
        // Promote a surviving extra ball instead of losing a life.
        for (ExtraBall& extra : state_.extra_balls) {
            if (extra.active) {
                ball = extra.ball;
                extra.active = false;
                return events;
            }
        }
        if (state_.lives > 0) {
            --state_.lives;
        }
        state_.launched = false;
        clearPowerUps();
        setPaddle(paddle_target);
        if (state_.lives == 0) {
            state_.game_over = true;
            return events | EVENT_GAME_OVER;
        }
        attachBall();
        return events | EVENT_LIFE_LOST;
    }
    return events;
}

const State& Game::state() const {
    return state_;
}

State& Game::mutableState() {
    return state_;
}

}  // namespace brickbreaker::game
