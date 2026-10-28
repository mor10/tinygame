#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "model.h"

using namespace maze;

namespace {

void enclosed(Model& m) {
    m.begin(Mode::Chill, 1, COLS + 1);
    auto& s = m.mutableState();
    std::fill(s.walls, s.walls + WALLS, true);
    s.goal = CELLS - 1;
}

void collisionAndSliding() {
    Model m;
    enclosed(m);
    auto& s = m.mutableState();
    const Vec2 p = Model::center(s.start);
    s.ball.x = p.x + 6.9f;
    s.velocity = {70, 20};
    m.step(0, 0);
    assert(s.ball.x <= p.x + 7.001f);
    assert(std::fabs(s.velocity.x) < 0.001f);
    assert(s.velocity.y > 0 && s.ball.y > p.y);
    for (int i = 0; i < 5000; ++i) m.step(1, 1);
    assert(s.ball.x <= p.x + 7.001f && s.ball.y <= p.y + 7.001f);
    assert(s.ball.x >= p.x - 7.001f && s.ball.y >= p.y - 7.001f);
}

void settlingAndFramePartitions() {
    Model a, b;
    enclosed(a);
    enclosed(b);
    tg::Input in;
    in.tilt_x = 0.7f;
    a.update(in, 0.05f);
    for (int i = 0; i < 6; ++i) b.step(in.tilt_x, 0);
    assert(std::fabs(a.state().ball.x - b.state().ball.x) < 0.001f);
    assert(std::fabs(a.state().velocity.x - b.state().velocity.x) < 0.001f);
    a.mutableState().velocity = {4, -3};
    in.tilt_x = 0.05f;
    in.tilt_y = -0.05f;
    for (int i = 0; i < 1200; ++i) a.step(in.tilt_x, in.tilt_y);
    assert(std::hypot(a.state().velocity.x, a.state().velocity.y) < 0.001f);
    const Vec2 before = b.state().ball;
    b.update(in, 10);
    assert(std::hypot(b.state().ball.x - before.x, b.state().ball.y - before.y)
           <= MAX_SPEED * MAX_FRAME + 0.01f);
}

void boundaryContainment() {
    Model m;
    m.begin(Mode::Chill, 2);
    auto& s = m.mutableState();
    s.goal = CELLS - 1;
    for (int wall = 0; wall < WALLS; ++wall) {
        const Rect r = Model::wallRect(wall);
        s.walls[wall] = r.x < LEFT || r.y < TOP ||
                       r.x > LEFT + (COLS - 1) * CELL ||
                       r.y > TOP + (ROWS - 1) * CELL;
    }
    for (int direction = 0; direction < 4; ++direction) {
        const float x = direction == 0 ? -1 : direction == 1 ? 1 : 0;
        const float y = direction == 2 ? -1 : direction == 3 ? 1 : 0;
        for (int i = 0; i < 1000; ++i) m.step(x, y);
        assert(s.ball.x >= LEFT + RADIUS + 1 - 0.01f);
        assert(s.ball.y >= TOP + RADIUS + 1 - 0.01f);
        assert(s.ball.x <= LEFT + COLS * CELL - RADIUS - 1 + 0.01f);
        assert(s.ball.y <= TOP + ROWS * CELL - RADIUS - 1 + 0.01f);
    }
}

void hazardsResetWithoutResettingClock() {
    Model m;
    m.begin(Mode::Thrill, 40);
    auto& s = m.mutableState();
    assert(s.laser_count > 0 && s.trap_count > 0);
    s.gate_count = 0;
    s.laser_count = 0;
    s.trap_count = 1;
    s.traps[0] = COLS + 1;
    s.ball = Model::center(s.traps[0]);
    s.velocity = {10, 0};
    const float time = s.remaining;
    assert(m.step(0, 0) & RESET);
    const Vec2 start = Model::center(s.start);
    assert(s.ball.x == start.x && s.ball.y == start.y);
    assert(s.velocity.x == 0 && s.velocity.y == 0);
    assert(std::fabs(s.remaining - (time - STEP)) < 0.0001f);
    assert(s.protection > 0);
    s.traps[0] = s.start;
    assert(!(m.step(0, 0) & RESET));
    s.protection = 0;
    assert(m.step(0, 0) & RESET);
}

void lasersCycleAndCollide() {
    Model m;
    m.begin(Mode::Thrill, 40);
    auto& s = m.mutableState();
    s.trap_count = s.gate_count = 0;
    s.laser_count = 1;
    s.lasers[0] = {Model::edge(COLS + 1, COLS + 2), 0};
    s.walls[s.lasers[0].wall] = false;
    const Vec2 a = Model::center(COLS + 1);
    const Vec2 b = Model::center(COLS + 2);
    for (float clock : {0.0f, 3.1f, 3.8f}) {
        s.clock = clock;
        s.ball = {(a.x + b.x) / 2, a.y};
        s.velocity = {MAX_SPEED, 0};
        s.protection = 0;
        const unsigned event = m.step(0, 0);
        assert(static_cast<bool>(event & RESET) == (clock > 3.75f));
    }
    // A hazard at the goal must reset rather than complete the maze.
    s.laser_count = 0;
    s.trap_count = 1;
    s.traps[0] = s.goal;
    s.ball = Model::center(s.goal);
    s.velocity = {};
    s.protection = 0;
    const unsigned event = m.step(0, 0);
    assert(event & RESET);
    assert(!(event & COMPLETED));
}

void oneWayGateCrossings() {
    const int from = COLS + 5;
    const int destinations[] = {from - 1, from + 1, from - COLS, from + COLS};
    for (int to : destinations) {
        for (bool reverse : {false, true}) {
            Model m;
            enclosed(m);
            auto& s = m.mutableState();
            s.gate_count = 1;
            s.gates[0] = {Model::edge(from, to), from, to};
            s.start = from;
            s.ball = Model::center(reverse ? to : from);
            const Vec2 a = Model::center(from);
            const Vec2 b = Model::center(to);
            const float x = (b.x - a.x) / CELL * (reverse ? -1 : 1);
            const float y = (b.y - a.y) / CELL * (reverse ? -1 : 1);
            for (int i = 0; i < 100; ++i) m.step(x, y);
            const Vec2 destination = Model::center(to);
            // Forward travel reaches this cell; reverse entry stays in it.
            assert(std::fabs(s.ball.x - destination.x) <= 7.01f);
            assert(std::fabs(s.ball.y - destination.y) <= 7.01f);
            assert(s.admitted_gate == -1);
        }
        // Approaching at maximum speed must complete passage without pinning.
        Model m;
        enclosed(m);
        auto& s = m.mutableState();
        s.gate_count = 1;
        s.gates[0] = {Model::edge(from, to), from, to};
        const Vec2 a = Model::center(from);
        const Vec2 b = Model::center(to);
        const Vec2 direction{(b.x - a.x) / CELL, (b.y - a.y) / CELL};
        s.ball = {a.x + direction.x * 6, a.y + direction.y * 6};
        s.velocity = {direction.x * MAX_SPEED, direction.y * MAX_SPEED};
        for (int i = 0; i < 40; ++i) m.step(direction.x, direction.y);
        assert((s.ball.x - a.x) * direction.x + (s.ball.y - a.y) * direction.y > CELL);
        assert(s.admitted_gate == -1);
    }
}

void laserPhaseBoundaries() {
    const Laser laser{0, 0};
    assert(Model::laserPhase(laser, LASER_OFF - 0.001f) == LaserPhase::Off);
    assert(Model::laserPhase(laser, LASER_OFF) == LaserPhase::Warning);
    assert(Model::laserPhase(laser, LASER_OFF + LASER_WARNING - 0.001f)
           == LaserPhase::Warning);
    assert(Model::laserPhase(laser, LASER_OFF + LASER_WARNING) == LaserPhase::Active);
    assert(Model::laserPhase(laser, LASER_OFF + LASER_WARNING + LASER_ACTIVE)
           == LaserPhase::Off);
}

void partialGateCanRetreatWithoutPinning() {
    Model m;
    enclosed(m);
    auto& s = m.mutableState();
    const int from = COLS + 5;
    const int to = from + 1;
    s.gate_count = 1;
    s.gates[0] = {Model::edge(from, to), from, to};
    const Vec2 a = Model::center(from);
    s.ball = {a.x + 6, a.y};
    s.velocity = {MAX_SPEED, 0};
    for (int i = 0; i < 12; ++i) m.step(1, 0);
    assert(s.admitted_gate == 0);
    s.velocity = {-MAX_SPEED, 0};
    for (int i = 0; i < 24; ++i) m.step(-1, 0);
    assert(s.ball.x < a.x + 7 && s.admitted_gate == -1);
    for (int i = 0; i < 140; ++i) m.step(1, 0);
    assert(s.ball.x > Model::center(to).x && s.admitted_gate == -1);
}

void randomRollingNeverPenetratesWalls() {
    unsigned random = 73;
    for (unsigned seed = 1; seed <= 50; ++seed) {
        Model m;
        m.begin(Mode::Chill, seed);
        for (int tick = 0; tick < 1000; ++tick) {
            random = random * 1664525U + 1013904223U;
            const float x = static_cast<int>(random % 201) / 100.0f - 1;
            random = random * 1664525U + 1013904223U;
            const float y = static_cast<int>(random % 201) / 100.0f - 1;
            m.step(x, y);
            const auto& s = m.state();
            for (int wall = 0; wall < WALLS; ++wall) {
                if (!s.walls[wall]) continue;
                const Rect r = Model::wallRect(wall);
                const float nx = std::clamp(s.ball.x, r.x, r.x + r.w);
                const float ny = std::clamp(s.ball.y, r.y, r.y + r.h);
                assert(std::hypot(s.ball.x - nx, s.ball.y - ny) >= RADIUS - 0.001f);
            }
        }
    }
}

}  // namespace

int main() {
    collisionAndSliding();
    settlingAndFramePartitions();
    boundaryContainment();
    hazardsResetWithoutResettingClock();
    lasersCycleAndCollide();
    oneWayGateCrossings();
    laserPhaseBoundaries();
    partialGateCanRetreatWithoutPinning();
    randomRollingNeverPenetratesWalls();
    std::puts("maze physics passed");
}
