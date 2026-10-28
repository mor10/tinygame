#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "model.h"

using namespace maze;

namespace {

void pickerAndIgnoredInputs() {
    Model m;
    assert(m.state().stage == Stage::Picker && m.state().mode == Mode::Chill);
    tg::Input in;
    in.main.delta = 3;
    m.update(in, 1);
    assert(m.state().mode == Mode::Chill);
    in.main.delta = 1;
    m.update(in, 1);
    assert(m.state().mode == Mode::Thrill);
    in.main.delta = 0;
    in.click = true;
    const unsigned started = m.update(in, 0);
    assert((started & STARTED) && (started & GENERATED));
    assert(m.state().stage == Stage::Playing);
    const auto revision = m.state().revision;
    const Vec2 before = m.state().ball;
    in.main.delta = -40;
    in.pressed = in.down = in.released = true;
    m.update(in, 0.1f);
    assert(m.state().mode == Mode::Thrill && m.state().revision == revision);
    assert(m.state().ball.x == before.x && m.state().ball.y == before.y);
}

void continuousMazesAndScoring() {
    for (Mode mode : {Mode::Chill, Mode::Thrill}) {
        Model m;
        m.begin(mode, 10);
        for (unsigned i = 0; i < 100; ++i) {
            auto& s = m.mutableState();
            const int hole = s.goal;
            const Vec2 position = Model::center(hole);
            s.ball = position;
            s.velocity = {};
            s.protection = 1;
            const unsigned completed = m.step(0, 0);
            assert(completed & COMPLETED);
            assert(static_cast<bool>(completed & SCORED) == (mode == Mode::Thrill));
            assert(s.completed == i + 1 && s.stage == Stage::Transition);
            assert(!(m.step(0, 0) & COMPLETED));
            for (int tick = 0; tick < 80 && s.stage == Stage::Transition; ++tick) {
                m.step(1, 1);
            }
            assert(s.stage == Stage::Playing && s.start == hole && s.goal != hole);
            assert(s.ball.x == position.x && s.ball.y == position.y);
            assert(s.velocity.x == 0 && s.velocity.y == 0);
            if (mode == Mode::Chill) assert(s.remaining == 0);
            else assert(s.remaining == s.budget);
        }
    }
}

void timeoutWinsAndFreezes() {
    Model m;
    m.begin(Mode::Thrill, 100);
    auto& s = m.mutableState();
    s.remaining = STEP / 2;
    s.ball = Model::center(s.goal);
    const unsigned events = m.step(0, 0);
    assert(events & TIMED_OUT);
    assert(!(events & COMPLETED) && !(events & SCORED));
    assert(s.stage == Stage::Over && s.remaining == 0);
    const Vec2 ball = s.ball;
    const float clock = s.clock;
    tg::Input in;
    in.click = true;
    in.tilt_x = 1;
    assert(m.update(in, 1) == 0);
    assert(s.ball.x == ball.x && s.clock == clock);
    Model fresh;
    assert(fresh.state().stage == Stage::Picker && fresh.state().completed == 0);
}

void pausedAndTransitionTime() {
    Model m;
    m.begin(Mode::Thrill, 30);
    auto& s = m.mutableState();
    const float clock = s.clock;
    const float time = s.remaining;
    tg::Input in;
    assert(m.update(in, 0) == 0);
    assert(s.clock == clock && s.remaining == time);
    s.ball = Model::center(s.goal);
    s.protection = 1;
    m.step(0, 0);
    const float after = s.remaining;
    for (int i = 0; i < 20; ++i) m.step(0, 0);
    assert(s.clock == STEP && s.remaining == after);
}

void stalledTransitionDoesNotMoveNewBall() {
    Model m;
    m.begin(Mode::Chill, 70);
    auto& s = m.mutableState();
    s.ball = Model::center(s.goal);
    m.step(0, 0);
    s.transition = STEP;
    const Vec2 old_goal = Model::center(s.goal);
    tg::Input in;
    in.tilt_x = in.tilt_y = 1;
    assert(m.update(in, 5) & GENERATED);
    assert(s.ball.x == old_goal.x && s.ball.y == old_goal.y);
    assert(s.velocity.x == 0 && s.velocity.y == 0);
}

void scoreDoesNotOverflow() {
    Model m;
    m.begin(Mode::Thrill, 90);
    auto& s = m.mutableState();
    s.completed = std::numeric_limits<std::uint32_t>::max();
    s.ball = Model::center(s.goal);
    s.protection = 1;
    assert(m.step(0, 0) & SCORED);
    assert(s.completed == std::numeric_limits<std::uint32_t>::max());
    for (int i = 0; i < 80 && s.stage == Stage::Transition; ++i) m.step(0, 0);
    assert(s.requested_hazards == MAX_HAZARDS);
    assert(s.route_length <= 52 && s.remaining == s.budget);
}

}  // namespace

int main() {
    pickerAndIgnoredInputs();
    continuousMazesAndScoring();
    timeoutWinsAndFreezes();
    pausedAndTransitionTime();
    stalledTransitionDoesNotMoveNewBall();
    scoreDoesNotOverflow();
    std::puts("maze game passed");
}
