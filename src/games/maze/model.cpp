#include "model.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "tg_random.h"

namespace maze {

namespace {

float steering(float tilt) {
    return std::fabs(tilt) < 0.06f ? 0.0f : std::clamp(tilt, -1.0f, 1.0f);
}

void collide(Vec2& position, Vec2& velocity, const Rect& r) {
    const Vec2 nearest{std::clamp(position.x, r.x, r.x + r.w),
                       std::clamp(position.y, r.y, r.y + r.h)};
    float x = position.x - nearest.x;
    float y = position.y - nearest.y;
    const float distance2 = x * x + y * y;
    if (distance2 >= RADIUS * RADIUS) return;
    float distance = std::sqrt(distance2);
    if (distance > 0.0001f) {
        x /= distance;
        y /= distance;
    } else {
        // A thin wall may contain the center after a forced test placement.
        if (r.w < r.h) {
            x = position.x < r.x + r.w / 2 ? -1.0f : 1.0f;
            y = 0;
            distance = -std::min(position.x - r.x, r.x + r.w - position.x);
        } else {
            x = 0;
            y = position.y < r.y + r.h / 2 ? -1.0f : 1.0f;
            distance = -std::min(position.y - r.y, r.y + r.h - position.y);
        }
    }
    position.x += x * (RADIUS - distance);
    position.y += y * (RADIUS - distance);
    const float inward = velocity.x * x + velocity.y * y;
    if (inward < 0) {
        velocity.x -= inward * x;
        velocity.y -= inward * y;
    }
}

}  // namespace

std::uint32_t Model::random() {
    return tg::randomNext(rng_);
}

Vec2 Model::center(int cell) {
    return {static_cast<float>(LEFT + (cell % COLS) * CELL + CELL / 2),
            static_cast<float>(TOP + (cell / COLS) * CELL + CELL / 2)};
}

int Model::edge(int from, int to) {
    if (from < 0 || to < 0 || from >= CELLS || to >= CELLS) return -1;
    if (from / COLS == to / COLS && std::abs(from - to) == 1) {
        return HORIZONTAL_WALLS + (from / COLS) * (COLS + 1) +
               std::max(from, to) % COLS;
    }
    if (std::abs(from - to) == COLS) {
        return (std::max(from, to) / COLS) * COLS + from % COLS;
    }
    return -1;
}

Rect Model::wallRect(int wall) {
    if (wall < HORIZONTAL_WALLS) {
        return {static_cast<float>(LEFT + (wall % COLS) * CELL - 1),
                static_cast<float>(TOP + (wall / COLS) * CELL - 1), CELL + 2.0f, 2};
    }
    const int i = wall - HORIZONTAL_WALLS;
    return {static_cast<float>(LEFT + (i % (COLS + 1)) * CELL - 1),
            static_cast<float>(TOP + (i / (COLS + 1)) * CELL - 1), 2, CELL + 2.0f};
}

bool Model::connected(int from, int to) const {
    const int wall = edge(from, to);
    return wall >= 0 && !state_.walls[wall];
}

void Model::wallCells(int wall, int& from, int& to) {
    from = to = -1;
    if (wall < HORIZONTAL_WALLS) {
        const int row = wall / COLS;
        if (row == 0 || row == ROWS) return;
        from = (row - 1) * COLS + wall % COLS;
        to = from + COLS;
    } else {
        const int i = wall - HORIZONTAL_WALLS;
        const int col = i % (COLS + 1);
        if (col == 0 || col == COLS) return;
        from = (i / (COLS + 1)) * COLS + col - 1;
        to = from + 1;
    }
}

LaserPhase Model::laserPhase(const Laser& laser, float clock) {
    const float phase = std::fmod(clock + laser.offset,
                                 LASER_OFF + LASER_WARNING + LASER_ACTIVE);
    return phase < LASER_OFF ? LaserPhase::Off
           : phase < LASER_OFF + LASER_WARNING ? LaserPhase::Warning : LaserPhase::Active;
}

Rect Model::gatePost(int wall, bool end) {
    Rect r = wallRect(wall);
    if (r.w > r.h) {
        if (end) r.x += r.w - 5;
        r.w = 5;
    } else {
        if (end) r.y += r.h - 5;
        r.h = 5;
    }
    return r;
}

void Model::begin(Mode mode, std::uint32_t seed, int start) {
    state_ = State{};
    state_.mode = mode;
    state_.stage = Stage::Playing;
    state_.start = start;
    rng_ = seed ? seed : 1;
    accumulator_ = 0;
    generate();
}

void Model::generate() {
    std::fill(std::begin(state_.walls), std::end(state_.walls), true);
    std::fill(std::begin(state_.protected_route), std::end(state_.protected_route), false);
    bool visited[CELLS] = {};
    int stack[CELLS] = {};
    int parent[CELLS] = {};
    int distance[CELLS] = {};
    int depth = 1;
    stack[0] = state_.start;
    visited[state_.start] = true;
    parent[state_.start] = -1;
    while (depth > 0) {
        const int cell = stack[depth - 1];
        int choices[4];
        int count = 0;
        const int neighbors[] = {cell - 1, cell + 1, cell - COLS, cell + COLS};
        for (int next : neighbors) {
            if (edge(cell, next) >= 0 && !visited[next]) choices[count++] = next;
        }
        if (!count) {
            --depth;
            continue;
        }
        const int next = choices[random() % count];
        state_.walls[edge(cell, next)] = false;
        visited[next] = true;
        parent[next] = cell;
        distance[next] = distance[cell] + 1;
        stack[depth++] = next;
    }
    const int target = state_.mode == Mode::Chill ? 8 + random() % 15
                       : 22 + static_cast<int>(std::min(state_.completed, 30U));
    state_.goal = state_.start;
    int error = CELLS;
    for (int cell = 0; cell < CELLS; ++cell) {
        if (cell == state_.start) continue;
        const int difference = std::abs(distance[cell] - target);
        if (difference < error) {
            error = difference;
            state_.goal = cell;
        }
    }
    state_.route_length = distance[state_.goal];
    for (int cell = state_.goal; cell >= 0; cell = parent[cell]) {
        state_.protected_route[cell] = true;
    }
    state_.ball = center(state_.start);
    state_.velocity = {};
    state_.admitted_gate = -1;
    state_.clock = 0;
    state_.protection = 0;
    placeHazards();
    state_.budget = state_.mode == Mode::Thrill
        ? state_.route_length * CELL / 30.0f + 20.0f +
          state_.laser_count * (LASER_WARNING + LASER_ACTIVE) : 0;
    state_.remaining = state_.budget;
    ++state_.revision;
}

bool Model::trapAt(int cell) const {
    for (int i = 0; i < state_.trap_count; ++i) {
        if (state_.traps[i] == cell) return true;
    }
    return false;
}

void Model::placeHazards() {
    state_.laser_count = state_.trap_count = state_.gate_count = 0;
    state_.requested_hazards = 0;
    if (state_.mode == Mode::Chill) return;
    state_.requested_hazards = 1 + static_cast<int>(std::min(state_.completed / 4, 3U));
    int cells[CELLS];
    for (int i = 0; i < CELLS; ++i) cells[i] = i;
    for (int i = CELLS - 1; i > 0; --i) std::swap(cells[i], cells[random() % (i + 1)]);
    for (int cell : cells) {
        if (state_.trap_count == state_.requested_hazards) break;
        if (state_.protected_route[cell]) continue;
        int degree = 0;
        const int neighbors[] = {cell - 1, cell + 1, cell - COLS, cell + COLS};
        for (int next : neighbors) degree += connected(cell, next) ? 1 : 0;
        // Removing an off-route leaf cannot break any safe route.
        if (degree == 1) state_.traps[state_.trap_count++] = cell;
    }
    int walls[WALLS];
    for (int i = 0; i < WALLS; ++i) walls[i] = i;
    for (int i = WALLS - 1; i > 0; --i) std::swap(walls[i], walls[random() % (i + 1)]);
    for (int wall : walls) {
        if (state_.laser_count == state_.requested_hazards) break;
        int a, b;
        wallCells(wall, a, b);
        if (a < 0 || state_.walls[wall] || a == state_.start || b == state_.start ||
            a == state_.goal || b == state_.goal || trapAt(a) || trapAt(b)) continue;
        state_.lasers[state_.laser_count++] = {wall, (random() % 500) / 100.0f};
    }
    for (int wall : walls) {
        if (state_.gate_count == state_.requested_hazards) break;
        int a, b;
        wallCells(wall, a, b);
        if (a < 0 || !state_.walls[wall] || trapAt(a) || trapAt(b) ||
            a == state_.start || b == state_.start || a == state_.goal || b == state_.goal) continue;
        bool laser_nearby = false;
        for (int i = 0; i < state_.laser_count; ++i) {
            int x, y;
            wallCells(state_.lasers[i].wall, x, y);
            laser_nearby |= a == x || a == y || b == x || b == y;
        }
        if (laser_nearby || !safeRoute(a) || !safeRoute(b)) continue;
        if (random() & 1) std::swap(a, b);
        state_.gates[state_.gate_count++] = {wall, a, b};
    }
}

bool Model::safeRoute(int origin) const {
    bool seen[CELLS] = {};
    int queue[CELLS] = {origin};
    int count = 1;
    seen[origin] = true;
    for (int i = 0; i < count; ++i) {
        const int cell = queue[i];
        if (cell == state_.goal) return true;
        const int neighbors[] = {cell - 1, cell + 1, cell - COLS, cell + COLS};
        for (int next : neighbors) {
            if (edge(cell, next) < 0 || seen[next] || trapAt(next)) continue;
            bool passable = connected(cell, next);
            for (int gate = 0; gate < state_.gate_count; ++gate) {
                passable |= state_.gates[gate].from == cell && state_.gates[gate].to == next;
            }
            if (passable) {
                seen[next] = true;
                queue[count++] = next;
            }
        }
    }
    return false;
}

bool Model::passGate(int index) {
    // Keep admission until the whole ball clears either side of the wall.
    if (state_.admitted_gate == index) return true;
    if (state_.admitted_gate >= 0) return false;
    const Gate& gate = state_.gates[index];
    const Vec2 a = center(gate.from);
    const Vec2 b = center(gate.to);
    const Vec2 direction{(b.x - a.x) / CELL, (b.y - a.y) / CELL};
    const Vec2 offset{state_.ball.x - a.x, state_.ball.y - a.y};
    const float along = offset.x * direction.x + offset.y * direction.y;
    const float across = offset.x * direction.y - offset.y * direction.x;
    const float speed = state_.velocity.x * direction.x + state_.velocity.y * direction.y;
    if (along >= CELL / 2 - RADIUS - 1 - MAX_SPEED * STEP &&
        along < CELL / 2 && std::fabs(across) <= 4 && speed > 0) {
        state_.admitted_gate = index;
        return true;
    }
    return false;
}

bool Model::hitHazard() const {
    if (state_.protection > 0) return false;
    for (int i = 0; i < state_.trap_count; ++i) {
        const Vec2 trap = center(state_.traps[i]);
        if (std::hypot(state_.ball.x - trap.x, state_.ball.y - trap.y) <= 7) return true;
    }
    for (int i = 0; i < state_.laser_count; ++i) {
        const Laser& laser = state_.lasers[i];
        if (laserPhase(laser, state_.clock) != LaserPhase::Active) continue;
        const Rect r = wallRect(laser.wall);
        const float x = std::clamp(state_.ball.x, r.x, r.x + r.w);
        const float y = std::clamp(state_.ball.y, r.y, r.y + r.h);
        if (std::hypot(state_.ball.x - x, state_.ball.y - y) <= RADIUS) return true;
    }
    return false;
}

void Model::move(float tilt_x, float tilt_y) {
    state_.velocity.x += steering(tilt_x) * 180.0f * STEP;
    state_.velocity.y += steering(tilt_y) * 180.0f * STEP;
    const float damping = 1.0f / (1.0f + 2.8f * STEP);
    state_.velocity.x *= damping;
    state_.velocity.y *= damping;
    const float speed = std::hypot(state_.velocity.x, state_.velocity.y);
    if (speed > MAX_SPEED) {
        state_.velocity.x *= MAX_SPEED / speed;
        state_.velocity.y *= MAX_SPEED / speed;
    }
    const int subdivisions = std::max(1, static_cast<int>(std::ceil(
        std::min(speed, MAX_SPEED) * STEP / (RADIUS / 2.0f))));
    for (int part = 0; part < subdivisions; ++part) {
        state_.ball.x += state_.velocity.x * STEP / subdivisions;
        state_.ball.y += state_.velocity.y * STEP / subdivisions;
        // Two passes resolve intersections where adjacent wall segments meet.
        for (int pass = 0; pass < 2; ++pass) {
            for (int wall = 0; wall < WALLS; ++wall) {
                if (!state_.walls[wall]) continue;
                int gate = -1;
                for (int i = 0; i < state_.gate_count; ++i) {
                    if (state_.gates[i].wall == wall) gate = i;
                }
                if (gate < 0 || !passGate(gate)) {
                    collide(state_.ball, state_.velocity, wallRect(wall));
                } else {
                    collide(state_.ball, state_.velocity, gatePost(wall, false));
                    collide(state_.ball, state_.velocity, gatePost(wall, true));
                }
            }
        }
        if (state_.admitted_gate >= 0) {
            const Gate& gate = state_.gates[state_.admitted_gate];
            const Vec2 a = center(gate.from);
            const Vec2 b = center(gate.to);
            const float along = ((state_.ball.x - a.x) * (b.x - a.x) +
                                 (state_.ball.y - a.y) * (b.y - a.y)) / CELL;
            if (along <= CELL / 2 - RADIUS - 1 || along >= CELL / 2 + RADIUS + 1) {
                state_.admitted_gate = -1;
            }
        }
    }
}

unsigned Model::step(float tilt_x, float tilt_y) {
    if (state_.stage == Stage::Transition) {
        state_.transition -= STEP;
        if (state_.transition <= 0.000001f) {
            state_.start = state_.goal;
            generate();
            state_.stage = Stage::Playing;
            return GENERATED;
        }
        return 0;
    }
    if (state_.stage != Stage::Playing) return 0;
    if (state_.mode == Mode::Thrill) {
        state_.remaining = std::max(0.0f, state_.remaining - STEP);
        if (state_.remaining <= 0) {
            state_.stage = Stage::Over;
            state_.velocity = {};
            return TIMED_OUT;
        }
    }
    state_.clock += STEP;
    state_.protection = std::max(0.0f, state_.protection - STEP);
    move(tilt_x, tilt_y);
    if (hitHazard()) {
        state_.ball = center(state_.start);
        state_.velocity = {};
        state_.admitted_gate = -1;
        state_.protection = PROTECTION;
        return RESET;
    }
    const Vec2 goal = center(state_.goal);
    if (std::hypot(state_.ball.x - goal.x, state_.ball.y - goal.y) <= 4) {
        state_.ball = goal;
        state_.velocity = {};
        if (state_.completed < UINT32_MAX) ++state_.completed;
        state_.stage = Stage::Transition;
        state_.transition = 0.6f;
        return COMPLETED | (state_.mode == Mode::Thrill ? SCORED : 0U);
    }
    return 0;
}

unsigned Model::update(const tg::Input& input, float seconds) {
    if (state_.stage == Stage::Picker) {
        auto current = static_cast<std::uint8_t>(state_.mode);
        state_.mode = static_cast<Mode>(mode_picker_.update(current, 2, input.main.delta));
        if (input.click) {
            begin(state_.mode, rng_);
            return STARTED | GENERATED;
        }
        return 0;
    }
    if (state_.stage == Stage::Over) return 0;
    accumulator_ += std::clamp(seconds, 0.0f, MAX_FRAME);
    unsigned events = 0;
    while (accumulator_ + 0.000001f >= STEP) {
        accumulator_ = std::max(0.0f, accumulator_ - STEP);
        const unsigned event = step(input.tilt_x, input.tilt_y);
        events |= event;
        if (event) {
            accumulator_ = 0;
            break;
        }
    }
    return events;
}

}  // namespace maze
