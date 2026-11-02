#include <cassert>
#include <cmath>
#include <cstdio>

#include "model.h"

using namespace tiltblock;

namespace {

Model playing(Mode mode = Mode::Twist) {
    Model model(81);
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
            assert(model.state().stage == Stage::LayerDrop ||
                   model.state().stage == Stage::LevelClear);
            assert(state.score == static_cast<std::uint32_t>(
                100 * level + (state.stage == Stage::LevelClear ? 1000 * level : 0)));
            const int ticks = model.state().stage == Stage::LevelClear ? 140 : 110;
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
    const unsigned fall = model.update(tg::Input{}, STEP);
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
    const unsigned before_inner = model.update(tg::Input{}, STEP);
    assert(before_inner & LIFE_LOST);
    assert(!(before_inner & BONUS_USED));
    assert(state.lives == 4);
    assert(state.tractor);

    state.hole[center] = false;
    state.x = -0.1f;
    state.y = 3.5f;
    const unsigned saved = model.update(tg::Input{}, STEP);
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
    const unsigned lost = model.update(tg::Input{}, STEP);
    assert(lost & GAME_OVER);
    assert(state.stage == Stage::Over);
    tg::Input restart;
    restart.click = true;
    model.update(restart, 0.01f);
    assert(state.stage == Stage::ModeSelect);
    assert(state.score >= 500);
}

}  // namespace

int main() {
    startFlowAndCalibration();
    twistAngleAndFixedSteps();
    generatedBoardsAreSolvableAndProgress();
    bonusesAndLifeRules();
    wallBouncesAndEdgeHoleUsesTractor();
    monsterContactCostsALife();
    laserHitAndGameOverRestart();
    std::puts("tiltblock model passed");
}
