#include "camera.h"

namespace tg {

namespace {
int followAxis(int camera, int target, int view, int world, int dead) {
    if (world <= view) return -((view - world) / 2);
    if (dead > view) dead = view;
    const int low = camera + (view - dead) / 2;
    const int high = low + dead;
    if (target < low) camera -= low - target;
    if (target > high) camera += target - high;
    if (camera < 0) camera = 0;
    if (camera > world - view) camera = world - view;
    return camera;
}
}  // namespace

void follow(Camera& camera, int target_x, int target_y, const FollowRules& rules) {
    camera.x = followAxis(camera.x, target_x, rules.view_w, rules.world_w, rules.dead_w);
    camera.y = followAxis(camera.y, target_y, rules.view_h, rules.world_h, rules.dead_h);
}

}  // namespace tg
