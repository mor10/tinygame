// __NAME__: catch the falling stars with your paddle.
//   Turn the main knob: move.  Tilt: the wind pushes the stars.
//   Click either knob: start again.
// Starter game from the TinyGame template. Change anything!

#include "tinygame.h"

#include "tg_random.h"

namespace __ID__ {

// Tuning: change these numbers to change how the game feels.
constexpr int PADDLE_W = 48;
constexpr int PADDLE_H = 8;
constexpr int PADDLE_Y = tg::HEIGHT - 20;
constexpr float PADDLE_TRAVEL = tg::WIDTH - PADDLE_W;
constexpr int STAR = 8;
constexpr float FALL_SPEED = 60.0f;   // pixels per second at the start
constexpr float SPEED_UP = 4.0f;      // extra speed per star caught
constexpr float WIND = 80.0f;         // sideways push at full tilt
constexpr int LIVES = 3;

class __CLASS__ : public tg::Game {
public:
    // Build everything once. The screen is already dark.
    void start(lv_obj_t* screen) override {
        seed_ = lv_tick_get();
        best_ = tg::bestScore();
        score_label_ = tg::ui::label(screen, "", tg::Font::S);
        lv_obj_set_pos(score_label_, tg::GAP_M, tg::GAP_M);
        star_ = tg::ui::block(screen, 0, 0, STAR, STAR, tg::color::YELLOW);
        paddle_ = tg::ui::block(screen, 0, PADDLE_Y, PADDLE_W, PADDLE_H, tg::color::MINT);
        game_over_ = tg::ui::popup(screen, "GAME OVER", "CLICK: PLAY AGAIN");
        newGame();
    }

    // Movement uses elapsed seconds, not a fixed number of updates.
    void update(const tg::Input& in, float dt) override {
        if (over_) {
            if (in.click) {
                newGame();
            }
            return;
        }

        // The main knob steers the paddle: slow turns move it a little,
        // fast spins cross the screen. glide() makes the motion smooth.
        const float target = steer_.update(in.main.delta, dt);
        paddle_x_ = tg::glide(paddle_x_, target, dt);
        lv_obj_set_x(paddle_, static_cast<int>(paddle_x_));

        // The star falls; tilting the console blows it sideways.
        star_y_ += speed_ * dt;
        star_x_ += in.tilt_x * WIND * dt;
        if (star_x_ < 0) star_x_ = 0;
        if (star_x_ > tg::WIDTH - STAR) star_x_ = tg::WIDTH - STAR;

        // Caught it?
        if (star_y_ + STAR >= PADDLE_Y && star_y_ < PADDLE_Y + PADDLE_H &&
            star_x_ + STAR > paddle_x_ && star_x_ < paddle_x_ + PADDLE_W) {
            score_ += 1;
            speed_ += SPEED_UP;
            tg::flashLed(tg::color::YELLOW, 80);
            dropStar();
        } else if (star_y_ > tg::HEIGHT) {  // missed it
            lives_ -= 1;
            tg::flashLed(tg::color::BAD, 300);
            if (lives_ == 0) {
                endGame();
                return;
            }
            dropStar();
        }

        lv_obj_set_pos(star_, static_cast<int>(star_x_), static_cast<int>(star_y_));
        refreshHud();
    }

private:
    void newGame() {
        score_ = 0;
        lives_ = LIVES;
        speed_ = FALL_SPEED;
        over_ = false;
        steer_.reset(PADDLE_TRAVEL / 2);
        paddle_x_ = steer_.position();
        lv_obj_set_hidden(game_over_, true);
        tg::led(tg::color::MINT);
        dropStar();
        refreshHud();
        lv_obj_set_pos(star_, static_cast<int>(star_x_), static_cast<int>(star_y_));
        lv_obj_set_x(paddle_, static_cast<int>(paddle_x_));
    }

    void dropStar() {
        star_x_ = static_cast<float>(tg::randomNext(seed_) % (tg::WIDTH - STAR + 1));
        star_y_ = 24;
    }

    void endGame() {
        over_ = true;
        const bool best = tg::submitScore(score_);
        if (best) best_ = score_;
        refreshHud();
        tg::ui::result(game_over_, score_, best_, best);
        lv_obj_set_hidden(game_over_, false);
        tg::led(tg::color::BAD);
    }

    void refreshHud() {
        if (score_ != shown_score_ || lives_ != shown_lives_) {
            tg::ui::text(score_label_, "SCORE %d   LIVES %d", score_, lives_);
            shown_score_ = score_;
            shown_lives_ = lives_;
        }
    }

    // All game state lives here (no global variables).
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* star_ = nullptr;
    lv_obj_t* paddle_ = nullptr;
    lv_obj_t* game_over_ = nullptr;
    tg::Steer steer_{PADDLE_TRAVEL};
    float paddle_x_ = 0;
    float star_x_ = 0;
    float star_y_ = 0;
    float speed_ = FALL_SPEED;
    int score_ = 0;
    int lives_ = LIVES;
    bool over_ = false;
    std::uint32_t seed_ = 1;
    std::uint32_t best_ = 0;
    int shown_score_ = -1;
    int shown_lives_ = -1;
};

}  // namespace __ID__

// Adds the game to the launcher: class, id, name (max 13 letters), colour.
TINYGAME(__ID__::__CLASS__, "__ID__", "__NAME__", tg::color::MINT)
