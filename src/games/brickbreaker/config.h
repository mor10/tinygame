#pragma once

#include <cstdint>

// Brick Breaker tuning. Change these to change how the game feels.
namespace brickbreaker::config {

// Initial gameplay values. Tune on physical hardware.
inline constexpr std::uint8_t STARTING_LIVES = 3;

inline constexpr std::uint8_t BRICK_ROWS = 5;
inline constexpr std::uint8_t BRICK_COLUMNS = 10;
inline constexpr float BRICK_HEIGHT = 8.0f;
inline constexpr float BRICK_GAP = 2.0f;
inline constexpr float BRICK_SIDE_MARGIN = 4.0f;
// Space between the HUD strip and the first brick row.
inline constexpr float BRICK_TOP_MARGIN = 8.0f;

// HUD strip at the top of the logical screen; the play field is below it.
inline constexpr float HUD_HEIGHT = 18.0f;

inline constexpr float PADDLE_WIDTH = 44.0f;
inline constexpr float PADDLE_HEIGHT = 6.0f;
// Gap between the paddle's bottom edge and the bottom of the screen.
inline constexpr float PADDLE_BOTTOM_MARGIN = 10.0f;
// Paddle bounce angle from vertical at the paddle's outermost edge.
inline constexpr float PADDLE_MAX_BOUNCE_DEGREES = 60.0f;

inline constexpr float BALL_RADIUS = 3.0f;
inline constexpr float BALL_INITIAL_SPEED = 120.0f;
inline constexpr float BALL_SPEED_PER_LEVEL = 15.0f;
inline constexpr float BALL_MAX_SPEED = 220.0f;
// Launch direction from vertical (positive = to the right).
inline constexpr float BALL_LAUNCH_DEGREES = 20.0f;

// Prevent nearly-horizontal trajectories.
inline constexpr float BALL_MIN_VERTICAL_SPEED = 35.0f;

inline constexpr std::uint16_t SCORE_PER_BRICK = 100;

// Fixed simulation step: 120 Hz.
inline constexpr float SIMULATION_STEP_SECONDS = 1.0f / 120.0f;
// Elapsed time is clamped after stalls to avoid large physics jumps.
inline constexpr float MAX_FRAME_SECONDS = 0.1f;

// Power-ups: a destroyed brick may drop a falling capsule.
inline constexpr std::uint8_t POWERUP_DROP_PERCENT = 18;
// Relative weights of the capsule types (sum = 100).
inline constexpr std::uint8_t POWERUP_WEIGHT_WIDE = 25;
inline constexpr std::uint8_t POWERUP_WEIGHT_LASER = 25;
inline constexpr std::uint8_t POWERUP_WEIGHT_MULTI = 20;
inline constexpr std::uint8_t POWERUP_WEIGHT_BIG = 15;
inline constexpr std::uint8_t POWERUP_WEIGHT_LIFE = 15;
static_assert(POWERUP_WEIGHT_WIDE + POWERUP_WEIGHT_LASER + POWERUP_WEIGHT_MULTI +
                      POWERUP_WEIGHT_BIG + POWERUP_WEIGHT_LIFE ==
                  100,
              "power-up weights must sum to 100");
// Multi-ball: extra balls split from the current ball at +/- this angle.
inline constexpr std::uint8_t EXTRA_BALL_COUNT = 2;
inline constexpr float MULTI_SPLIT_DEGREES = 25.0f;
inline constexpr std::uint8_t CAPSULE_POOL_SIZE = 3;
inline constexpr float CAPSULE_WIDTH = 14.0f;
inline constexpr float CAPSULE_HEIGHT = 7.0f;
inline constexpr float CAPSULE_FALL_SPEED = 55.0f;
// Effect durations (count down only while the ball is in play).
inline constexpr float WIDE_SECONDS = 12.0f;
inline constexpr float LASER_SECONDS = 12.0f;
inline constexpr float WIDE_PADDLE_WIDTH = 66.0f;
inline constexpr float BIG_SECONDS = 10.0f;
inline constexpr float BIG_BALL_RADIUS = 9.0f;
// Life capsules can raise the starting life count up to this cap.
inline constexpr std::uint8_t MAX_LIVES = 9;

// Laser bolts fired from both paddle edges.
inline constexpr std::uint8_t BOLT_POOL_SIZE = 6;
inline constexpr float BOLT_WIDTH = 2.0f;
inline constexpr float BOLT_HEIGHT = 6.0f;
inline constexpr float BOLT_SPEED = 260.0f;
inline constexpr float BOLT_COOLDOWN_SECONDS = 0.25f;
// Horizontal inset of each bolt from the paddle edge.
inline constexpr float BOLT_EDGE_INSET = 3.0f;

// Brick-break particle effect (visual only; not part of the simulation).
inline constexpr std::uint8_t PARTICLE_POOL_SIZE = 24;
inline constexpr std::uint8_t PARTICLES_PER_BURST = 8;
inline constexpr std::uint32_t PARTICLE_LIFETIME_MS = 400;
inline constexpr float PARTICLE_SPEED_SLOW = 55.0f;
inline constexpr float PARTICLE_SPEED_FAST = 95.0f;
inline constexpr float PARTICLE_GRAVITY = 260.0f;
inline constexpr std::int32_t PARTICLE_SIZE = 3;

}  // namespace brickbreaker::config
