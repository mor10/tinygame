// Rock Rain: bricks fall from the sky. Turn the main encoder to move your robot
// and dodge them (slow turns for precision, fast spins to cross the screen).
// Catch power-ups, then press either encoder to shoot bricks apart.

#include "tinygame.h"

#include "config.h"
#include "logic.h"
#include "view.h"

namespace rockrain {

class RockRain : public tg::Game {
public:
    void start(lv_obj_t* screen) override {
        best_ = tg::bestScore();
        robot_.reset(ROBOT_TRAVEL / 2.0f);
        game_.newGame(lv_tick_get(), robot_.fraction());
        view_.build(screen);
    }

    void update(const tg::Input& in, float dt) override {
        std::uint8_t events = 0;
        robot_.update(in.main.delta, dt);
        if (game_.state().game_over) {
            if (in.click) {
                game_.newGame(lv_tick_get(), robot_.fraction());
                new_best_ = false;
            }
        } else {
            // Shoot the moment a knob is pressed (no waiting for release).
            if (in.pressed) {
                game_.fire();
            }
            events = game_.update(dt, robot_.fraction());
            if (events & logic::EVENT_GAME_OVER) {
                new_best_ = tg::submitScore(game_.state().score);
                if (new_best_) {
                    best_ = game_.state().score;
                }
                tg::log("game over, score %lu", static_cast<unsigned long>(game_.state().score));
            }
        }
        updateLed(events);
        view_.update(game_.state(), events, dt, best_, new_best_);
    }

private:
    void updateLed(std::uint8_t events) {
        const logic::State& s = game_.state();
        tg::led(s.game_over                   ? tg::color::BAD
                : s.rapid_seconds_left > 0.0f ? tg::color::PINK
                : s.ammo > 0                  ? tg::color::SKY
                                              : tg::color::GOOD);
        if (events & (logic::EVENT_ROBOT_HIT | logic::EVENT_GAME_OVER)) {
            tg::flashLed(tg::color::BAD, 400);
        } else if (events & logic::EVENT_POWERUP) {
            tg::flashLed(powerUpColor(s.last_powerup), 300);
        } else if (events & logic::EVENT_BOSS_HIT) {
            tg::flashLed(tg::color::PLUM, 60);
        } else if (events & logic::EVENT_BRICK_BROKEN) {
            tg::flashLed(tg::color::WHITE, 60);
        }
    }

    logic::Game game_;
    static constexpr float ROBOT_TRAVEL = tg::WIDTH - config::ROBOT_WIDTH;
    tg::Steer robot_{ROBOT_TRAVEL};
    View view_;
    std::uint32_t best_ = 0;
    bool new_best_ = false;
};

}  // namespace rockrain

TINYGAME(rockrain::RockRain, "rockrain", "ROCK RAIN", tg::color::ORANGE)
