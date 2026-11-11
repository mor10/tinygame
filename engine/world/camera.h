#pragma once

// The camera is the world position (in screen pixels) shown at the stage's
// top-left corner. It follows a target lazily: nothing moves while the target
// stays inside a dead zone in the middle of the view.

namespace tg {

struct Camera {
    int x = 0;
    int y = 0;
};

struct FollowRules {
    int view_w = 0, view_h = 0;    // stage size in pixels
    int world_w = 0, world_h = 0;  // level size in pixels
    int dead_w = 0, dead_h = 0;    // size of the central dead zone
};

// Moves the camera just enough to keep (target_x, target_y) inside the dead
// zone, then keeps it inside the level. A level smaller than the view is
// centred.
void follow(Camera& camera, int target_x, int target_y, const FollowRules& rules);

}  // namespace tg
