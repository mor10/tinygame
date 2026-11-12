#pragma once

#include <cmath>

namespace tg::engine {

struct Aabb {
    double x;
    double y;
    double width;
    double height;
};

struct SweepResult {
    enum class Status {
        Miss,
        Hit,
        InitialContact,
        InvalidInput,
    };

    Status status = Status::Miss;
    double time = 0.0;
    double normal_x = 0.0;
    double normal_y = 0.0;
};

// Starting overlap or edge contact is reported separately for caller policy.
inline SweepResult sweepAabb(const Aabb& moving, double delta_x, double delta_y,
                             const Aabb& target) {
    const auto valid = [](const Aabb& box) {
        return std::isfinite(box.x) && std::isfinite(box.y) &&
               std::isfinite(box.width) && std::isfinite(box.height) &&
               box.width > 0.0 && box.height > 0.0;
    };
    if (!valid(moving) || !valid(target) ||
        !std::isfinite(delta_x) || !std::isfinite(delta_y)) {
        return {SweepResult::Status::InvalidInput};
    }

    const double moving_right = moving.x + moving.width;
    const double moving_bottom = moving.y + moving.height;
    const double expanded_left = target.x - moving.width;
    const double expanded_top = target.y - moving.height;
    const double target_right = target.x + target.width;
    const double target_bottom = target.y + target.height;
    if (!std::isfinite(moving_right) || !std::isfinite(moving_bottom) ||
        !std::isfinite(expanded_left) || !std::isfinite(expanded_top) ||
        !std::isfinite(target_right) || !std::isfinite(target_bottom)) {
        return {SweepResult::Status::InvalidInput};
    }
    if (moving_right >= target.x && moving.x <= target_right &&
        moving_bottom >= target.y && moving.y <= target_bottom) {
        return {SweepResult::Status::InitialContact};
    }

    double enter_x = -INFINITY;
    double exit_x = INFINITY;
    double enter_y = -INFINITY;
    double exit_y = INFINITY;
    double normal_x = 0.0;
    double normal_y = 0.0;

    const auto axis_times = [](double position, double delta, double minimum,
                               double maximum, double& enter, double& exit,
                               double& normal) {
        if (delta == 0.0) return position >= minimum && position <= maximum;
        if (delta > 0.0) {
            enter = (minimum - position) / delta;
            exit = (maximum - position) / delta;
            normal = -1.0;
        } else {
            enter = (maximum - position) / delta;
            exit = (minimum - position) / delta;
            normal = 1.0;
        }
        return true;
    };

    if (!axis_times(moving.x, delta_x, expanded_left, target_right,
                    enter_x, exit_x, normal_x) ||
        !axis_times(moving.y, delta_y, expanded_top, target_bottom,
                    enter_y, exit_y, normal_y)) {
        return {SweepResult::Status::Miss};
    }

    const double enter = enter_x > enter_y ? enter_x : enter_y;
    const double exit = exit_x < exit_y ? exit_x : exit_y;
    if (enter > exit || exit < 0.0 || enter < 0.0 || enter > 1.0) {
        return {SweepResult::Status::Miss};
    }
    const bool hit_x = enter_x == enter;
    const bool hit_y = enter_y == enter;
    return {SweepResult::Status::Hit, enter,
            hit_x ? normal_x : 0.0, hit_y ? normal_y : 0.0};
}

}  // namespace tg::engine
