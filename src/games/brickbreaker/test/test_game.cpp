#include <cassert>
#include <cmath>
#include <cstdio>

#include "tg_types.h"
#include "game.h"
#include "config.h"
#include "level.h"

using namespace brickbreaker;

namespace {

using config::BRICK_COLUMNS;
using config::BRICK_ROWS;

void clearAllBricks(game::State& state) {
    for (auto& row : state.bricks) {
        for (auto& brick : row) {
            brick = 0;
        }
    }
    state.bricks_remaining = 0;
}

void newGameStartsWithLivesAndAttachedBall() {
    game::Game g;
    g.newGame(0.5f);
    const game::State& s = g.state();
    assert(s.lives == config::STARTING_LIVES);
    assert(s.level == 1);
    assert(s.score == 0);
    assert(!s.launched);
    assert(s.bricks_remaining > 0);
    const float paddle_centre = s.paddle.x + s.paddle.w / 2.0f;
    assert(std::fabs(s.ball.position.x - paddle_centre) < 0.01f);
    assert(s.ball.position.y < s.paddle.y);
}

void stepMany(game::Game& g, float target, int steps = 60) {
    for (int i = 0; i < steps; ++i) g.step(target);
}

void paddleTracksTargetAndStaysInField() {
    game::Game g;
    g.newGame(0.0f);
    g.step(0.0f);
    assert(g.state().paddle.x == 0.0f);
    stepMany(g, 1.0f);
    assert(std::fabs(g.state().paddle.x + g.state().paddle.w -
                     tg::WIDTH) < 0.01f);
    stepMany(g, 1.7f);
    assert(g.state().paddle.x + g.state().paddle.w <=
           tg::WIDTH + 0.01f);
}

void paddleGlidesInsteadOfSnapping() {
    game::Game g;
    g.newGame(0.0f);
    assert(g.state().paddle.x == 0.0f);  // a new game snaps
    g.step(1.0f);
    const float first = g.state().paddle.x;
    assert(first > 0.0f && first < 276.0f * 0.5f);
    g.step(1.0f);
    assert(g.state().paddle.x > first);
}

void attachedBallFollowsPaddle() {
    game::Game g;
    g.newGame(0.0f);
    const float before = g.state().ball.position.x;
    stepMany(g, 1.0f);
    assert(g.state().ball.position.x > before + 100.0f);
}

void launchMovesBallUpward() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    assert(g.state().launched);
    const float y = g.state().ball.position.y;
    g.step(0.5f);
    assert(g.state().ball.position.y < y);
    assert(std::fabs(physics::speed(g.state().ball) -
                     config::BALL_INITIAL_SPEED) < 0.5f);
}

void updateUsesFixedStepsAndClampsStalls() {
    game::Game a;
    a.newGame(0.5f);
    a.launch();
    a.update(0.05f, 0.5f);
    game::Game b;
    b.newGame(0.5f);
    b.launch();
    for (int i = 0; i < 6; ++i) {
        b.step(0.5f);
    }
    assert(std::fabs(a.state().ball.position.y - b.state().ball.position.y) <
           0.001f);

    game::Game stalled;
    stalled.newGame(0.5f);
    stalled.launch();
    const float y = stalled.state().ball.position.y;
    stalled.update(5.0f, 0.5f);
    const float max_travel = config::BALL_INITIAL_SPEED *
                             (config::MAX_FRAME_SECONDS + 0.01f);
    assert(y - stalled.state().ball.position.y <= max_travel);
}

void ballLostBelowCostsLifeAndReattaches() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    game::State& s = g.mutableState();
    s.ball.position = {10.0f, tg::HEIGHT + 10.0f};
    s.ball.velocity = {0.0f, 100.0f};
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_LIFE_LOST);
    assert(g.state().lives == config::STARTING_LIVES - 1);
    assert(!g.state().launched);
    assert(!g.state().game_over);
}

void lastLifeLostEndsGame() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    game::State& s = g.mutableState();
    s.lives = 1;
    s.ball.position = {10.0f, tg::HEIGHT + 10.0f};
    s.ball.velocity = {0.0f, 100.0f};
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_GAME_OVER);
    assert(g.state().game_over);
    assert(g.state().lives == 0);
    const float y = g.state().ball.position.y;
    g.update(1.0f, 0.5f);
    assert(g.state().ball.position.y == y);
}

