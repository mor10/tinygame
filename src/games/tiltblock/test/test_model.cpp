#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <utility>

#include "model.h"

using namespace tiltblock;

namespace {

Model playing(Mode mode = Mode::Twist, std::uint32_t seed = 81) {
    Model model(seed);
    tg::Input input;
    if (mode == Mode::Twist) input.main.delta = 4;
    input.click = true;
    model.update(input, 0.01f);
    assert(model.state().stage == Stage::Playing);
    assert(model.state().mode == mode);
    return model;
}

Model playingTilt() {
    Model model(81);
    tg::Input click;
    click.click = true;
    model.update(click, 0.01f);
    model.update(tg::Input{}, 0.01f);
    click.tilt_x = click.tilt_y = 0.0f;
    click.click = true;
    model.update(click, 0.01f);
    assert(model.state().stage == Stage::Playing);
    return model;
}

void clearBoard(State& state) {
    for (int i = 0; i < CELLS; ++i) {
        state.hole[i] = false;
        state.wall[i] = false;
        state.bonus[i] = Bonus::None;
    }
    state.monster_alive = false;
    state.safety = 0.0f;
}

float speedOf(const State& state) { return std::hypot(state.vx, state.vy); }

unsigned advance(Model& model, float seconds) {
    unsigned events = NO_EVENT;
    const int steps = static_cast<int>(std::lround(seconds / STEP));
    for (int i = 0; i < steps; ++i) events |= model.update(tg::Input{}, STEP);
    return events;
}

void clearLayer(Model& model) {
    State& state = model.mutableState();
    for (bool& touched : state.touched) touched = true;
    assert(model.update(tg::Input{}, STEP) & LAYER_CLEARED);
    advance(model, BEAM_SECONDS);
    advance(model, 1.2f);
    assert(state.stage == Stage::Playing);
}

Model tutorial(int lesson, std::uint32_t seed = 81) {
    Model model = playing(Mode::Twist, seed);
    for (int i = 1; i < lesson; ++i) clearLayer(model);
    assert(model.state().level == 1);
    assert(model.state().layer == LAYERS_PER_LEVEL - lesson + 1);
    return model;
}

int bonusCount(const State& state, Bonus bonus) {
    return static_cast<int>(std::count(std::begin(state.bonus), std::end(state.bonus), bonus));
}

void tutorialLessonsFollowPlayOrder() {
    bool holes_seen[CELLS] = {};
    bool walls_seen[CELLS] = {};
    for (std::uint32_t seed = 1; seed <= 128; ++seed) {
        Model model = playing(Mode::Twist, seed);
        Model repeat = playing(Mode::Twist, seed);
        for (int lesson = 1; lesson <= LAYERS_PER_LEVEL; ++lesson) {
            const State& s = model.state();
            assert(s.level == 1 && LAYERS_PER_LEVEL - s.layer + 1 == lesson);
            assert(Model::solvable(s));
            assert(s.x == 3.5f && s.y == 3.5f && s.vx == 0.0f && s.vy == 0.0f);
            const int center = Model::cell(3, 3);
            assert(!s.wall[center] && !s.hole[center] && s.bonus[center] == Bonus::None);
            assert(std::count(std::begin(s.hole), std::end(s.hole), true) == (lesson == 2 || lesson == 8 ? 5 : 0));
            assert(std::count(std::begin(s.wall), std::end(s.wall), true) == (lesson == 3 || lesson == 8 ? 5 : 0));
            assert(bonusCount(s, Bonus::Life) == (lesson == 4 || lesson == 8 ? 1 : 0));
            assert(bonusCount(s, Bonus::Zone) == (lesson == 4 || lesson == 8 ? 1 : 0));
            assert(bonusCount(s, Bonus::Tractor) == (lesson == 4 || lesson == 8 ? 1 : 0));
            assert(bonusCount(s, Bonus::Boing) == (lesson == 5 || lesson == 8 ? 1 : 0));
            assert(bonusCount(s, Bonus::Quake) == (lesson == 6 || lesson == 8 ? 1 : 0));
            assert(bonusCount(s, Bonus::Laser) == (lesson >= 7 ? 1 : 0));
            assert(s.monster_alive == (lesson >= 7));
            assert(!s.monster_pending && !s.laser_armed);
            for (int i = 0; i < CELLS; ++i) {
                holes_seen[i] |= s.hole[i];
                walls_seen[i] |= s.wall[i];
                assert(s.wall[i] == repeat.state().wall[i]);
                assert(s.hole[i] == repeat.state().hole[i]);
                assert(s.bonus[i] == repeat.state().bonus[i]);
                assert(!(s.wall[i] && s.hole[i]));
                if (s.bonus[i] != Bonus::None) assert(!s.wall[i] && !s.hole[i]);
            }
            assert(s.monster_corner == repeat.state().monster_corner);
            clearLayer(model);
            clearLayer(repeat);
        }
        assert(model.state().level == 2 && model.state().layer == LAYERS_PER_LEVEL);
        assert(!model.state().monster_alive);
    }
    assert(std::count(std::begin(holes_seen), std::end(holes_seen), true) > 1);
    assert(std::count(std::begin(walls_seen), std::end(walls_seen), true) > 1);
}

void tutorialHoleAndWallUseNormalRules() {
    Model holes = tutorial(2);
    State& h = holes.mutableState();
    const int hole = static_cast<int>(std::find(std::begin(h.hole), std::end(h.hole), true) -
                                      std::begin(h.hole));
    h.x = hole % SIDE + 0.5f;
    h.y = hole / SIDE + 0.5f;
    holes.update(tg::Input{}, STEP);
    assert(h.stage == Stage::Falling && h.fall_hole);
    assert(advance(holes, FALL_SECONDS) & LIFE_LOST);
    assert(h.lives == START_LIVES - 1 && h.layer == 7);

    Model walls = tutorial(3);
    State& w = walls.mutableState();
    const int wall = static_cast<int>(std::find(std::begin(w.wall), std::end(w.wall), true) -
                                      std::begin(w.wall));
    // Approach a reachable side from inside the board.
    int dx = 0, dy = 0;
    for (const auto& offset : {std::pair<int, int>{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
        const int neighbor = Model::cell(wall % SIDE + offset.first, wall / SIDE + offset.second);
        if (neighbor >= 0 && !w.wall[neighbor]) {
            dx = offset.first;
            dy = offset.second;
            break;
        }
    }
    assert(dx || dy);
    w.x = wall % SIDE + 0.5f + dx * 0.6f;
    w.y = wall / SIDE + 0.5f + dy * 0.6f;
    w.vx = static_cast<float>(-dx);
    w.vy = static_cast<float>(-dy);
    walls.update(tg::Input{}, STEP);
    assert(w.touched[wall]);
    assert(w.vx * dx + w.vy * dy > 0.0f);
}

void laserLessonAndFinaleAreLive() {
    tg::Input click;
    click.click = true;
    Model encounter = tutorial(7);
    State& s = encounter.mutableState();
    assert(s.monster_alive && s.laser_cell >= 0);
    s.x = s.laser_cell % SIDE + 0.5f;
    s.y = s.laser_cell / SIDE + 0.5f;
    encounter.update(tg::Input{}, STEP);
    assert(s.laser_armed && s.bonus[s.laser_cell] == Bonus::None);
    assert(!(encounter.update(click, 0.0f) & MONSTER_KILLED));
    assert(s.laser_cooldown == 3.0f);  // Must roll to aim, just like later levels.
    advance(encounter, 3.1f);
    const float dx = s.monster_x - s.x, dy = s.monster_y - s.y;
    const float distance = std::hypot(dx, dy);
    assert(distance > 8.0f);
    s.vx = dx / distance;
    s.vy = dy / distance;
    const std::uint32_t before = s.score;
    assert(encounter.update(click, 0.0f) & MONSTER_KILLED);
    assert(s.score == before + 500 && !s.monster_alive && !s.laser_armed);
    assert(s.monster_burst == BURST_SECONDS);

    Model contact = tutorial(8);
    State& c = contact.mutableState();
    c.safety = 0.0f;
    c.x = c.monster_x;
    c.y = c.monster_y;
    assert(contact.update(tg::Input{}, STEP) & LIFE_LOST);
    assert(c.lives == START_LIVES - 1);

    Model moving = tutorial(7);
    const float old_x = moving.state().monster_x, old_y = moving.state().monster_y;
    advance(moving, 1.0f);
    assert(std::hypot(moving.state().monster_x - old_x, moving.state().monster_y - old_y) > 0.0f);
}

void laterLevelFeatureThresholdsStayUnchanged() {
    for (int level = 2; level <= 10; ++level) {
        int encounters = 0;
        for (std::uint32_t seed = 1; seed <= 128; ++seed) {
            Model model = playing(Mode::Twist, seed);
            State& s = model.mutableState();
            s.level = level;
            s.stage = Stage::LevelClear;
            s.timer = 0.0f;
            model.update(tg::Input{}, STEP);
            assert(s.stage == Stage::Playing && s.layer == LAYERS_PER_LEVEL);
            assert(Model::solvable(s) && !s.monster_alive);
            const int walls = std::count(std::begin(s.wall), std::end(s.wall), true);
            const int holes = std::count(std::begin(s.hole), std::end(s.hole), true);
            assert(walls > 0 && walls <= std::min(5 + (level - 2) * 2, 22));
            if (level < 3) assert(holes == 0);
            else assert(holes > 0 && holes <= std::min(2 + (level - 3) * 2, 14));
            assert(bonusCount(s, Bonus::Life) == (level >= 4 ? 1 : 0));
            assert(bonusCount(s, Bonus::Zone) == (level >= 4 ? 1 : 0));
            assert(bonusCount(s, Bonus::Tractor) == (level >= 4 ? 1 : 0));
            assert(bonusCount(s, Bonus::Boing) == (level >= 5 ? 1 : 0));
            assert(bonusCount(s, Bonus::Quake) == (level >= 5 ? 1 : 0));
            assert(bonusCount(s, Bonus::Laser) == 0);
            if (s.monster_pending) {
                ++encounters;
                assert(s.monster_spawn == 2.5f);
            }
        }
        if (level < 6) assert(encounters == 0);
        else assert(encounters > 0 && encounters < 128);
    }
}

// Keeps the ball near the middle so long runs measure speed, not edge falls.
void recentre(State& state) {
    state.x = 3.5f;
    state.y = 3.5f;
}

void rollingIsResponsiveAndBrakes() {
    // Full one-axis Twist: about twice the old 1.4 cubes/s terminal speed.
    Model twist = playing();
    State& t = twist.mutableState();
    clearBoard(t);
    tg::Input lean;
    lean.right.delta = 20;
    twist.update(lean, 0.0f);
    const tg::Input hold{};
    for (int i = 0; i < 30; ++i) twist.update(hold, 1.0f / 60.0f);  // 0.5 s
    assert(t.vx > 1.2f);                  // old tuning: ~0.66 cubes/s
    for (int i = 0; i < 600; ++i) {
        twist.update(hold, 1.0f / 60.0f);
        recentre(t);
    }
    assert(t.vx > 2.5f && t.vx < 3.1f);   // terminal ~2.8 cubes/s
    assert(std::fabs(t.vy) < 0.0001f);

    // Leaning the other way brakes quickly and reverses.
    tg::Input back;
    back.right.delta = -40;
    twist.update(back, 0.0f);
    float stop_time = 0.0f;
    while (t.vx > 0.0f && stop_time < 2.0f) {
        twist.update(hold, 1.0f / 60.0f);
        recentre(t);
        stop_time += 1.0f / 60.0f;
    }
    assert(stop_time < 0.5f);

    // A gentle Tilt (0.2) gets going sooner than before (old: ~0.6 after 1 s).
    Model gentle = playingTilt();
    State& g = gentle.mutableState();
    clearBoard(g);
    tg::Input tilt;
    tilt.tilt_x = 0.2f * 0.70710678f;
    tilt.tilt_y = 0.2f * 0.70710678f;
    for (int i = 0; i < 60; ++i) {
        gentle.update(tilt, 1.0f / 60.0f);
        recentre(g);
    }
    assert(g.vx > 1.0f && std::fabs(g.vy) < 0.0001f);

    // Full Tilt hits the speed cap, and one physics step moves far less than
    // the ball radius, so walls and holes can't be skipped.
    Model fast = playingTilt();
    State& f = fast.mutableState();
    clearBoard(f);
    tilt.tilt_x = tilt.tilt_y = 0.70710678f;
    for (int i = 0; i < 300; ++i) {
        fast.update(tilt, 1.0f / 60.0f);
        recentre(f);
        assert(speedOf(f) <= MAX_SPEED + 0.0001f);
    }
    assert(speedOf(f) > MAX_SPEED - 0.01f);
    assert(MAX_SPEED * STEP < BALL_RADIUS * 0.5f);
}

void fastBallStillCollides() {
    Model model = playingTilt();
    State& state = model.mutableState();
    clearBoard(state);
    for (int row = 0; row < SIDE; ++row) state.wall[Model::cell(6, row)] = true;
    tg::Input tilt;
    tilt.tilt_x = tilt.tilt_y = 0.70710678f;  // full lean toward +x
    for (int i = 0; i < 240; ++i) {
        model.update(tilt, 1.0f / 60.0f);
        assert(state.x < 6.0f - BALL_RADIUS + 0.001f);
    }
    assert(state.touched[Model::cell(6, 3)]);

    Model holes = playingTilt();
    State& h = holes.mutableState();
    clearBoard(h);
    h.hole[Model::cell(5, 3)] = true;
    unsigned events = NO_EVENT;
    for (int i = 0; i < 120 && !(events & LIFE_LOST); ++i) events |= holes.update(tilt, 1.0f / 60.0f);
    assert(events & LIFE_LOST);
}

void framePartitionsMatch() {
    // Same total time in different frame sizes gives the same motion.
    Model a = playing();
    Model b = playing();
    clearBoard(a.mutableState());
    clearBoard(b.mutableState());
    tg::Input lean;
    lean.left.delta = 12;
    lean.right.delta = 20;
    a.update(lean, 0.0f);
    b.update(lean, 0.0f);
    const tg::Input hold{};
    for (int i = 0; i < 40; ++i) a.update(hold, 1.0f / 120.0f);
    const float sizes[] = {3.0f / 120.0f, 1.0f / 120.0f, 5.0f / 120.0f, 2.0f / 120.0f};
    int steps = 0;
    for (int i = 0; steps < 40; ++i) {
        const int n = std::min(static_cast<int>(std::lround(sizes[i % 4] * 120.0f)), 40 - steps);
        b.update(hold, n / 120.0f);
        steps += n;
    }
    assert(std::fabs(a.state().x - b.state().x) < 0.0005f);
    assert(std::fabs(a.state().y - b.state().y) < 0.0005f);
    assert(std::fabs(a.state().vx - b.state().vx) < 0.0005f);
}

void startFlowAndCalibration() {
    State initial_view;
    assert(Model::stackDepthLayersForView(initial_view) == LAYERS_PER_LEVEL);

    State dropping_view;
    dropping_view.stage = Stage::LayerDrop;
    dropping_view.layer = LAYERS_PER_LEVEL - 1;
    dropping_view.timer = 0.85f;
    assert(std::fabs(Model::stackDepthLayersForView(dropping_view) -
                     LAYERS_PER_LEVEL) < 0.0001f);
    dropping_view.timer = 0.425f;
    assert(std::fabs(Model::stackDepthLayersForView(dropping_view) -
                     (LAYERS_PER_LEVEL - 0.5f)) < 0.0001f);
    dropping_view.timer = 0.0f;
    assert(Model::stackDepthLayersForView(dropping_view) == LAYERS_PER_LEVEL - 1);

    Model tilt(9);
    tg::Input in;
    in.tilt_x = 0.25f;
    in.tilt_y = -0.5f;
    in.click = true;
    tilt.update(in, 0.01f);
    assert(tilt.state().stage == Stage::Calibration);
    tilt.update(in, 0.01f);
    assert(tilt.state().stage == Stage::Playing);
    assert(std::fabs(tilt.state().calibration_x - 0.25f) < 0.0001f);
    assert(std::fabs(tilt.state().calibration_y + 0.5f) < 0.0001f);

    float x = 0.0f;
    float y = 0.0f;
    Model::tiltAcceleration(1.0f, 0.0f, x, y);
    assert(std::fabs(x - 0.7071067f) < 0.0001f);
    assert(std::fabs(y + 0.7071067f) < 0.0001f);

    Model playtest(9);
    tg::Input hidden_start;
    hidden_start.main.delta = 1234;
    playtest.update(hidden_start, 0.01f);
    tg::Input select;
    select.click = true;
    playtest.update(select, 0.01f);
    assert(playtest.state().level == 6);
    assert(playtest.state().monster_pending);

    Model lower_layer(9);
    hidden_start.main.delta = 1233;
    lower_layer.update(hidden_start, 0.01f);
    lower_layer.update(select, 0.01f);
    assert(lower_layer.state().stage == Stage::Playing);
    assert(lower_layer.state().level == 1);
    assert(lower_layer.state().layer == 4);

    for (int shortcut : {1235, 1236}) {
        Model lesson(9);
        hidden_start.main.delta = shortcut;
        lesson.update(hidden_start, 0.01f);
        lesson.update(select, 0.01f);
        assert(lesson.state().level == 1);
        assert(lesson.state().layer == (shortcut == 1235 ? 2 : 1));
        assert(lesson.state().monster_alive);
        assert(bonusCount(lesson.state(), Bonus::Laser) == 1);
    }
}

void twistAngleAndFixedSteps() {
    Model model = playing();
    tg::Input in;
    in.left.delta = 20;
    model.update(in, 0.0f);
    assert(std::fabs(model.state().left_angle - 20.0f) < 0.0001f);

    in.left.delta = -20;
    model.update(in, 0.0f);
    assert(std::fabs(model.state().left_angle) < 0.0001f);

    in.left.delta = -20;
    model.update(in, 0.0f);
    assert(std::fabs(model.state().left_angle + 20.0f) < 0.0001f);

    in.left.delta = 20;
    in.right.delta = 20;
    model.update(in, 0.0f);
    assert(std::fabs(model.state().left_angle) < 0.0001f);
    assert(std::fabs(model.state().right_angle - 20.0f) < 0.0001f);

    in.right.delta = -20;
    model.update(in, 0.0f);
    assert(std::fabs(model.state().right_angle) < 0.0001f);

    Model physics = playing();
    in = tg::Input{};
    in.left.delta = 20;
    physics.update(in, 0.1f);
    assert(std::fabs(physics.state().left_angle - 20.0f) < 0.0001f);
    assert(std::fabs(physics.state().x - 3.5f) < 0.0001f);
    assert(physics.state().y < 3.5f);

    float x = 0.0f;
    float y = 0.0f;
    Model::twistAcceleration(20.0f, 0.0f, x, y);
    assert(std::fabs(x) < 0.0001f && y < 0.0f);
    Model::twistAcceleration(-20.0f, 0.0f, x, y);
    assert(std::fabs(x) < 0.0001f && y > 0.0f);
    Model::twistAcceleration(0.0f, 20.0f, x, y);
    assert(x > 0.0f && std::fabs(y) < 0.0001f);
    Model::twistAcceleration(0.0f, -20.0f, x, y);
    assert(x < 0.0f && std::fabs(y) < 0.0001f);

    Model a = playingTilt();
    Model b = playingTilt();
    tg::Input tilt;
    tilt.tilt_y = 0.4f;
    a.update(tilt, 0.05f);
    for (int i = 0; i < 6; ++i) b.update(tilt, 1.0f / 120.0f);
    assert(std::fabs(a.state().x - b.state().x) < 0.001f);
    assert(std::fabs(a.state().y - b.state().y) < 0.001f);
}

void generatedBoardsAreSolvableAndProgress() {
    Model model = playing();
    for (int level = 1; level <= 6; ++level) {
        bool saw_edge_hole = false;
        for (int layer = 0; layer < LAYERS_PER_LEVEL; ++layer) {
            assert(Model::solvable(model.state()));
            State& state = model.mutableState();
            if (level >= 3) {
                for (int i = 0; i < CELLS; ++i) {
                    if (state.hole[i] &&
                        (i % SIDE == 0 || i % SIDE == SIDE - 1 ||
                         i / SIDE == 0 || i / SIDE == SIDE - 1)) saw_edge_hole = true;
                }
            }
            state.score = 0;
            for (int i = 0; i < CELLS; ++i) {
                if (!state.hole[i]) state.touched[i] = true;
            }
            model.update(tg::Input{}, STEP);
            assert(model.state().stage == Stage::BeamUp);
            assert(state.score == static_cast<std::uint32_t>(
                100 * level + (state.layer == 1 ? 1000 * level : 0)));
            assert(state.level == level);
            const int previous_layer = state.layer;
            advance(model, BEAM_SECONDS - STEP);
            assert(state.stage == Stage::BeamUp && state.layer == previous_layer);
            advance(model, STEP);
            assert(state.stage == (previous_layer == 1 ? Stage::LevelClear : Stage::LayerDrop));
            if (state.stage == Stage::LayerDrop) {
                for (bool touched : state.touched) assert(!touched);
                assert(state.safety > 0.0f);
            }
            const int ticks = state.stage == Stage::LevelClear ? 140 : 110;
            for (int i = 0; i < ticks; ++i) model.update(tg::Input{}, 0.01f);
        }
        if (level >= 3) assert(saw_edge_hole);
        assert(model.state().level == level + 1);
        assert(model.state().layer == LAYERS_PER_LEVEL);
        assert(model.state().stage == Stage::Playing);
    }

    Model a = playing();
    Model b = playing();
    for (int i = 0; i < 64; ++i) {
        assert(a.state().wall[i] == b.state().wall[i]);
        assert(a.state().hole[i] == b.state().hole[i]);
        assert(a.state().bonus[i] == b.state().bonus[i]);
    }
}

void wallBouncesAndEdgeHoleUsesTractor() {
    Model model = playing();
    State& state = model.mutableState();
    const int wall = Model::cell(3, 2);
    state.wall[wall] = true;
    state.x = 2.5f;
    state.y = 2.5f;
    state.vx = 2.0f;
    state.vy = 0.0f;
    for (int i = 0; i < 80; ++i) model.update(tg::Input{}, STEP);
    assert(state.touched[wall]);
    assert(state.vx < 0.0f);

    state.wall[wall] = false;
    state.tractor = true;
    state.hole[Model::cell(0, 3)] = true;
    state.x = 0.5f;
    state.y = 3.5f;
    state.vx = state.vy = 0.0f;
    const int lives = state.lives;
    assert(model.update(tg::Input{}, STEP) == NO_EVENT);
    assert(state.stage == Stage::Falling && state.fall_hole && state.fall_edge);
    assert(state.tractor);
    const unsigned fall = advance(model, FALL_SECONDS);
    assert(fall & BONUS_USED);
    assert(!(fall & LIFE_LOST));
    assert(state.stage == Stage::TractorSave);
    assert(state.lives == lives);
}

void monsterContactCostsALife() {
    Model model = playing();
    State& state = model.mutableState();
    state.safety = 0.0f;
    state.monster_alive = true;
    state.monster_x = state.x + 0.2f;
    state.monster_y = state.y;
    const unsigned event = model.update(tg::Input{}, STEP);
    assert(event & LIFE_LOST);
    assert(state.lives == START_LIVES - 1);
    assert(state.monster_alive);
    assert(state.monster_pause > 0.0f);
}

void bonusesAndLifeRules() {
    Model model = playing();
    State& state = model.mutableState();
    const int center = Model::cell(3, 3);
    state.lives = 4;
    state.bonus[center] = Bonus::Life;
    model.update(tg::Input{}, STEP);
    assert(state.touched[center]);
    assert(state.lives == 5);
    assert(state.score == 10);

    state.bonus[center] = Bonus::Boing;
    state.touched[center] = false;
    model.update(tg::Input{}, STEP);
    assert(state.airborne > 0.9f);

    state.airborne = 0.0f;
    state.tractor = true;
    state.hole[center] = true;
    state.x = 3.5f;
    state.y = 3.5f;
    model.update(tg::Input{}, STEP);
    assert(state.stage == Stage::Falling);
    const unsigned before_inner = advance(model, FALL_SECONDS);
    assert(before_inner & LIFE_LOST);
    assert(!(before_inner & BONUS_USED));
    assert(state.lives == 4);
    assert(state.tractor);

    state.hole[center] = false;
    state.x = -0.1f;
    state.y = 3.5f;
    model.update(tg::Input{}, STEP);
    const unsigned saved = advance(model, FALL_SECONDS);
    assert(saved & BONUS_USED);
    assert(!(saved & LIFE_LOST));
    assert(!state.tractor);
    assert(state.stage == Stage::TractorSave);
    for (int i = 0; i < 100; ++i) model.update(tg::Input{}, 0.01f);
    assert(state.stage == Stage::Playing);
    assert(state.lives == 4);
}

void laserHitAndGameOverRestart() {
    Model model = playing();
    State& state = model.mutableState();
    state.x = 2.5f;
    state.y = 2.5f;
    state.vx = 1.0f;
    state.vy = 0.0f;
    state.laser_armed = true;
    state.monster_alive = true;
    state.monster_x = 4.5f;
    state.monster_y = 2.5f;
    tg::Input click;
    click.click = true;
    const unsigned hit = model.update(click, STEP);
    assert(hit & MONSTER_KILLED);
    assert(state.score >= 500);
    assert(!state.monster_alive && !state.laser_armed);

    state.lives = 1;
    state.x = -0.1f;
    state.y = 3.5f;
    model.update(tg::Input{}, STEP);
    assert(state.stage == Stage::Falling);
    const unsigned lost = advance(model, FALL_SECONDS);
    assert(lost & GAME_OVER);
    assert(state.stage == Stage::Over);
    assert(state.lives == 0);
    assert(advance(model, 0.5f) == NO_EVENT);
    assert(state.stage == Stage::Over && state.lives == 0);
    tg::Input restart;
    restart.click = true;
    model.update(restart, 0.01f);
    assert(state.stage == Stage::ModeSelect);
    assert(state.score >= 500);
}

bool hasFeedback(const State& state, Message message, float x, float y) {
    for (const Feedback& item : state.feedback) {
        if (item.message == message && item.remaining > 0.0f &&
            item.x == x && item.y == y) return true;
    }
    return false;
}

void laserSweepsWithMovementAndHitsOnce() {
    tg::Input click;
    click.click = true;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            Model model = playing();
            State& s = model.mutableState();
            clearBoard(s);
            s.vx = static_cast<float>(dx);
            s.vy = static_cast<float>(dy);
            s.laser_armed = true;
            model.update(click, 0.0f);
            const float ray_x = s.laser_x2 - s.laser_x1;
            const float ray_y = s.laser_y2 - s.laser_y1;
            assert(std::fabs(std::hypot(ray_x, ray_y) - 128.0f) < 0.001f);
            assert(ray_x * dx + ray_y * dy > 0.0f);
            assert(std::fabs(ray_x * dy - ray_y * dx) < 0.001f);
        }
    }

    Model model = playing();
    State& s = model.mutableState();
    clearBoard(s);
    s.laser_armed = true;
    s.vx = 0.06f;
    model.update(click, 0.0f);
    assert(s.laser_x2 > s.laser_x1);
    tg::Input turn;
    turn.right.delta = -20;
    model.update(turn, 0.05f);
    assert(s.vx < 0.0f && s.laser_x2 < s.laser_x1);
    assert(s.laser_x1 == s.x && s.laser_y1 == s.y);
    s.monster_alive = true;
    s.monster_x = s.x - 1.0f;
    s.monster_y = s.y;
    const float hit_x = s.monster_x, hit_y = s.monster_y;
    const std::uint32_t before = s.score;
    assert(model.update(tg::Input{}, STEP) & MONSTER_KILLED);
    assert(s.score == before + 500);
    assert(!s.monster_alive && !s.monster_pending && !s.laser_armed);
    assert(hasFeedback(s, Message::Kill, hit_x, hit_y));
    assert(s.burst_x == hit_x && s.burst_y == hit_y && s.monster_burst == BURST_SECONDS);
    assert(!(model.update(click, STEP) & MONSTER_KILLED));
    advance(model, LASER_SECONDS);
    assert(s.laser_flash == 0.0f && s.monster_burst > 0.0f);
    advance(model, BURST_SECONDS);
    assert(s.monster_burst == 0.0f && !s.monster_alive);
    advance(model, FEEDBACK_SECONDS);
    for (const Feedback& item : s.feedback) assert(item.remaining == 0.0f);

    Model contact = playing();
    State& c = contact.mutableState();
    clearBoard(c);
    c.x = 6.5f;
    c.vx = 1.0f;
    c.monster_alive = true;
    c.monster_x = 6.3f;
    c.monster_y = c.y;
    c.laser_armed = true;
    const unsigned collided = contact.update(click, STEP);
    assert((collided & LIFE_LOST) && !(collided & MONSTER_KILLED));
    assert(c.x == 3.5f && c.laser_flash == 0.0f);
    assert(!(contact.update(tg::Input{}, STEP) & MONSTER_KILLED));
    assert(c.monster_alive);  // Respawning must not move a shot onto the alien.
}

void pickupAndSaveFeedbackIsMeaningfulAndExpires() {
    const int center = Model::cell(3, 3);
    for (const auto& pair : {std::pair<Bonus, Message>{Bonus::Life, Message::Life},
                            {Bonus::Zone, Message::Zone}, {Bonus::Boing, Message::Boing},
                            {Bonus::Quake, Message::Quake}, {Bonus::Tractor, Message::Tractor},
                            {Bonus::Laser, Message::Laser}}) {
        Model model = playing();
        State& s = model.mutableState();
        s.bonus[center] = pair.first;
        model.update(tg::Input{}, STEP);
        assert(hasFeedback(s, pair.second, 3.5f, 3.5f));
        assert(s.bonus[center] == Bonus::None);
        if (pair.first == Bonus::Zone) {
            for (int y = 2; y <= 4; ++y) {
                for (int x = 2; x <= 4; ++x) assert(s.touched[Model::cell(x, y)]);
            }
            assert(s.x == 3.5f && s.y == 3.5f);  // Zone does not teleport.
        }
    }
    assert(std::strcmp(Model::messageText(Message::Life), "+1 LIFE") == 0);
    assert(std::strcmp(Model::messageText(Message::Zone), "ZONE") == 0);
    Model full = playing();
    State& f = full.mutableState();
    f.lives = MAX_LIVES;
    f.bonus[center] = Bonus::Life;
    full.update(tg::Input{}, STEP);
    assert(f.lives == MAX_LIVES && hasFeedback(f, Message::LifeFull, 3.5f, 3.5f));

    Model chain = playing();
    State& c = chain.mutableState();
    c.bonus[center] = Bonus::Zone;
    c.bonus[Model::cell(2, 2)] = Bonus::Life;
    chain.update(tg::Input{}, STEP);
    assert(hasFeedback(c, Message::Zone, 3.5f, 3.5f));
    assert(hasFeedback(c, Message::Life, 2.5f, 2.5f));

    Model pooled = playing();
    State& p = pooled.mutableState();
    for (int i = 0; i < FEEDBACK_SLOTS + 4; ++i) {
        p.bonus[center] = Bonus::Tractor;
        p.touched[center] = false;
        pooled.update(tg::Input{}, STEP);
    }
    for (const Feedback& item : p.feedback) {
        assert(item.message == Message::Tractor);
        assert(item.remaining > 0.0f && item.remaining <= FEEDBACK_SECONDS);
    }
    clearLayer(pooled);
    for (const Feedback& item : p.feedback) assert(item.remaining == 0.0f);
    assert(p.monster_burst == 0.0f && p.laser_flash == 0.0f);

    for (bool saved : {false, true}) {
        Model model = playing();
        State& s = model.mutableState();
        s.tractor = saved;
        s.x = -0.01f;
        model.update(tg::Input{}, STEP);
        const float x = s.x, y = s.y;
        advance(model, FALL_SECONDS);
        assert(hasFeedback(s, saved ? Message::Saved : Message::Lost, x, y));
        assert(s.lives == START_LIVES - (saved ? 0 : 1));
        advance(model, FEEDBACK_SECONDS + STEP);
        for (const Feedback& item : s.feedback) assert(item.remaining == 0.0f);
    }
}

void fallAnimationPrecedesExactlyOneLifeLoss() {
    for (bool hole : {false, true}) {
        Model model = playing();
        State& s = model.mutableState();
        clearBoard(s);
        s.x = hole ? 2.2f : 8.01f;
        s.y = 3.5f;
        s.vx = 1.0f;
        s.hole[Model::cell(2, 3)] = hole;
        s.monster_alive = true;
        s.monster_x = s.x;
        s.monster_y = s.y;
        const unsigned start = model.update(tg::Input{}, STEP);
        assert(start == NO_EVENT);
        assert(s.stage == Stage::Falling && s.fall_hole == hole);
        assert(s.lives == START_LIVES && s.safety == 0.0f);
        const float x = s.x, y = s.y;
        assert(advance(model, FALL_SECONDS - STEP) == NO_EVENT);
        assert(s.stage == Stage::Falling && s.x == x && s.y == y);
        assert(s.lives == START_LIVES);
        const unsigned end = advance(model, STEP);
        assert((end & LIFE_LOST) && !(end & GAME_OVER));
        assert(s.stage == Stage::Playing && s.lives == START_LIVES - 1);
        assert(s.x == 3.5f && s.y == 3.5f && s.safety == 1.5f);
        assert(!(advance(model, 0.5f) & LIFE_LOST));
        assert(s.lives == START_LIVES - 1);
    }

    Model saved = playing();
    State& s = saved.mutableState();
    s.x = -0.01f;
    s.tractor = true;
    s.lives = 1;
    saved.update(tg::Input{}, STEP);
    assert(s.stage == Stage::Falling && s.tractor);
    assert(advance(saved, FALL_SECONDS) == BONUS_USED);
    assert(s.stage == Stage::TractorSave && s.lives == 1 && !s.tractor);
    advance(saved, 0.9f);
    assert(s.stage == Stage::Playing && s.safety > 1.3f && s.lives == 1);
}

void animationFramePartitionsMatch() {
    for (bool clearing : {false, true}) {
        Model a = playing();
        Model b = playing();
        for (Model* model : {&a, &b}) {
            State& s = model->mutableState();
            clearBoard(s);
            if (clearing) {
                for (bool& touched : s.touched) touched = true;
            } else {
                s.x = 8.01f;
            }
            model->update(tg::Input{}, STEP);
        }
        const float duration = clearing ? BEAM_SECONDS : FALL_SECONDS;
        unsigned events_a = advance(a, duration);
        unsigned events_b = NO_EVENT;
        for (int i = 0; i < static_cast<int>(std::lround(duration * 10)); ++i) {
            events_b |= b.update(tg::Input{}, 0.1f);
        }
        assert(events_a == events_b);
        assert(a.state().stage == b.state().stage);
        assert(a.state().lives == b.state().lives && a.state().layer == b.state().layer);
        assert(std::fabs(a.state().timer - b.state().timer) < 0.0001f);
        assert(std::fabs(a.state().safety - b.state().safety) < 0.0001f);
        assert(a.state().score == b.state().score);
    }
}

}  // namespace

int main() {
    tutorialLessonsFollowPlayOrder();
    tutorialHoleAndWallUseNormalRules();
    laserLessonAndFinaleAreLive();
    laterLevelFeatureThresholdsStayUnchanged();
    fallAnimationPrecedesExactlyOneLifeLoss();
    animationFramePartitionsMatch();
    startFlowAndCalibration();
    twistAngleAndFixedSteps();
    rollingIsResponsiveAndBrakes();
    fastBallStillCollides();
    framePartitionsMatch();
    generatedBoardsAreSolvableAndProgress();
    bonusesAndLifeRules();
    wallBouncesAndEdgeHoleUsesTractor();
    monsterContactCostsALife();
    laserHitAndGameOverRestart();
    laserSweepsWithMovementAndHitsOnce();
    pickupAndSaveFeedbackIsMeaningfulAndExpires();
    std::puts("tiltblock model passed");
}
