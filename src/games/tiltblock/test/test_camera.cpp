#include <cassert>
#include <cmath>
#include <cstdio>

#include "camera.h"

using namespace tiltblock;

void fitsSurface(const State& state) {
    const Camera camera = Camera::fit(state);
    for (int row = 0; row <= SIDE; ++row) {
        for (int col = 0; col <= SIDE; ++col) {
            const auto p = camera.project(boardPoint(state, col, row));
            assert(p.x >= 12.0f && p.x <= 308.0f);
            assert(p.y >= 24.0f && p.y <= 164.0f);
        }
    }
    for (int row = 0; row < SIDE; ++row) {
        for (int col = 0; col < SIDE; ++col) {
            if (!state.wall[row * SIDE + col]) continue;
            for (int y : {row, row + 1}) {
                for (int x : {col, col + 1}) {
                    const auto p = camera.project(boardPoint(state, x, y, CUBE_HEIGHT));
                    assert(p.y >= 24.0f && p.y <= 164.0f);
                }
            }
        }
    }
}

void stableScale(const State& state) {
    const Camera camera = Camera::fit(state);
    assert(std::fabs(camera.zoom - SCENE_ZOOM) < 1e-6f);
    const auto left = camera.project(boardPoint(state, 0, SIDE));
    const auto right = camera.project(boardPoint(state, SIDE, 0));
    assert(std::fabs(right.x - left.x - 225.0f) < 0.01f);
    const auto pivot = camera.project(boardPoint(state, 4, 4));
    assert(std::fabs(pivot.x - BOARD_X) < 0.001f);
    assert(std::fabs(pivot.y - SCENE_Y) < 0.001f);
    const auto raised = camera.project(boardPoint(state, 4, 4, CUBE_HEIGHT));
    assert(std::fabs(pivot.y - raised.y - CUBE_HEIGHT * SCENE_ZOOM) < 0.001f);
    // Actors, beam widths and sprites use these same scene-pixel offsets.
    const auto a = camera.project({150, 70});
    const auto b = camera.project({160, 78});
    assert(std::fabs(b.x - a.x - 10 * SCENE_ZOOM) < 0.001f);
    assert(std::fabs(b.y - a.y - 8 * SCENE_ZOOM) < 0.001f);
    State neutral = state;
    neutral.left_angle = neutral.right_angle = 0;
    for (int row = 0; row <= SIDE; ++row) {
        for (int col = 0; col <= SIDE; ++col) {
            assert(std::fabs(boardPoint(state, col, row).x -
                             boardPoint(neutral, col, row).x) < 0.001f);
        }
    }
}

void stateAndEffectInvariance() {
    State state;
    for (Stage stage : {Stage::Playing, Stage::Falling, Stage::BeamUp, Stage::LayerDrop,
                        Stage::LevelClear, Stage::TractorSave, Stage::Over}) {
        state.stage = stage;
        for (int layer = 1; layer <= LAYERS_PER_LEVEL; ++layer) {
            state.layer = layer;
            for (float timer : {0.0f, 0.425f, 0.85f}) {
                state.timer = timer;
                state.airborne = 0.5f;
                state.earthquake = 2.0f;
                state.laser_flash = LASER_SECONDS;
                state.monster_burst = BURST_SECONDS;
                state.monster_alive = true;
                state.tractor = true;
                state.feedback[0] = {Message::Zone, 0, 0, FEEDBACK_SECONDS};
                stableScale(state);
                fitsSurface(state);
            }
        }
    }
}

int main() {
    static_assert(SIDE == 8 && CELLS == 64 && LAYERS_PER_LEVEL == 8);
    State state;
    state.stage = Stage::Playing;
    const Camera camera = Camera::fit(state);
    const auto left = camera.project(boardPoint(state, 0, SIDE));
    const auto right = camera.project(boardPoint(state, SIDE, 0));
    assert(right.x - left.x >= 225.0f);  // Original pre-zoom board: 180px.
    assert(std::fabs((right.x + left.x) * 0.5f - 160.0f) < 0.001f);
    const auto center = camera.project(boardPoint(state, 4, 4));
    const auto raised = camera.project(boardPoint(state, 4, 4, CUBE_HEIGHT));
    assert(std::fabs(center.y - raised.y - CUBE_HEIGHT * camera.zoom) < 0.001f);
    State tilted = state;
    tilted.left_angle = 20;
    tilted.right_angle = -20;
    assert(std::fabs(camera.project(boardPoint(tilted, SIDE, SIDE)).y -
                     camera.project(boardPoint(state, SIDE, SIDE)).y) >= 2.9f);
    for (bool walls : {false, true}) {
        for (bool& wall : state.wall) wall = walls;
        for (int left_angle = -20; left_angle <= 20; ++left_angle) {
            for (int right_angle = -20; right_angle <= 20; ++right_angle) {
                state.left_angle = left_angle;
                state.right_angle = right_angle;
                stableScale(state);
                fitsSurface(state);
                for (int y : {0, SIDE}) {
                    for (int x : {0, SIDE}) {
                        state.x = x;
                        state.y = y;
                        state.airborne = 0.5f;
                        const Camera hop = Camera::fit(state);
                        assert(hop.project(boardPoint(state, x, y, 15)).y >= 24.0f - 0.001f);
                        stableScale(state);
                        fitsSurface(state);
                    }
                }
                state.airborne = 0;
                state.stage = Stage::LayerDrop;
                state.timer = 0.425f;
                fitsSurface(state);
                state.stage = Stage::Playing;
            }
        }
    }
    // Actors at the playable radius limit, including a hop and 3px shake,
    // stay below the 18px HUD and above the bottom screen edge.
    for (int angle : {-20, 20}) {
        state.left_angle = -angle;
        state.right_angle = angle;
        const Camera view = Camera::fit(state);
        for (float y : {BALL_RADIUS, SIDE - BALL_RADIUS}) {
            for (float x : {BALL_RADIUS, SIDE - BALL_RADIUS}) {
                const auto p = view.project(boardPoint(state, x, y));
                assert(p.x - 6 * view.zoom - 3 >= 0);
                assert(p.x + 6 * view.zoom + 3 < 320);
                assert(p.y - (15 + 4) * view.zoom - 3 >= 18);
                assert(p.y + 7 * view.zoom + 3 < 172);
            }
        }
    }
    stateAndEffectInvariance();
    std::puts("tiltblock camera tests passed");
}