void hittingBrickScoresAndRemovesIt() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    clearAllBricks(g.mutableState());
    game::State& s = g.mutableState();
    s.bricks[0][0] = true;
    s.bricks[1][0] = true;
    s.bricks_remaining = 2;
    const physics::Rect brick = game::Game::brickRect(1, 0);
    s.ball.position = {brick.x + brick.w / 2.0f,
                       brick.y + brick.h + s.ball.radius - 0.5f};
    s.ball.velocity = {0.0f, -100.0f};
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_BRICK_HIT);
    assert(!g.state().bricks[1][0]);
    assert(g.state().bricks[0][0]);
    assert(g.state().bricks_remaining == 1);
    assert(g.state().score == config::SCORE_PER_BRICK);
    assert(g.state().ball.velocity.y > 0.0f);
}

void clearingLastBrickAdvancesLevelAndSpeedsUp() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    clearAllBricks(g.mutableState());
    game::State& s = g.mutableState();
    s.bricks[2][3] = true;
    s.bricks_remaining = 1;
    const physics::Rect brick = game::Game::brickRect(2, 3);
    s.ball.position = {brick.x + brick.w / 2.0f,
                       brick.y + brick.h + s.ball.radius - 0.5f};
    s.ball.velocity = {0.0f, -100.0f};
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_LEVEL_CLEARED);
    assert(g.state().level == 2);
    assert(!g.state().launched);
    assert(g.state().bricks_remaining > 0);
    assert(g.state().ball_speed > config::BALL_INITIAL_SPEED);
    assert(g.state().score == config::SCORE_PER_BRICK);
}

void bricksFitInsideScreen() {
    const physics::Rect last =
        game::Game::brickRect(BRICK_ROWS - 1, BRICK_COLUMNS - 1);
    assert(last.x + last.w <= tg::WIDTH);
    const physics::Rect first = game::Game::brickRect(0, 0);
    assert(first.y >= config::HUD_HEIGHT);
    assert(last.y + last.h < game::Game::playField().y +
                                 game::Game::playField().h - 40.0f);
}

void ballNeverEscapesTopOrSidesDuringLongRun() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    const physics::Rect field = game::Game::playField();
    for (int i = 0; i < 20000 && !g.state().game_over; ++i) {
        // Simple autopilot: follow the ball with the paddle.
        const float target = (g.state().ball.position.x - config::PADDLE_WIDTH / 2.0f) /
                          (tg::WIDTH - config::PADDLE_WIDTH);
        g.step(target);
        if (!g.state().launched) {
            g.launch();
        }
        const physics::Ball& b = g.state().ball;
        assert(b.position.x >= field.x + b.radius - 0.01f);
        assert(b.position.x <= field.x + field.w - b.radius + 0.01f);
        assert(b.position.y >= field.y + b.radius - 0.01f);
        if (g.state().launched) {
            assert(std::fabs(b.velocity.y) >=
                   config::BALL_MIN_VERTICAL_SPEED - 0.01f);
        }
    }
}

void placeSingleBrickBelowBall(game::Game& g, std::uint8_t row,
                               std::uint8_t col, std::uint8_t hits) {
    clearAllBricks(g.mutableState());
    game::State& s = g.mutableState();
    s.bricks[row][col] = hits;
    s.bricks_remaining = 1;
    const physics::Rect brick = game::Game::brickRect(row, col);
    s.ball.position = {brick.x + brick.w / 2.0f,
                       brick.y + brick.h + s.ball.radius - 0.5f};
    s.ball.velocity = {0.0f, -100.0f};
}

void toughBrickTakesTwoHits() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    placeSingleBrickBelowBall(g, 2, 3, 2);
    std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_BRICK_HIT);
    assert((events & game::EVENT_BRICK_BROKEN) == 0);
    assert(g.state().bricks[2][3] == 1);
    assert(g.state().bricks_remaining == 1);
    assert(g.state().score == config::SCORE_PER_BRICK);

    placeSingleBrickBelowBall(g, 2, 3, 1);
    events = g.step(0.5f);
    assert(events & game::EVENT_BRICK_BROKEN);
    assert(events & game::EVENT_LEVEL_CLEARED);
}

