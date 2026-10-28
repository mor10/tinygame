#pragma once

#include <cstdint>

#include "tg_types.h"

// Rock Rain tuning. Change these to change how the game feels.
namespace rockrain::config {

inline constexpr std::uint8_t STARTING_LIVES = 3;
// Extra-life capsules can take you up to this many hearts.
inline constexpr std::uint8_t MAX_LIVES = 5;
// Shots you start with, to blast through the first wave.
inline constexpr std::uint8_t STARTING_AMMO = 5;

// HUD strip at the top; bricks appear just below it.
inline constexpr float HUD_HEIGHT = 14.0f;

// The robot is a 7x8 pixel-art sprite drawn with 2x2 "fat pixels".
inline constexpr float ROBOT_WIDTH = 14.0f;
inline constexpr float ROBOT_HEIGHT = 16.0f;
inline constexpr float ROBOT_Y = tg::HEIGHT - ROBOT_HEIGHT - 2.0f;
// Bricks must overlap the robot this much before they count as a hit.
inline constexpr float ROBOT_HITBOX_INSET = 2.0f;
// Seconds of safety (blinking) after getting hit.
inline constexpr float INVULNERABLE_SECONDS = 1.5f;

// Small bricks break in one shot, big bricks need two.
inline constexpr float SMALL_BRICK_WIDTH = 16.0f;
inline constexpr float SMALL_BRICK_HEIGHT = 8.0f;
inline constexpr float BIG_BRICK_WIDTH = 30.0f;
inline constexpr float BIG_BRICK_HEIGHT = 12.0f;
inline constexpr std::uint8_t BIG_BRICK_PERCENT = 30;
inline constexpr std::uint8_t BRICK_POOL_SIZE = 32;

// Getting harder: bricks fall faster and appear more often over time.
inline constexpr float FALL_SPEED_START = 45.0f;      // pixels per second
inline constexpr float FALL_SPEED_PER_SECOND = 1.5f;  // added every second
inline constexpr float FALL_SPEED_MAX = 140.0f;
// Each brick's speed is randomly 85%..115% of the current fall speed.
inline constexpr float FALL_SPEED_SPREAD = 0.15f;
inline constexpr float SPAWN_INTERVAL_START = 1.1f;   // seconds between bricks
inline constexpr float SPAWN_INTERVAL_PER_SECOND = 0.01f;
inline constexpr float SPAWN_INTERVAL_MIN = 0.3f;
inline constexpr float FIRST_BRICK_DELAY = 1.0f;

// Waves: rows of small bricks with 1 or 2 openings to slip through. One wave
// starts every game, and another follows each boss you beat. Random bricks
// and the boss wait while a wave is falling. Each unbroken stretch of a row is
// one long brick (fewer objects to draw); a shot knocks out one brick-sized
// piece and splits it.
inline constexpr std::uint8_t WAVE_ROWS = 6;
inline constexpr std::uint8_t ROW_SLOTS = static_cast<std::uint8_t>(tg::WIDTH / SMALL_BRICK_WIDTH);
static_assert(ROW_SLOTS * SMALL_BRICK_WIDTH == tg::WIDTH, "a row must fill the screen exactly");
inline constexpr std::uint8_t ONE_GAP_BRICKS = 3;   // opening width when a row has one gap
inline constexpr std::uint8_t TWO_GAP_BRICKS = 2;   // width of each opening when it has two
inline constexpr std::uint8_t TWO_GAP_PERCENT = 50;
inline constexpr float WAVE_FALL_SPEED = 55.0f;     // first wave
inline constexpr float WAVE_SPEED_STEP = 10.0f;     // faster for each later wave
inline constexpr float WAVE_FALL_SPEED_MAX = 110.0f;
inline constexpr float WAVE_ROW_SPACING = 85.0f;    // pixels between rows
inline constexpr float WAVE_FIRST_ROW_DELAY = 0.5f;
inline constexpr float WAVE_END_DELAY = 1.5f;       // calm gap before random bricks return

// Power-up capsules fall from the sky every few seconds.
inline constexpr std::uint8_t CAPSULE_POOL_SIZE = 3;
inline constexpr float CAPSULE_SIZE = 12.0f;
inline constexpr float CAPSULE_FALL_SPEED = 50.0f;
inline constexpr float FIRST_CAPSULE_DELAY = 4.0f;
inline constexpr float CAPSULE_INTERVAL_MIN = 6.0f;
inline constexpr float CAPSULE_INTERVAL_MAX = 10.0f;
// Capsule mix: LIFE is rare, then RAPID; the rest are AMMO.
inline constexpr std::uint8_t LIFE_PERCENT = 12;
// After this many capsules in a row without a LIFE, the next one is a LIFE.
inline constexpr std::uint8_t LIFE_GUARANTEE_AFTER = 6;
inline constexpr std::uint8_t RAPID_PERCENT = 32;
static_assert(LIFE_PERCENT + RAPID_PERCENT <= 100, "capsule chances add up to more than 100");
inline constexpr std::uint8_t AMMO_PER_CAPSULE = 5;
inline constexpr std::uint8_t MAX_AMMO = 20;
inline constexpr float RAPID_SECONDS = 5.0f;

// The boss brick turns up at random times, sinks to the middle of the
// screen and then slides back and forth, dropping small bricks, until you
// break it.
inline constexpr float BOSS_WIDTH = 32.0f;  // a square with a frowny face
inline constexpr float BOSS_HEIGHT = 32.0f;
inline constexpr std::uint8_t BOSS_HITS = 10;
inline constexpr float BOSS_ENTER_SPEED = 30.0f;  // sinking in from the top
inline constexpr float BOSS_HOVER_Y = 64.0f;      // top edge while hovering
inline constexpr float BOSS_HOVER_SPEED = 45.0f;   // sideways, pixels per second
inline constexpr float BOSS_DROP_INTERVAL = 0.7f;
inline constexpr float FIRST_BOSS_DELAY_MIN = 15.0f;
inline constexpr float FIRST_BOSS_DELAY_MAX = 25.0f;
inline constexpr float BOSS_INTERVAL_MIN = 20.0f;
inline constexpr float BOSS_INTERVAL_MAX = 40.0f;
// While the boss is out, AMMO and RAPID capsules come quicker (never LIFE).
inline constexpr float BOSS_FIRST_CAPSULE_DELAY = 1.0f;
inline constexpr float BOSS_CAPSULE_INTERVAL_MIN = 2.5f;
inline constexpr float BOSS_CAPSULE_INTERVAL_MAX = 4.0f;

// Bolts shoot straight up from the robot's cannon.
inline constexpr std::uint8_t BOLT_POOL_SIZE = 8;
inline constexpr float BOLT_WIDTH = 2.0f;
inline constexpr float BOLT_HEIGHT = 6.0f;
inline constexpr float BOLT_SPEED = 240.0f;
inline constexpr float AMMO_COOLDOWN_SECONDS = 0.15f;
inline constexpr float RAPID_COOLDOWN_SECONDS = 0.08f;

// Scoring.
inline constexpr std::uint16_t SCORE_PER_SECOND = 10;
inline constexpr std::uint16_t SCORE_SMALL_BRICK = 50;
inline constexpr std::uint16_t SCORE_BIG_BRICK = 150;
inline constexpr std::uint16_t SCORE_BOSS = 500;

// Fixed simulation step: 120 Hz. Long frames are clamped.
inline constexpr float STEP_SECONDS = 1.0f / 120.0f;
inline constexpr float MAX_FRAME_SECONDS = 0.1f;

// Brick-break particles (visual only).
inline constexpr std::uint8_t PARTICLE_POOL_SIZE = 16;
inline constexpr std::uint8_t PARTICLES_PER_BURST = 8;  // a boss uses the whole pool
inline constexpr std::uint32_t PARTICLE_LIFETIME_MS = 350;
inline constexpr float PARTICLE_SPEED = 80.0f;
inline constexpr float PARTICLE_GRAVITY = 260.0f;
inline constexpr std::int32_t PARTICLE_SIZE = 3;

}  // namespace rockrain::config
