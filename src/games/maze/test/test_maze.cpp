#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "model.h"

using namespace maze;

namespace {

void connectedAndRepeatable() {
    for (unsigned seed = 1; seed <= 1000; ++seed) {
        for (int start = 0; start < CELLS; ++start) {
            Model a;
            a.begin(Mode::Chill, seed, start);
            const auto& s = a.state();
            assert(s.start == start && s.goal != start);
            assert(s.laser_count == 0 && s.trap_count == 0 && s.gate_count == 0);
            bool seen[CELLS] = {};
            int queue[CELLS] = {start};
            int count = 1;
            seen[start] = true;
            for (int i = 0; i < count; ++i) {
                for (int next = 0; next < CELLS; ++next) {
                    if (!seen[next] && a.connected(queue[i], next)) {
                        seen[next] = true;
                        queue[count++] = next;
                    }
                }
            }
            assert(count == CELLS);
            assert(s.route_length > 0 && s.route_length <= 22);
            assert(s.protected_route[s.start] && s.protected_route[s.goal]);
            const Vec2 p = Model::center(start);
            assert(s.ball.x == p.x && s.ball.y == p.y);
            Model b;
            b.begin(Mode::Chill, seed, start);
            assert(b.state().goal == s.goal);
            for (int w = 0; w < WALLS; ++w) {
                assert(b.state().walls[w] == s.walls[w]);
            }
        }
    }
}

void passagesHaveBallClearance() {
    Model m;
    m.begin(Mode::Chill, 52);
    for (int cell = 0; cell < CELLS; ++cell) {
        const Vec2 p = Model::center(cell);
        for (int wall = 0; wall < WALLS; ++wall) {
            if (!m.state().walls[wall]) continue;
            const Rect r = Model::wallRect(wall);
            const float x = std::fmax(r.x, std::fmin(p.x, r.x + r.w));
            const float y = std::fmax(r.y, std::fmin(p.y, r.y + r.h));
            assert((p.x - x) * (p.x - x) + (p.y - y) * (p.y - y) >= RADIUS * RADIUS);
        }
    }
}

void thrillHazardsPreserveSafeRoutes() {
    for (unsigned seed = 1; seed <= 1000; ++seed) {
        for (unsigned completed : {0U, 4U, 16U, 1000000U}) {
            Model m;
            m.begin(Mode::Thrill, seed, seed % CELLS);
            auto& s = m.mutableState();
            s.completed = completed;
            s.ball = Model::center(s.goal);
            s.protection = 1;
            m.step(0, 0);
            for (int i = 0; i < 80 && s.stage == Stage::Transition; ++i) m.step(0, 0);
            assert(s.laser_count <= MAX_HAZARDS && s.trap_count <= MAX_HAZARDS);
            assert(s.gate_count > 0 && s.gate_count <= MAX_HAZARDS);
            assert(s.requested_hazards == (completed < 12 ? 1 + (completed + 1) / 4 : 4));
            bool trap[CELLS] = {};
            for (int i = 0; i < s.trap_count; ++i) {
                const int cell = s.traps[i];
                assert(!s.protected_route[cell]);
                assert(!trap[cell]);
                trap[cell] = true;
            }
            for (int origin = -1; origin < s.gate_count; ++origin) {
                const int start = origin < 0 ? s.start : s.gates[origin].to;
                bool seen[CELLS] = {};
                int queue[CELLS] = {start};
                int count = 1;
                seen[start] = true;
                for (int i = 0; i < count; ++i) {
                    const int cell = queue[i];
                    const int neighbors[] = {cell - 1, cell + 1, cell - COLS, cell + COLS};
                    for (int next : neighbors) {
                        if (next < 0 || next >= CELLS || trap[next] || seen[next]) continue;
                        bool passable = m.connected(cell, next);
                        for (int gate = 0; gate < s.gate_count; ++gate) {
                            passable |= s.gates[gate].from == cell && s.gates[gate].to == next;
                        }
                        if (passable) {
                            seen[next] = true;
                            queue[count++] = next;
                        }
                    }
                }
                assert(seen[s.goal]);
                for (int cell = 0; cell < CELLS; ++cell) {
                    assert(trap[cell] || seen[cell]);
                }
            }
            for (int i = 0; i < s.laser_count; ++i) {
                int a, b;
                Model::wallCells(s.lasers[i].wall, a, b);
                assert(m.connected(a, b));
                assert(a != s.start && b != s.start && a != s.goal && b != s.goal);
                assert(!trap[a] && !trap[b]);
                assert(CELL / 2.0f > RADIUS + 1);
            }
            assert(LASER_OFF >= CELL / 30.0f + 0.5f);
            assert(s.budget >= s.route_length * CELL / 30.0f +
                   s.laser_count * (LASER_WARNING + LASER_ACTIVE) + 20);
        }
    }
}

}  // namespace

int main() {
    connectedAndRepeatable();
    passagesHaveBallClearance();
    thrillHazardsPreserveSafeRoutes();
    std::puts("maze generation passed");
}