void brokenBrickIsReportedForEffects() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    placeSingleBrickBelowBall(g, 3, 6, 1);
    g.mutableState().bricks[0][0] = 1;
    g.mutableState().bricks_remaining = 2;
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_BRICK_BROKEN);
    assert(g.state().last_broken_row == 3);
    assert(g.state().last_broken_col == 6);
}

void sixLevelsAndLaterOnesHaveToughBricks() {
    assert(level::LEVEL_COUNT == 6);
    for (std::uint8_t index = 0; index < level::LEVEL_COUNT; ++index) {
        unsigned bricks = 0;
        unsigned tough = 0;
        for (std::uint8_t row = 0; row < BRICK_ROWS; ++row) {
            for (std::uint8_t col = 0; col < BRICK_COLUMNS; ++col) {
                const std::uint8_t hits = level::brickHits(index, row, col);
                assert(hits <= 2);
                bricks += hits > 0 ? 1 : 0;
                tough += hits == 2 ? 1 : 0;
            }
        }
        assert(bricks > 0);
        assert(index < 3 ? tough == 0 : tough > 0);
    }
    // Wraps after the last layout.
    assert(level::brickHits(level::LEVEL_COUNT, 0, 0) == level::brickHits(0, 0, 0));
}

void levelFourLoadsToughBricks() {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    g.mutableState().level = 3;
    placeSingleBrickBelowBall(g, 2, 3, 1);
    assert(g.step(0.5f) & game::EVENT_LEVEL_CLEARED);
    assert(g.state().level == 4);
    unsigned bricks = 0;
    unsigned tough = 0;
    for (const auto& row : g.state().bricks) {
        for (const std::uint8_t hits : row) {
            bricks += hits > 0 ? 1 : 0;
            tough += hits == 2 ? 1 : 0;
        }
    }
    assert(tough > 0);
    assert(g.state().bricks_remaining == bricks);
}

game::Game launchedGameWithBrickAt(std::uint8_t row, std::uint8_t col) {
    game::Game g;
    g.newGame(0.5f);
    g.launch();
    clearAllBricks(g.mutableState());
    // Keep a far-away brick so the level does not clear.
    g.mutableState().bricks[0][9] = 1;
    g.mutableState().bricks_remaining = 1;
    if (row != 0 || col != 9) {
        g.mutableState().bricks[row][col] = 1;
        g.mutableState().bricks_remaining = 2;
    }
    // Park the ball away from bricks, moving down slowly.
    g.mutableState().ball.position = {20.0f, 120.0f};
    g.mutableState().ball.velocity = {0.0f, 40.0f};
    return g;
}

void placeCapsuleOnPaddle(game::Game& g, game::PowerUp type) {
    game::State& s = g.mutableState();
    s.capsules[0].active = true;
    s.capsules[0].type = type;
    s.capsules[0].position = {s.paddle.x + s.paddle.w / 2.0f,
                              s.paddle.y - config::CAPSULE_HEIGHT / 2.0f};
}

void brokenBricksSometimesDropCapsules() {
    game::Options options;
    options.seed = 12345;
    game::Game g;
    g.newGame(0.5f, options);
    g.launch();
    unsigned drops = 0;
    const unsigned trials = 200;
    for (unsigned i = 0; i < trials; ++i) {
        for (auto& c : g.mutableState().capsules) {
            c.active = false;
        }
        placeSingleBrickBelowBall(g, 2, 3, 1);
        g.mutableState().bricks[0][0] = 1;
        g.mutableState().bricks_remaining = 2;
        const std::uint8_t events = g.step(0.5f);
        assert(events & game::EVENT_BRICK_BROKEN);
        for (const auto& c : g.state().capsules) {
            if (c.active) {
                ++drops;
                assert(c.type != game::PowerUp::None);
                const physics::Rect brick = game::Game::brickRect(2, 3);
                assert(std::fabs(c.position.x - (brick.x + brick.w / 2.0f)) < 0.01f);
            }
        }
    }
    // Roughly POWERUP_DROP_PERCENT; deterministic for a given seed.
    assert(drops > trials * config::POWERUP_DROP_PERCENT / 200);
    assert(drops < trials * config::POWERUP_DROP_PERCENT * 2 / 100);
}

