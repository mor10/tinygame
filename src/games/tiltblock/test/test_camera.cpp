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
            assert(p.y >= 24.0f && p.y <= 160.0f);
        }
    }
    for (int row = 0; row < SIDE; ++row) {
        for (int col = 0; col < SIDE; ++col) {
            if (!state.wall[row * SIDE + col]) continue;
            for (int y : {row, row + 1}) {
                for (int x : {col, col + 1}) {
                    const auto p = camera.project(boardPoint(state, x, y, CUBE_HEIGHT));
                    assert(p.y >= 24.0f && p.y <= 160.0f);
                }
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
    assert(right.x - left.x >= 250.0f);  // Previously 180px: at least 39% wider.
    assert(std::fabs((right.x + left.x) * 0.5f - 160.0f) < 0.001f);
    const auto center = camera.project(boardPoint(state, 4, 4));
    const auto raised = camera.project(boardPoint(state, 4, 4, CUBE_HEIGHT));
    assert(std::fabs(center.y - raised.y - CUBE_HEIGHT * camera.zoom) < 0.001f);
    for (bool walls : {false, true}) {
        for (bool& wall : state.wall) wall = walls;
        for (int left_angle : {-20, 0, 20}) {
            for (int right_angle : {-20, 0, 20}) {
                state.left_angle = left_angle;
                state.right_angle = right_angle;
                fitsSurface(state);
                for (int y : {0, SIDE}) {
                    for (int x : {0, SIDE}) {
                        state.x = x;
                        state.y = y;
                        state.airborne = 0.5f;
                        const Camera hop = Camera::fit(state);
                        assert(hop.project(boardPoint(state, x, y, 15)).y >= 24.0f);
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
    std::puts("tiltblock camera tests passed");
}
