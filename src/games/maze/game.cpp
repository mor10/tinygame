#include "tinygame.h"

#include "model.h"
#include "view.h"

namespace maze {

class Maze : public tg::Game {
public:
    void start(lv_obj_t* screen) override {
        seed_ = lv_tick_get();
        model_ = Model(seed_);
        view_.build(screen);
        view_.render(model_.state());
    }

    void update(const tg::Input& input, float dt) override {
        const unsigned events = model_.update(input, dt);
        if (events & STARTED) {
            tg::log("started %s, seed %lu",
                    model_.state().mode == Mode::Chill ? "chill" : "thrill",
                    static_cast<unsigned long>(seed_));
        }
        const State& s = model_.state();
        if (events & GENERATED) {
            tg::log("maze %lu: start %d, goal %d, route %d, budget %.1f",
                    static_cast<unsigned long>(s.revision), s.start, s.goal,
                    s.route_length, static_cast<double>(s.budget));
            if (s.mode == Mode::Thrill &&
                (s.laser_count < s.requested_hazards || s.trap_count < s.requested_hazards ||
                 s.gate_count < s.requested_hazards)) {
                tg::log("limited placement: requested %d each; lasers %d, traps %d, gates %d",
                        s.requested_hazards, s.laser_count, s.trap_count, s.gate_count);
            }
        }
        if (events & COMPLETED) {
            if (events & SCORED) tg::submitScore(s.completed);
            tg::log("completed %lu", static_cast<unsigned long>(s.completed));
        }
        if (events & RESET) tg::log("hazard reset, time %.1f", static_cast<double>(s.remaining));
        if (events & TIMED_OUT) {
            tg::log("time up, completed %lu", static_cast<unsigned long>(s.completed));
        }
        if (s.stage != led_stage_ || s.mode != led_mode_) {
            tg::led(s.stage == Stage::Over ? tg::color::BAD
                    : s.mode == Mode::Thrill ? tg::color::WARN : tg::color::MINT);
            led_stage_ = s.stage;
            led_mode_ = s.mode;
        }
        if (events & (RESET | TIMED_OUT)) tg::flashLed(tg::color::BAD, 350);
        else if (events & COMPLETED) tg::flashLed(tg::color::GOOD, 450);
        view_.render(model_.state());
    }

private:
    Model model_;
    View view_;
    std::uint32_t seed_ = 1;
    Stage led_stage_ = Stage::Over;
    Mode led_mode_ = Mode::Chill;
};

}  // namespace maze

TINYGAME(maze::Maze, "maze", "MAZE", tg::color::MINT)