void capsuleFallsAndIsLostBelow() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    game::State& s = g.mutableState();
    s.capsules[0].active = true;
    s.capsules[0].type = game::PowerUp::Wide;
    s.capsules[0].position = {5.0f, 60.0f};
    g.step(1.0f);  // paddle far right
    assert(g.state().capsules[0].position.y > 60.0f);
    g.mutableState().capsules[0].position.y = tg::HEIGHT + 20.0f;
    g.step(1.0f);
    assert(!g.state().capsules[0].active);
    assert(g.state().paddle.w == config::PADDLE_WIDTH);
}

void widePowerUpWidensPaddleTemporarily() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    placeCapsuleOnPaddle(g, game::PowerUp::Wide);
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_POWERUP_CAUGHT);
    assert(g.state().last_powerup == game::PowerUp::Wide);
    assert(!g.state().capsules[0].active);
    assert(g.state().paddle.w == config::WIDE_PADDLE_WIDTH);
    assert(g.state().wide_seconds_left > 0.0f);
    // Paddle stays inside the field at full travel.
    g.step(1.0f);
    assert(g.state().paddle.x + g.state().paddle.w <= tg::WIDTH + 0.01f);

    g.mutableState().wide_seconds_left = config::SIMULATION_STEP_SECONDS / 2.0f;
    g.mutableState().ball.position = {20.0f, 120.0f};
    g.step(0.5f);
    assert(g.state().wide_seconds_left == 0.0f);
    assert(g.state().paddle.w == config::PADDLE_WIDTH);
}

void lifePowerUpAddsLifeUpToCap() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    placeCapsuleOnPaddle(g, game::PowerUp::Life);
    g.step(0.5f);
    assert(g.state().lives == config::STARTING_LIVES + 1);
    g.mutableState().lives = config::MAX_LIVES;
    placeCapsuleOnPaddle(g, game::PowerUp::Life);
    g.step(0.5f);
    assert(g.state().lives == config::MAX_LIVES);
}

void laserFiresBoltsThatBreakBricks() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    // No laser yet: fire does nothing.
    assert(!g.fire());

    placeCapsuleOnPaddle(g, game::PowerUp::Laser);
    g.step(0.5f);
    assert(g.state().laser_seconds_left > 0.0f);
    assert(g.fire());
    unsigned active = 0;
    for (const auto& b : g.state().bolts) {
        active += b.active ? 1 : 0;
    }
    assert(active == 2);
    // Cooldown prevents immediate refire.
    assert(!g.fire());

    // Put a brick directly above the left bolt.
    float left_x = 1e9f;
    for (const auto& b : g.state().bolts) {
        if (b.active && b.position.x < left_x) {
            left_x = b.position.x;
        }
    }
    std::uint8_t target_col = 0;
    for (std::uint8_t col = 0; col < BRICK_COLUMNS; ++col) {
        const physics::Rect r = game::Game::brickRect(4, col);
        if (left_x >= r.x && left_x <= r.x + r.w) {
            target_col = col;
        }
    }
    g.mutableState().bricks[4][target_col] = 1;
    ++g.mutableState().bricks_remaining;
    const std::uint32_t score = g.state().score;
    std::uint8_t events = 0;
    for (int i = 0; i < 120; ++i) {
        g.mutableState().ball.position = {20.0f, 120.0f};
        g.mutableState().ball.velocity = {0.0f, 40.0f};
        events |= g.step(0.5f);
    }
    assert(events & game::EVENT_BRICK_BROKEN);
    assert(g.state().bricks[4][target_col] == 0);
    assert(g.state().score == score + config::SCORE_PER_BRICK);
    // Bolts leave the top of the field.
    for (const auto& b : g.state().bolts) {
        assert(!b.active);
    }
}

