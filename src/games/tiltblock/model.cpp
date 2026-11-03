#include "model.h"

#include <algorithm>
#include <cmath>

#include "tg_random.h"

namespace tiltblock {

namespace {

constexpr float GRAVITY = 13.0f;
constexpr float DRAG = 1.6f;
constexpr float PI = 3.14159265358979323846f;
constexpr float TWIST_DEGREES_PER_TRANSITION = 1.0f;

void addScore(State& state, std::uint32_t amount) {
    state.score = amount > UINT32_MAX - state.score ? UINT32_MAX : state.score + amount;
}

bool isCornerCell(int index) {
    return index == 0 || index == SIDE - 1 || index == CELLS - SIDE || index == CELLS - 1;
}

bool edgeCell(int index) {
    const int col = index % SIDE;
    const int row = index / SIDE;
    return col == 0 || col == SIDE - 1 || row == 0 || row == SIDE - 1;
}

}  // namespace

Model::Model(std::uint32_t seed) : seed_(seed ? seed : 1) {
    state_.best = 0;
}

int Model::cell(int col, int row) {
    return col < 0 || row < 0 || col >= SIDE || row >= SIDE ? -1 : row * SIDE + col;
}

std::uint32_t Model::random() {
    return tg::randomNext(seed_);
}

float Model::stackDepthLayersForView(const State& state) {
    const float layers = static_cast<float>(
        std::clamp(state.layer, 1, LAYERS_PER_LEVEL));
    if (state.stage != Stage::LayerDrop) return layers;
    return layers + std::clamp(state.timer / 0.85f, 0.0f, 1.0f);
}

void Model::tiltAcceleration(float tilt_x, float tilt_y, float& x, float& y) {
    constexpr float DIAGONAL = 0.70710678118f;
    x = std::clamp((tilt_x + tilt_y) * DIAGONAL, -1.0f, 1.0f);
    y = std::clamp((-tilt_x + tilt_y) * DIAGONAL, -1.0f, 1.0f);
}

void Model::twistAcceleration(float left_angle, float right_angle, float& x, float& y) {
    x = std::sin(right_angle * PI / 180.0f);
    y = -std::sin(left_angle * PI / 180.0f);
}

void Model::startRun() {
    const Mode mode = state_.mode;
    const std::uint32_t best = state_.best;
    const float calibration_x = state_.calibration_x;
    const float calibration_y = state_.calibration_y;
    const int start_level = playtest_level_;
    const int start_layer = playtest_layer_;
    state_ = State{};
    state_.mode = mode;
    state_.best = best;
    state_.calibration_x = calibration_x;
    state_.calibration_y = calibration_y;
    state_.level = start_level;
    state_.layer = start_layer;
    state_.stage = Stage::Playing;
    accumulator_ = 0.0f;
    generateLayer();
    playtest_level_ = 1;
    playtest_layer_ = LAYERS_PER_LEVEL;
    playtest_mode_ = false;
}

int Model::cornerCell(int corner) const {
    constexpr int CORNERS[] = {0, SIDE - 1, CELLS - SIDE, CELLS - 1};
    return CORNERS[corner & 3];
}

int Model::safeCell(int preferred) const {
    if (preferred >= 0 && preferred < CELLS && !state_.wall[preferred] && !state_.hole[preferred]) {
        return preferred;
    }
    int best = 0;
    int distance = CELLS;
    const int origin_col = preferred % SIDE;
    const int origin_row = preferred / SIDE;
    for (int i = 0; i < CELLS; ++i) {
        if (state_.wall[i] || state_.hole[i]) continue;
        const int d = std::abs(i % SIDE - origin_col) + std::abs(i / SIDE - origin_row);
        if (d < distance) {
            distance = d;
            best = i;
        }
    }
    return best;
}

bool Model::connected() const {
    const int origin = cell(3, 3);
    if (state_.wall[origin] || state_.hole[origin]) return false;
    bool seen[CELLS] = {};
    int queue[CELLS] = {};
    int count = 1;
    queue[0] = origin;
    seen[origin] = true;
    for (int q = 0; q < count; ++q) {
        const int current = queue[q];
        const int col = current % SIDE;
        const int row = current / SIDE;
        const int adjacent[] = {cell(col - 1, row), cell(col + 1, row),
                                cell(col, row - 1), cell(col, row + 1)};
        for (int next : adjacent) {
            if (next < 0 || seen[next] || state_.wall[next] || state_.hole[next]) continue;
            seen[next] = true;
            queue[count++] = next;
        }
    }
    for (int i = 0; i < CELLS; ++i) {
        if (!state_.wall[i] && !state_.hole[i] && !seen[i]) return false;
        if (!state_.wall[i]) continue;
        bool reachable_side = false;
        const int col = i % SIDE;
        const int row = i / SIDE;
        const int adjacent[] = {cell(col - 1, row), cell(col + 1, row),
                                cell(col, row - 1), cell(col, row + 1)};
        for (int next : adjacent) {
            reachable_side |= next >= 0 && seen[next];
        }
        if (!reachable_side) return false;
    }
    return true;
}

bool Model::solvable(const State& state) {
    State copy = state;
    const int origin = cell(3, 3);
    if (copy.wall[origin] || copy.hole[origin]) return false;
    bool seen[CELLS] = {};
    int queue[CELLS] = {};
    int count = 1;
    queue[0] = origin;
    seen[origin] = true;
    for (int q = 0; q < count; ++q) {
        const int current = queue[q];
        const int col = current % SIDE;
        const int row = current / SIDE;
        const int adjacent[] = {cell(col - 1, row), cell(col + 1, row),
                                cell(col, row - 1), cell(col, row + 1)};
        for (int next : adjacent) {
            if (next < 0 || seen[next] || copy.wall[next] || copy.hole[next]) continue;
            seen[next] = true;
            queue[count++] = next;
        }
    }
    for (int i = 0; i < CELLS; ++i) {
        if (!copy.wall[i] && !copy.hole[i] && !seen[i]) return false;
        if (!copy.wall[i]) continue;
        const int col = i % SIDE;
        const int row = i / SIDE;
        const int adjacent[] = {cell(col - 1, row), cell(col + 1, row),
                                cell(col, row - 1), cell(col, row + 1)};
        bool reachable_side = false;
        for (int next : adjacent) reachable_side |= next >= 0 && seen[next];
        if (!reachable_side) return false;
    }
    return true;
}

void Model::addBonus(Bonus bonus) {
    int candidates[CELLS];
    int count = 0;
    for (int i = 0; i < CELLS; ++i) {
        if (i == cell(3, 3) || edgeCell(i) || state_.wall[i] || state_.hole[i] ||
            state_.bonus[i] != Bonus::None) continue;
        candidates[count++] = i;
    }
    if (count > 0) state_.bonus[candidates[random() % count]] = bonus;
}

void Model::generateLayer() {
    for (int i = 0; i < CELLS; ++i) {
        state_.touched[i] = false;
        state_.wall[i] = false;
        state_.hole[i] = false;
        state_.bonus[i] = Bonus::None;
    }

    int order[CELLS];
    for (int i = 0; i < CELLS; ++i) order[i] = i;
    for (int i = CELLS - 1; i > 0; --i) {
        std::swap(order[i], order[random() % (i + 1)]);
    }

    // layer counts down; tutorial lessons count up in the order played.
    const int lesson = state_.level == 1 ? LAYERS_PER_LEVEL - state_.layer + 1 : 0;
    const int wall_target = lesson == 3 || lesson == 8 ? 5 : state_.level < 2 ? 0
        : std::min(5 + (state_.level - 2) * 2, 22);
    int walls = 0;
    for (int i : order) {
        if (walls >= wall_target) break;
        if (isCornerCell(i) || i == cell(3, 3)) continue;
        state_.wall[i] = true;
        if (connected()) {
            ++walls;
        } else {
            state_.wall[i] = false;
        }
    }

    const int hole_target = lesson == 2 || lesson == 8 ? 5 : state_.level < 3 ? 0
        : std::min(2 + (state_.level - 3) * 2, 14);
    int holes = 0;
    if (state_.level >= 3) {
        for (int i : order) {
            if (!edgeCell(i) || isCornerCell(i) || i == cell(3, 3) || state_.wall[i]) continue;
            state_.hole[i] = true;
            if (connected()) {
                ++holes;
                break;
            }
            state_.hole[i] = false;
        }
    }
    for (int i : order) {
        if (holes >= hole_target) break;
        if (isCornerCell(i) || i == cell(3, 3) || state_.wall[i]) continue;
        state_.hole[i] = true;
        if (connected()) {
            ++holes;
        } else {
            state_.hole[i] = false;
        }
    }

    if (lesson == 4 || lesson == 8 || state_.level >= 4) {
        addBonus(Bonus::Life);
        addBonus(Bonus::Zone);
        if (!state_.tractor) addBonus(Bonus::Tractor);
    }
    if (lesson == 8 || state_.level >= 5) {
        addBonus(Bonus::Boing);
        addBonus(Bonus::Quake);
    }
    if (lesson == 5) addBonus(Bonus::Boing);
    if (lesson == 6) addBonus(Bonus::Quake);

    state_.monster_alive = false;
    state_.monster_pending = false;
    state_.monster_corner = -1;
    state_.laser_cell = -1;
    state_.laser_armed = false;
    state_.monster_pause = 0.0f;
    if (lesson == 7 || lesson == 8) {
        spawnMonster();
    } else if (state_.level >= 6) {
        const int chance = std::min(25 + (state_.level - 6) * 10, 75);
        if (playtest_mode_ || static_cast<int>(random() % 100) < chance) {
            state_.monster_pending = true;
            state_.monster_spawn = 2.5f;
        }
    }

    const int start = safeCell(cell(3, 3));
    state_.x = static_cast<float>(start % SIDE) + 0.5f;
    state_.y = static_cast<float>(start / SIDE) + 0.5f;
    state_.vx = state_.vy = 0.0f;
    state_.safety = 0.7f;
    state_.airborne = 0.0f;
    state_.earthquake = 0.0f;
    state_.quake_shove = 0.0f;
    state_.laser_cooldown = 0.0f;
    state_.laser_flash = 0.0f;
    state_.monster_burst = 0.0f;
    for (Feedback& item : state_.feedback) item = Feedback{};
    state_.timer = 0.85f;
}

void Model::markTouched(int index) {
    if (index < 0 || index >= CELLS || state_.hole[index] || state_.touched[index]) return;
    state_.touched[index] = true;
    addScore(state_, 10);
}

void Model::touch(int index) {
    if (index < 0 || index >= CELLS || state_.hole[index] || state_.touched[index]) return;
    const Bonus bonus = state_.bonus[index];
    markTouched(index);
    state_.bonus[index] = Bonus::None;
    if (bonus == Bonus::None) return;
    collect(index, bonus);
}

void Model::collect(int index, Bonus bonus) {
    const Message message = bonus == Bonus::Life
        ? (state_.lives < MAX_LIVES ? Message::Life : Message::LifeFull)
        : bonus == Bonus::Zone ? Message::Zone
        : bonus == Bonus::Boing ? Message::Boing
        : bonus == Bonus::Quake ? Message::Quake
        : bonus == Bonus::Tractor ? Message::Tractor
        : bonus == Bonus::Laser ? Message::Laser : Message::None;
    feedback(message, index % SIDE + 0.5f, index / SIDE + 0.5f);
    switch (bonus) {
        case Bonus::Life:
            if (state_.lives < MAX_LIVES) ++state_.lives;
            break;
        case Bonus::Zone: {
            const int col = index % SIDE;
            const int row = index / SIDE;
            for (int y = row - 1; y <= row + 1; ++y) {
                for (int x = col - 1; x <= col + 1; ++x) {
                    if (x == col && y == row) continue;
                    touch(cell(x, y));
                }
            }
            break;
        }
        case Bonus::Boing:
            state_.airborne = 1.0f;
            break;
        case Bonus::Quake:
            state_.earthquake = 3.0f;
            state_.quake_shove = 0.0f;
            break;
        case Bonus::Tractor:
            state_.tractor = true;
            break;
        case Bonus::Laser:
            state_.laser_armed = true;
            break;
        case Bonus::None:
            break;
    }
}

const char* Model::messageText(Message message) {
    switch (message) {
        case Message::Life: return "+1 LIFE";
        case Message::LifeFull: return "LIFE FULL";
        case Message::Zone: return "ZONE";
        case Message::Boing: return "BOING";
        case Message::Quake: return "QUAKE";
        case Message::Tractor: return "TRACTOR READY";
        case Message::Laser: return "LASER READY";
        case Message::Kill: return "ALIEN +500";
        case Message::Saved: return "SAVED";
        case Message::Lost: return "-1 LIFE";
        case Message::Aim: return "ROLL TO AIM";
        case Message::None: return "";
    }
    return "";
}

void Model::feedback(Message message, float x, float y) {
    if (message == Message::None) return;
    Feedback* slot = &state_.feedback[0];
    for (Feedback& item : state_.feedback) {
        if (item.remaining < slot->remaining) slot = &item;
        if (item.remaining <= 0.0f) {
            slot = &item;
            break;
        }
    }
    *slot = {message, x, y, FEEDBACK_SECONDS};
}

bool Model::complete() const {
    for (int i = 0; i < CELLS; ++i) {
        if (!state_.hole[i] && !state_.touched[i]) return false;
    }
    return true;
}

void Model::respawn() {
    const int index = safeCell(cell(3, 3));
    state_.x = static_cast<float>(index % SIDE) + 0.5f;
    state_.y = static_cast<float>(index / SIDE) + 0.5f;
    state_.vx = state_.vy = 0.0f;
    state_.safety = 1.5f;
    state_.airborne = 0.0f;
    state_.laser_flash = 0.0f;
}

void Model::loseLife(bool edge_fall, unsigned& events) {
    if (edge_fall && state_.tractor) {
        feedback(Message::Saved, state_.x, state_.y);
        state_.tractor = false;
        state_.stage = Stage::TractorSave;
        state_.timer = 0.8f;
        state_.vx = state_.vy = 0.0f;
        events |= BONUS_USED;
        return;
    }
    if (state_.lives > 0) --state_.lives;
    feedback(Message::Lost, state_.x, state_.y);
    events |= LIFE_LOST;
    if (state_.monster_alive) state_.monster_pause = 1.0f;
    if (state_.lives <= 0) {
        state_.stage = Stage::Over;
        state_.vx = state_.vy = 0.0f;
        events |= GAME_OVER;
        return;
    }
    respawn();
}

void Model::beginFall(bool edge, bool hole) {
    state_.stage = Stage::Falling;
    state_.timer = FALL_SECONDS;
    state_.fall_edge = edge;
    state_.fall_hole = hole;
    state_.safety = 0.0f;
    state_.airborne = 0.0f;
}

void Model::finishLayer(unsigned& events) {
    addScore(state_, static_cast<std::uint32_t>(100 * state_.level));
    state_.stage = Stage::BeamUp;
    state_.timer = BEAM_SECONDS;
    state_.vx = state_.vy = 0.0f;
    state_.monster_alive = false;
    state_.monster_pending = false;
    state_.laser_armed = false;
    events |= LAYER_CLEARED;
    if (state_.layer == 1) {
        addScore(state_, static_cast<std::uint32_t>(1000 * state_.level));
        events |= LEVEL_CLEARED;
    }
}

void Model::move(float ax, float ay, unsigned& events) {
    state_.vx += ax * GRAVITY * STEP;
    state_.vy += ay * GRAVITY * STEP;
    const float damping = 1.0f / (1.0f + DRAG * STEP);
    state_.vx *= damping;
    state_.vy *= damping;
    const float speed = std::hypot(state_.vx, state_.vy);
    if (speed > MAX_SPEED) {
        state_.vx *= MAX_SPEED / speed;
        state_.vy *= MAX_SPEED / speed;
    }

    state_.x += state_.vx * STEP;
    state_.y += state_.vy * STEP;

    if (state_.airborne <= 0.0f) {
        for (int index = 0; index < CELLS; ++index) {
            if (!state_.wall[index]) continue;
            const float left = static_cast<float>(index % SIDE);
            const float top = static_cast<float>(index / SIDE);
            const float nearest_x = std::clamp(state_.x, left, left + 1.0f);
            const float nearest_y = std::clamp(state_.y, top, top + 1.0f);
            float dx = state_.x - nearest_x;
            float dy = state_.y - nearest_y;
            float distance = std::hypot(dx, dy);
            if (distance >= BALL_RADIUS) continue;
            if (distance < 0.0001f) {
                const float to_left = state_.x - left;
                const float to_right = left + 1.0f - state_.x;
                const float to_top = state_.y - top;
                const float to_bottom = top + 1.0f - state_.y;
                const float nearest = std::min(std::min(to_left, to_right), std::min(to_top, to_bottom));
                if (nearest == to_left) dx = -1.0f, dy = 0.0f;
                else if (nearest == to_right) dx = 1.0f, dy = 0.0f;
                else if (nearest == to_top) dx = 0.0f, dy = -1.0f;
                else dx = 0.0f, dy = 1.0f;
                distance = -nearest;
            } else {
                dx /= distance;
                dy /= distance;
            }
            state_.x += dx * (BALL_RADIUS - distance);
            state_.y += dy * (BALL_RADIUS - distance);
            const float inward = state_.vx * dx + state_.vy * dy;
            if (inward < 0.0f) {
                state_.vx -= inward * dx * 1.35f;
                state_.vy -= inward * dy * 1.35f;
            }
            touch(index);
        }
    }

    if (state_.x < 0.0f || state_.x >= SIDE || state_.y < 0.0f || state_.y >= SIDE) {
        beginFall(true, false);
        return;
    }
    const int index = cell(static_cast<int>(state_.x), static_cast<int>(state_.y));
    if (state_.airborne <= 0.0f && state_.hole[index]) {
        beginFall(edgeCell(index), true);
        return;
    }
    if (state_.airborne <= 0.0f && !state_.wall[index]) touch(index);
    if (state_.airborne > 0.0f) {
        state_.airborne = std::max(0.0f, state_.airborne - STEP);
    }
    if (state_.stage == Stage::Playing && complete()) finishLayer(events);
}

void Model::spawnMonster() {
    state_.monster_pending = false;
    state_.monster_alive = true;
    state_.monster_corner = static_cast<int>(random() % 4);
    const int corner = cornerCell(state_.monster_corner);
    state_.monster_x = static_cast<float>(corner % SIDE) + 0.5f;
    state_.monster_y = static_cast<float>(corner / SIDE) + 0.5f;
    state_.laser_cell = cornerCell(state_.monster_corner ^ 3);
    state_.bonus[state_.laser_cell] = Bonus::Laser;
}

void Model::moveMonster(float dt, unsigned& events) {
    if (state_.monster_pending) {
        state_.monster_spawn -= dt;
        if (state_.monster_spawn <= 0.0f) {
            spawnMonster();
        }
    }
    if (!state_.monster_alive) return;
    if (state_.monster_pause > 0.0f) {
        state_.monster_pause = std::max(0.0f, state_.monster_pause - dt);
        if (state_.monster_pause == 0.0f) {
            spawnMonster();
        }
        return;
    }
    const float dx = state_.x - state_.monster_x;
    const float dy = state_.y - state_.monster_y;
    const float distance = std::hypot(dx, dy);
    if (distance < 0.42f) {
        loseLife(false, events);
        return;
    }
    if (distance > 0.0001f) {
        const float amount = std::min(distance, 0.22f * dt) / distance;
        state_.monster_x += dx * amount;
        state_.monster_y += dy * amount;
    }
}

void Model::fireLaser(unsigned& events) {
    if (!state_.laser_armed || state_.laser_cooldown > 0.0f) return;
    const float speed = std::hypot(state_.vx, state_.vy);
    if (speed < 0.05f) {
        state_.laser_cooldown = 3.0f;
        feedback(Message::Aim, state_.x, state_.y);
        return;
    }
    state_.laser_flash = LASER_SECONDS;
    state_.laser_cooldown = 3.0f;
    sweepLaser(events);
}

void Model::sweepLaser(unsigned& events) {
    if (state_.laser_flash <= 0.0f) return;
    const float speed = std::hypot(state_.vx, state_.vy);
    const float previous_x = state_.laser_x2 - state_.laser_x1;
    const float previous_y = state_.laser_y2 - state_.laser_y1;
    const float previous_length = std::hypot(previous_x, previous_y);
    if (speed < 0.05f && previous_length < 0.0001f) return;
    // Keep the last heading when braking to a stop during the flash.
    const float dx = speed >= 0.05f ? state_.vx / speed : previous_x / previous_length;
    const float dy = speed >= 0.05f ? state_.vy / speed : previous_y / previous_length;
    state_.laser_x1 = state_.x;
    state_.laser_y1 = state_.y;
    // The view clips this ray at the screen edge, including when the board tips.
    state_.laser_x2 = state_.x + dx * 128.0f;
    state_.laser_y2 = state_.y + dy * 128.0f;
    if (state_.monster_alive) {
        const float mx = state_.monster_x - state_.x;
        const float my = state_.monster_y - state_.y;
        const float along = mx * dx + my * dy;
        const float across = std::fabs(mx * dy - my * dx);
        if (along > 0.0f && across <= 0.38f) {
            state_.burst_x = state_.monster_x;
            state_.burst_y = state_.monster_y;
            state_.monster_burst = BURST_SECONDS;
            feedback(Message::Kill, state_.monster_x, state_.monster_y);
            state_.monster_alive = false;
            state_.monster_pending = false;
            state_.laser_armed = false;
            addScore(state_, 500);
            events |= MONSTER_KILLED;
        }
    }
}

void Model::step(float ax, float ay, unsigned& events) {
    for (Feedback& item : state_.feedback) {
        item.remaining = std::max(0.0f, item.remaining - STEP);
    }
    state_.monster_burst = std::max(0.0f, state_.monster_burst - STEP);
    state_.laser_flash = std::max(0.0f, state_.laser_flash - STEP);
    if (state_.stage == Stage::Falling || state_.stage == Stage::BeamUp) {
        state_.timer = std::max(0.0f, state_.timer - STEP);
        if (state_.timer > 0.00001f) return;
        if (state_.stage == Stage::Falling) {
            state_.stage = Stage::Playing;
            loseLife(state_.fall_edge, events);
        } else if (state_.layer == 1) {
            if (state_.level < 1000000) ++state_.level;
            state_.stage = Stage::LevelClear;
            state_.timer = 1.1f;
        } else {
            --state_.layer;
            state_.stage = Stage::LayerDrop;
            generateLayer();
        }
        return;
    }
    if (state_.stage == Stage::LayerDrop || state_.stage == Stage::LevelClear ||
        state_.stage == Stage::TractorSave) {
        state_.timer -= STEP;
        if (state_.timer > 0.0f) return;
        if (state_.stage == Stage::LayerDrop) {
            state_.stage = Stage::Playing;
        } else if (state_.stage == Stage::LevelClear) {
            state_.layer = LAYERS_PER_LEVEL;
            state_.stage = Stage::Playing;
            generateLayer();
        } else {
            state_.stage = Stage::Playing;
            respawn();
        }
        return;
    }
    if (state_.stage != Stage::Playing) return;

    state_.safety = std::max(0.0f, state_.safety - STEP);
    state_.laser_cooldown = std::max(0.0f, state_.laser_cooldown - STEP);
    state_.earthquake = std::max(0.0f, state_.earthquake - STEP);
    if (state_.airborne <= 0.0f && state_.earthquake > 0.0f) {
        state_.quake_shove -= STEP;
        if (state_.quake_shove <= 0.0f) {
            state_.vx += (static_cast<int>(random() % 201) - 100) / 180.0f;
            state_.vy += (static_cast<int>(random() % 201) - 100) / 180.0f;
            state_.quake_shove = 0.35f;
        }
    }
    move(ax, ay, events);
    if (state_.stage != Stage::Playing) return;
    sweepLaser(events);
    if (state_.safety <= 0.0f) moveMonster(STEP, events);
    if (state_.stage == Stage::Playing && state_.safety <= 0.0f &&
        state_.monster_alive &&
        std::hypot(state_.x - state_.monster_x,
                                          state_.y - state_.monster_y) < 0.42f) {
        loseLife(false, events);
    }
}

unsigned Model::update(const tg::Input& input, float seconds) {
    if (state_.stage == Stage::ModeSelect) {
        if (input.main.delta == 1233) {
            playtest_level_ = 1;
            playtest_layer_ = 4;
            playtest_mode_ = false;
        } else if (input.main.delta == 1234) {
            playtest_level_ = 6;
            playtest_layer_ = LAYERS_PER_LEVEL;
            playtest_mode_ = true;
        } else if (input.main.delta == 1235 || input.main.delta == 1236) {
            playtest_level_ = 1;
            playtest_layer_ = input.main.delta == 1235 ? 2 : 1;
            playtest_mode_ = false;
        }
        const std::uint8_t choice = mode_picker_.update(
            static_cast<std::uint8_t>(state_.mode), 2, input.main.delta);
        state_.mode = static_cast<Mode>(choice);
        if (input.click) {
            if (state_.mode == Mode::Tilt) state_.stage = Stage::Calibration;
            else startRun();
        }
        return NO_EVENT;
    }
    if (state_.stage == Stage::Calibration) {
        if (input.click) {
            state_.calibration_x = input.tilt_x;
            state_.calibration_y = input.tilt_y;
            startRun();
        }
        return NO_EVENT;
    }
    if (state_.stage == Stage::Over) {
        if (input.click) {
            state_.stage = Stage::ModeSelect;
            state_.mode = Mode::Tilt;
            state_.left_angle = 0.0f;
            state_.right_angle = 0.0f;
            mode_picker_ = tg::Picker{};
        }
        return NO_EVENT;
    }

    if (state_.mode == Mode::Twist && state_.stage == Stage::Playing) {
        state_.left_angle = std::clamp(
            state_.left_angle + input.left.delta * TWIST_DEGREES_PER_TRANSITION, -20.0f, 20.0f);
        state_.right_angle = std::clamp(
            state_.right_angle + input.right.delta * TWIST_DEGREES_PER_TRANSITION, -20.0f, 20.0f);
    }
    unsigned events = NO_EVENT;
    if (state_.stage == Stage::Playing && input.click) fireLaser(events);

    float ax = 0.0f;
    float ay = 0.0f;
    if (state_.mode == Mode::Tilt) {
        const float tx = std::clamp(input.tilt_x - state_.calibration_x, -1.0f, 1.0f);
        const float ty = std::clamp(input.tilt_y - state_.calibration_y, -1.0f, 1.0f);
        tiltAcceleration(tx, ty, ax, ay);
    } else {
        twistAcceleration(state_.left_angle, state_.right_angle, ax, ay);
    }

    accumulator_ += std::clamp(seconds, 0.0f, MAX_FRAME);
    while (accumulator_ + 0.000001f >= STEP) {
        accumulator_ = std::max(0.0f, accumulator_ - STEP);
        step(ax, ay, events);
        if (events & (LIFE_LOST | LAYER_CLEARED | LEVEL_CLEARED | GAME_OVER | BONUS_USED)) {
            accumulator_ = 0.0f;
            break;
        }
    }
    return events;
}

}  // namespace tiltblock
