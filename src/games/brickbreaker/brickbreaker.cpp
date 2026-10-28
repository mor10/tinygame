// Brick Breaker: turn the main encoder to move the paddle (slow turns for
// precision, fast spins to cross the screen), click either encoder to launch
// the ball (and to fire lasers when you have them). Break every brick to level up.

#include "tinygame.h"

#include "config.h"
#include "game.h"
#include "view.h"

namespace brickbreaker {

class BrickBreaker : public tg::Game {
public:
    void start(lv_obj_t* screen) override {
        best_ = tg::bestScore();
        paddle_.reset(PADDLE_TRAVEL / 2.0f);
        game_.newGame(paddle_.fraction());
        view_.build(screen);
    }

    void update(const tg::Input& in, float dt) override {
        std::uint8_t events = 0;
        paddle_.update(in.main.delta, dt);
        const float paddle = paddle_.fraction();
        if (game_.state().game_over) {
            if (in.click) {
                game_.newGame(paddle);
                new_best_ = false;
            }
        } else {
            if (in.click) {
                if (!game_.state().launched) {
                    game_.launch();
                } else {
                    game_.fire();
                }
            }
            events |= game_.update(dt, paddle);
            if (events & game::EVENT_GAME_OVER) {
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
        const game::State& s = game_.state();
        tg::led(s.laser_seconds_left > 0.0f  ? tg::color::PINK
                : s.wide_seconds_left > 0.0f ? tg::color::SKY
                : s.big_seconds_left > 0.0f  ? tg::color::YELLOW
                : s.game_over                ? tg::color::BAD
                                             : tg::color::GOOD);
        if (events & (game::EVENT_LIFE_LOST | game::EVENT_GAME_OVER)) {
            tg::flashLed(tg::color::BAD, 400);
        } else if (events & game::EVENT_LEVEL_CLEARED) {
            tg::flashLed(tg::color::GOOD, 600);
        } else if (events & game::EVENT_POWERUP_CAUGHT) {
            tg::flashLed(powerUpColor(s.last_powerup), 300);
        } else if (events & game::EVENT_BRICK_BROKEN) {
            tg::flashLed(tg::color::WHITE, 60);
        }
    }

    game::Game game_;
    static constexpr float PADDLE_TRAVEL = tg::WIDTH - config::PADDLE_WIDTH;
    tg::Steer paddle_{PADDLE_TRAVEL};
    View view_;
    std::uint32_t best_ = 0;
    bool new_best_ = false;
};

}  // namespace brickbreaker

TINYGAME(brickbreaker::BrickBreaker, "brickbreaker", "BRICK BREAKER", tg::color::ORANGE)