void losingLifeClearsPowerUps() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    placeCapsuleOnPaddle(g, game::PowerUp::Wide);
    g.step(0.5f);
    game::State& s = g.mutableState();
    s.laser_seconds_left = 5.0f;
    s.capsules[1].active = true;
    s.capsules[1].position = {50.0f, 50.0f};
    s.bolts[0].active = true;
    s.bolts[0].position = {50.0f, 100.0f};
    s.ball.position = {10.0f, tg::HEIGHT + 10.0f};
    s.ball.velocity = {0.0f, 100.0f};
    assert(g.step(0.5f) & game::EVENT_LIFE_LOST);
    assert(g.state().wide_seconds_left == 0.0f);
    assert(g.state().laser_seconds_left == 0.0f);
    assert(g.state().paddle.w == config::PADDLE_WIDTH);
    assert(!g.state().capsules[1].active);
    assert(!g.state().bolts[0].active);
}

void bigPowerUpGrowsBallTemporarily() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    placeCapsuleOnPaddle(g, game::PowerUp::Big);
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_POWERUP_CAUGHT);
    assert(g.state().last_powerup == game::PowerUp::Big);
    assert(g.state().ball.radius == config::BIG_BALL_RADIUS);
    assert(g.state().big_seconds_left > 0.0f);

    g.mutableState().big_seconds_left = config::SIMULATION_STEP_SECONDS / 2.0f;
    g.mutableState().ball.position = {20.0f, 120.0f};
    g.step(0.5f);
    assert(g.state().big_seconds_left == 0.0f);
    assert(g.state().ball.radius == config::BALL_RADIUS);
}

void losingLifeShrinksBigBall() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    placeCapsuleOnPaddle(g, game::PowerUp::Big);
    g.step(0.5f);
    game::State& s = g.mutableState();
    s.ball.position = {10.0f, tg::HEIGHT + 20.0f};
    s.ball.velocity = {0.0f, 100.0f};
    assert(g.step(0.5f) & game::EVENT_LIFE_LOST);
    assert(g.state().big_seconds_left == 0.0f);
    assert(g.state().ball.radius == config::BALL_RADIUS);
}

unsigned activeExtraBalls(const game::State& s) {
    unsigned n = 0;
    for (const auto& e : s.extra_balls) {
        n += e.active ? 1 : 0;
    }
    return n;
}

float speedOf(const physics::Ball& b) {
    return std::sqrt(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y);
}

void multiPowerUpSplitsBallIntoThree() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    g.mutableState().ball.velocity = {0.0f, -120.0f};
    placeCapsuleOnPaddle(g, game::PowerUp::Multi);
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_POWERUP_CAUGHT);
    assert(g.state().last_powerup == game::PowerUp::Multi);
    assert(activeExtraBalls(g.state()) == config::EXTRA_BALL_COUNT);
    const physics::Ball& main_ball = g.state().ball;
    for (const auto& e : g.state().extra_balls) {
        assert(std::fabs(speedOf(e.ball) - speedOf(main_ball)) < 1.0f);
        assert(std::fabs(e.ball.velocity.x - main_ball.velocity.x) > 10.0f);
        assert(e.ball.velocity.y < 0.0f);
    }
    assert(g.state().extra_balls[0].ball.velocity.x !=
           g.state().extra_balls[1].ball.velocity.x);
}

void extraBallBreaksBricks() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    game::State& s = g.mutableState();
    s.bricks[2][3] = 1;
    s.bricks_remaining = 2;
    const physics::Rect brick = game::Game::brickRect(2, 3);
    s.extra_balls[0].active = true;
    s.extra_balls[0].ball.radius = config::BALL_RADIUS;
    s.extra_balls[0].ball.position = {brick.x + brick.w / 2.0f,
                                      brick.y + brick.h + config::BALL_RADIUS - 0.5f};
    s.extra_balls[0].ball.velocity = {0.0f, -100.0f};
    const std::uint8_t events = g.step(0.5f);
    assert(events & game::EVENT_BRICK_BROKEN);
    assert(g.state().bricks[2][3] == 0);
    assert(g.state().extra_balls[0].ball.velocity.y > 0.0f);
}

