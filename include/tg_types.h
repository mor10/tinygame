#pragma once

// Plain data types shared by the OS and games. No LVGL or Arduino here, so
// game logic that uses them can be unit-tested on a computer.

#include <cmath>
#include <cstdint>

namespace tg {

class Game;

struct GameInfo {
    const char* id;      // unique id matching [a-z][a-z0-9_]{0,14}
    const char* name;    // ASCII launcher name, up to 13 characters
    std::uint32_t color; // launcher highlight / LED colour
    Game* (*create)();
};

// Screen size in pixels (landscape). (0, 0) is the top-left corner.
inline constexpr int WIDTH = 320;
inline constexpr int HEIGHT = 172;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct EncoderInput {
    // Signed electrical transitions this update, positive = clockwise.
    // A nominal 20-pulse EC11 produces 80 transitions per revolution.
    std::int32_t delta = 0;
    bool down = false;
    bool pressed = false;
    bool released = false;
    bool click = false;
    std::uint32_t held_ms = 0;
};

// Everything a game can read from the player, refreshed on every update.
struct Input {
    // The player's main (turning) encoder: a copy of `right`, or of `left` when
    // System > Settings > HANDEDNESS is LEFT. Steer with this (see tg::Steer).
    EncoderInput main;
    // The physical encoders.
    EncoderInput left;
    EncoderInput right;

    // Action button: either encoder's click switch. `down` is true while held.
    // `pressed`/`released` are true for one update when it goes down/up. `click`
    // is true for one update after a short press. Long presses are reserved by
    // the OS (they open the pause menu), so games never see them.
    bool down = false;
    bool pressed = false;
    bool released = false;
    bool click = false;
    std::uint32_t held_ms = 0;

    // Motion sensor, in screen directions: +x = right, +y = down (toward the
    // bottom of the screen), +z = out of the screen toward the player.
    Vec3 accel_g;     // acceleration in g (1 g = gravity)
    Vec3 gyro_dps;    // rotation speed in degrees per second

    // Smoothed tilt, -1..1: the direction a marble on the screen would roll.
    // tilt_x > 0 rolls right, tilt_y > 0 rolls down the screen. 0, 0 = level.
    float tilt_x = 0.0f;
    float tilt_y = 0.0f;
};

// Standard encoder steering: slow turns move a little per transition so you
// can aim precisely, fast spins move further so you can still cross the
// screen quickly. Tracks a position from 0 to `travel` pixels.
struct SteerTuning {
    float slow_px = 2.5f;          // pixels per transition when turning slowly
    float fast_px = 8.0f;          // pixels per transition when spinning fast
    float slow_speed = 20.0f;      // transitions/s where the speed-up starts
    float fast_speed = 160.0f;     // transitions/s where it reaches fast_px
    float speed_smooth_s = 0.06f;  // how quickly the speed estimate reacts
};

class Steer {
public:
    explicit Steer(float travel = 0.0f, SteerTuning tuning = SteerTuning{})
        : travel_(travel), tuning_(tuning) {}

    void setTravel(float travel) {
        travel_ = travel;
        position_ = clamp(position_);
    }
    // Jumps to `position` (pixels) and forgets the turn speed. Call on new games.
    void reset(float position) {
        position_ = clamp(position);
        speed_ = 0.0f;
    }
    // Feed `in.main.delta` and `dt` on every update. Returns the new position.
    float update(std::int32_t transitions, float dt) {
        if (dt > 0.0f) {
            const float magnitude = static_cast<float>(transitions < 0 ? -transitions : transitions);
            float k = dt / tuning_.speed_smooth_s;
            k = k > 1.0f ? 1.0f : k;
            speed_ += (magnitude / dt - speed_) * k;
        }
        if (transitions == 0) return position_;
        float t = (speed_ - tuning_.slow_speed) / (tuning_.fast_speed - tuning_.slow_speed);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float px = tuning_.slow_px + (tuning_.fast_px - tuning_.slow_px) * t;
        position_ = clamp(position_ + static_cast<float>(transitions) * px);
        return position_;
    }
    float position() const { return position_; }
    // Position as 0 (left end) .. 1 (right end).
    float fraction() const { return travel_ > 0.0f ? position_ / travel_ : 0.0f; }

private:
    float clamp(float v) const { return v < 0.0f ? 0.0f : (v > travel_ ? travel_ : v); }
    float travel_;
    SteerTuning tuning_;
    float position_ = 0.0f;
    float speed_ = 0.0f;
};

// Standard glide time for things that follow a Steer position.
inline constexpr float GLIDE_SECONDS = 0.025f;

// Moves `current` smoothly toward `target` (exponential ease), hiding the
// per-transition steps of an encoder. Frame-rate independent. Call on every
// update (or every fixed physics step) and draw the result.
inline float glide(float current, float target, float dt, float ease_seconds = GLIDE_SECONDS) {
    if (ease_seconds <= 0.0f || dt <= 0.0f) return ease_seconds <= 0.0f ? target : current;
    const float next = current + (target - current) * (1.0f - std::exp(-dt / ease_seconds));
    return std::fabs(target - next) < 0.05f ? target : next;
}

// Turns encoder transitions into a choice from 0 to count-1 (menus, lanes,
// levels). One step per 4 transitions, like the OS menus; stops at the ends.
// Keep one Picker per choice in your class.
class Picker {
public:
    std::uint8_t update(std::uint8_t current, std::uint8_t count, std::int32_t transitions) {
        if (count == 0) return 0;
        if (current >= count) current = static_cast<std::uint8_t>(count - 1);
        remainder_ += transitions;
        while (remainder_ >= STEP && current + 1 < count) {
            ++current;
            remainder_ -= STEP;
        }
        while (remainder_ <= -STEP && current > 0) {
            --current;
            remainder_ += STEP;
        }
        if ((current + 1 == count && remainder_ > 0) || (current == 0 && remainder_ < 0)) {
            remainder_ = 0;
        }
        return current;
    }

private:
    static constexpr std::int32_t STEP = 4;
    std::int32_t remainder_ = 0;
};

}  // namespace tg
