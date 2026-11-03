#pragma once

#include <algorithm>
#include <cmath>

#include "framebuffer.h"
#include "model.h"

namespace tiltblock {

inline constexpr float HALF_X = 11.25f;
inline constexpr float HALF_Y = 5.625f;
inline constexpr float CUBE_HEIGHT = HALF_X * 1.11803398875f;
inline constexpr float BOARD_X = 160.0f;
inline constexpr float BOARD_Y = 82.0f;

inline fb::Point boardPoint(const State& state, float x, float y, float z = 0.0f) {
    const float col = x - SIDE / 2.0f;
    const float row = y - SIDE / 2.0f;
    const float tilt = (-state.right_angle * col + state.left_angle * row) * 0.15f;
    const float drop = state.stage == Stage::LayerDrop
        ? std::clamp(state.timer / 0.85f, 0.0f, 1.0f) * CUBE_HEIGHT : 0.0f;
    return {BOARD_X + (col - row) * HALF_X,
            BOARD_Y + (col + row) * HALF_Y - tilt - z + drop};
}

// Frame the playable surface, not the fading stack or departing effect rays.
// Uniform zoom keeps every scene primitive in the same camera space.
struct Camera {
    float zoom = 1.0f;
    float center_y = BOARD_Y;

    fb::Point project(fb::Point p) const {
        return {BOARD_X + (p.x - BOARD_X) * zoom,
                92.0f + (p.y - center_y) * zoom};
    }

    static Camera fit(const State& state) {
        float top = boardPoint(state, 0.0f, 0.0f).y;
        float bottom = top;
        auto include = [&](float x, float y, float z = 0.0f) {
            const float py = boardPoint(state, x, y, z).y;
            top = std::min(top, py);
            bottom = std::max(bottom, py);
        };
        for (int y : {0, SIDE})
            for (int x : {0, SIDE}) include(x, y);
        for (int row = 0; row < SIDE; ++row) {
            for (int col = 0; col < SIDE; ++col) {
                if (!state.wall[row * SIDE + col]) continue;
                for (int y : {row, row + 1})
                    for (int x : {col, col + 1}) include(x, y, CUBE_HEIGHT);
            }
        }
        // A hop can lift the ball above a back edge; keep it below the HUD.
        if (state.airborne > 0.0f) {
            include(std::clamp(state.x, 0.0f, float(SIDE)),
                    std::clamp(state.y, 0.0f, float(SIDE)), 15.0f);
        }
        // Six scene pixels cover actors; screen margins cover shake and falls.
        top -= 6.0f;
        bottom += 6.0f;
        return {std::min(1.5f, 144.0f / (bottom - top)), (top + bottom) * 0.5f};
    }
};

}  // namespace tiltblock
