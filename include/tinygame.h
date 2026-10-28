#pragma once

// TinyGame: everything a game needs. A game is a folder in src/games/ with a
// class like this (see .claude/skills/make-a-game for a full template):
//
//   #include "tinygame.h"
//
//   class Dodge : public tg::Game {
//   public:
//       void start(lv_obj_t* screen) override {
//           player_ = tg::ui::block(screen, 0, 140, 16, 16, tg::color::MINT);
//       }
//       void update(const tg::Input& in, float dt) override {
//           x_ = tg::glide(x_, steer_.update(in.main.delta, dt), dt);
//           lv_obj_set_x(player_, (int)x_);
//       }
//   private:
//       lv_obj_t* player_ = nullptr;
//       tg::Steer steer_{tg::WIDTH - 16};
//       float x_ = 0;
//   };
//
//   TINYGAME(Dodge, "dodge", "DODGE", tg::color::MINT)
//
// That's it. The game shows up in the launcher automatically.
//
// Rules of the console:
//   * Inputs are only the two encoders (turns + clicks) and the motion sensor.
//     Steer with in.main + tg::Steer + tg::glide; pick options with tg::Picker.
//   * A long press always opens the OS pause menu (Resume / Restart / Quit).
//     Your game never sees it, and update() is not called while paused.
//   * Restart and Quit delete your game object and its screen, and Restart
//     makes a fresh one. Keep all state in your class (no globals), and don't
//     delete LVGL objects in your destructor.

#include <lvgl.h>

#include <cstdint>

#include "tg_style.h"
#include "tg_types.h"

namespace tg {

class Game {
public:
    virtual ~Game() = default;
    // Build your LVGL objects on `screen` (320x172, already dark). Called once.
    virtual void start(lv_obj_t* screen) = 0;
    // Called each OS update. `dt` = elapsed seconds, not a guaranteed frame rate.
    virtual void update(const Input& in, float dt) = 0;
    // Optional: told when the pause menu opens (true) and closes (false).
    virtual void paused(bool /*is_paused*/) {}
};

// ---- Scores (saved in flash, per game) -------------------------------------
// Best score of the running game (0 if none yet). Shown in the launcher.
std::uint32_t bestScore();
// Returns true only when a new best is saved. Failures are logged.
bool submitScore(std::uint32_t score);

// ---- Status LED -----------------------------------------------------------------
// Sets the LED colour while your game runs (0xRRGGBB, 0 = off).
void led(std::uint32_t rgb);
// Briefly flashes the LED, e.g. flashLed(tg::color::RED, 300) when hit.
void flashLed(std::uint32_t rgb, std::uint32_t ms);

// ---- Debug ----------------------------------------------------------------------
// printf to the USB serial log, prefixed with your game id. Don't log every frame.
void log(const char* fmt, ...);

// ---- Registration (used by the TINYGAME macro) --------------------------------------
namespace detail {
struct Registrar {
    explicit Registrar(const GameInfo& info);
};
}  // namespace detail

}  // namespace tg

// One TINYGAME per game, in one of its .cpp files.
#define TINYGAME(Class, id, name, color)                                                \
    static_assert(sizeof(id) <= 16, "TINYGAME id must be at most 15 characters");      \
    static_assert(sizeof(name) <= 14, "TINYGAME name must be at most 13 characters");  \
    namespace {                                                                          \
    tg::Game* tinygame_create() { return new Class(); }                                 \
    const tg::detail::Registrar tinygame_registrar(tg::GameInfo{id, name, color, tinygame_create}); \
    }
