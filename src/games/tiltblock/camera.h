#pragma once

#include <algorithm>

#include "framebuffer.h"
#include "model.h"

namespace tiltblock {

inline constexpr float HALF_X = 11.25f;
inline constexpr float HALF_Y = 5.625f;
inline constexpr float CUBE_HEIGHT = HALF_X * 1.11803398875f;
inline constexpr float BOARD_X = 160.0f;
inline constexpr float BOARD_Y = 82.0f;
inline constexpr float SCENE_ZOOM = 1.25f;
inline constexpr float SCENE_Y = 93.985f;
// Legal walls exclude corner cells. At zoom 1.25, the backmost raised wall
// and front-edge shadow bound tilt to 0.038824 with 3px shake + 1.25px raster.
inline constexpr float VISUAL_TILT = 0.0388f;

inline float layerDropForView(const State& state) {
    return state.stage == Stage::LayerDrop
        ? std::clamp(state.timer / 0.85f, 0.0f, 1.0f) * CUBE_HEIGHT : 0.0f;
}

inline fb::Point boardPoint(const State& state, float x, float y, float z = 0.0f) {
    const float col = x - SIDE / 2.0f;
    const float row = y - SIDE / 2.0f;
    const float tilt = (-state.right_angle * col + state.left_angle * row) * VISUAL_TILT;
    return {BOARD_X + (col - row) * HALF_X,
            BOARD_Y + (col + row) * HALF_Y - tilt - z + layerDropForView(state)};
}

// Magnification and the play pivot never follow instantaneous scene bounds.
// Only the deliberate layer-drop translation is followed, as before.
struct Camera {
    float zoom = SCENE_ZOOM;
    float center_y = BOARD_Y;

    fb::Point project(fb::Point p) const {
        return {BOARD_X + (p.x - BOARD_X) * zoom,
                SCENE_Y + (p.y - center_y) * zoom};
    }

    float hopLift(fb::Point floor, float desired) const {
        // Keep the 4px-radius ball clear of the HUD, even during screen shake.
        const float minimum_center = 18.0f + 3.0f + 1.25f + 4.0f * zoom;
        return std::clamp(desired, 0.0f,
                          std::max(0.0f, (project(floor).y - minimum_center) / zoom));
    }

    static Camera fit(const State& state) {
        return {SCENE_ZOOM, BOARD_Y + layerDropForView(state)};
    }
};

}  // namespace tiltblock