void losingMainBallWithExtrasKeepsLife() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    game::State& s = g.mutableState();
    s.extra_balls[1].active = true;
    s.extra_balls[1].ball.radius = config::BALL_RADIUS;
    s.extra_balls[1].ball.position = {150.0f, 100.0f};
    s.extra_balls[1].ball.velocity = {30.0f, -90.0f};
    s.ball.position = {10.0f, tg::HEIGHT + 10.0f};
    s.ball.velocity = {0.0f, 100.0f};
    const std::uint8_t events = g.step(0.5f);
    assert((events & game::EVENT_LIFE_LOST) == 0);
    assert(g.state().lives == config::STARTING_LIVES);
    assert(g.state().launched);
    assert(activeExtraBalls(g.state()) == 0);
    // The surviving extra ball became the main ball.
    assert(std::fabs(g.state().ball.position.x - 150.0f) < 2.0f);
    assert(g.state().ball.velocity.y < 0.0f);
}

void losingExtraBallOnlyRemovesIt() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    game::State& s = g.mutableState();
    s.extra_balls[0].active = true;
    s.extra_balls[0].ball.radius = config::BALL_RADIUS;
    s.extra_balls[0].ball.position = {10.0f, tg::HEIGHT + 10.0f};
    s.extra_balls[0].ball.velocity = {0.0f, 100.0f};
    const std::uint8_t events = g.step(0.5f);
    assert((events & game::EVENT_LIFE_LOST) == 0);
    assert(activeExtraBalls(g.state()) == 0);
    assert(g.state().lives == config::STARTING_LIVES);
}

void lifeLostAndLevelClearRemoveExtraBalls() {
    game::Game g = launchedGameWithBrickAt(0, 9);
    g.mutableState().extra_balls[0].active = true;
    g.mutableState().extra_balls[0].ball.position = {20.0f, tg::HEIGHT + 10.0f};
    g.mutableState().extra_balls[0].ball.velocity = {0.0f, 100.0f};
    g.mutableState().ball.position = {10.0f, tg::HEIGHT + 10.0f};
    g.mutableState().ball.velocity = {0.0f, 100.0f};
    assert(g.step(0.5f) & game::EVENT_LIFE_LOST);
    assert(activeExtraBalls(g.state()) == 0);

    g.launch();
    g.mutableState().extra_balls[0].active = true;
    g.mutableState().extra_balls[0].ball.position = {150.0f, 100.0f};
    placeSingleBrickBelowBall(g, 2, 3, 1);
    assert(g.step(0.5f) & game::EVENT_LEVEL_CLEARED);
    assert(activeExtraBalls(g.state()) == 0);
}

}  // namespace

int main() {
    newGameStartsWithLivesAndAttachedBall();
    paddleTracksTargetAndStaysInField();
    paddleGlidesInsteadOfSnapping();
    attachedBallFollowsPaddle();
    launchMovesBallUpward();
    updateUsesFixedStepsAndClampsStalls();
    ballLostBelowCostsLifeAndReattaches();
    lastLifeLostEndsGame();
    hittingBrickScoresAndRemovesIt();
    clearingLastBrickAdvancesLevelAndSpeedsUp();
    bricksFitInsideScreen();
    toughBrickTakesTwoHits();
    brokenBrickIsReportedForEffects();
    sixLevelsAndLaterOnesHaveToughBricks();
    levelFourLoadsToughBricks();
    brokenBricksSometimesDropCapsules();
    capsuleFallsAndIsLostBelow();
    widePowerUpWidensPaddleTemporarily();
    lifePowerUpAddsLifeUpToCap();
    laserFiresBoltsThatBreakBricks();
    losingLifeClearsPowerUps();
    bigPowerUpGrowsBallTemporarily();
    losingLifeShrinksBigBall();
    multiPowerUpSplitsBallIntoThree();
    extraBallBreaksBricks();
    losingMainBallWithExtrasKeepsLife();
    losingExtraBallOnlyRemovesIt();
    lifeLostAndLevelClearRemoveExtraBalls();
    ballNeverEscapesTopOrSidesDuringLongRun();
    std::puts("game tests passed");
}
