#pragma once

#include <algorithm>
#include <cmath>

namespace tg::engine {

struct CameraBounds {
    float world_width;
    float world_height;
    float view_width;
    float view_height;
    float dead_left;
    float dead_top;
    float dead_right;
    float dead_bottom;
};

class Camera {
public:
    bool configure(const CameraBounds& bounds) {
        if (!finite(bounds.world_width) || !finite(bounds.world_height) ||
            !finite(bounds.view_width) || !finite(bounds.view_height) ||
            !finite(bounds.dead_left) || !finite(bounds.dead_top) ||
            !finite(bounds.dead_right) || !finite(bounds.dead_bottom) ||
            bounds.world_width <= 0 || bounds.world_height <= 0 ||
            bounds.view_width <= 0 || bounds.view_height <= 0 ||
            bounds.dead_left < 0 || bounds.dead_top < 0 ||
            bounds.dead_right < bounds.dead_left || bounds.dead_bottom < bounds.dead_top ||
            bounds.dead_right > bounds.view_width || bounds.dead_bottom > bounds.view_height) {
            return false;
        }
        bounds_ = bounds;
        configured_ = true;
        clamp();
        return true;
    }

    bool reset(float x = 0, float y = 0) {
        if (!configured_ || !finite(x) || !finite(y)) return false;
        x_ = x;
        y_ = y;
        clamp();
        return true;
    }

    bool follow(float x, float y) {
        if (!configured_ || !finite(x) || !finite(y)) return false;
        if (x < x_ + bounds_.dead_left) x_ = x - bounds_.dead_left;
        else if (x > x_ + bounds_.dead_right) x_ = x - bounds_.dead_right;
        if (y < y_ + bounds_.dead_top) y_ = y - bounds_.dead_top;
        else if (y > y_ + bounds_.dead_bottom) y_ = y - bounds_.dead_bottom;
        clamp();
        return true;
    }

    float x() const { return x_; }
    float y() const { return y_; }

private:
    static bool finite(float value) { return std::isfinite(value); }
    void clamp() {
        x_ = std::clamp(x_, 0.0f, std::max(0.0f, bounds_.world_width - bounds_.view_width));
        y_ = std::clamp(y_, 0.0f, std::max(0.0f, bounds_.world_height - bounds_.view_height));
    }

    CameraBounds bounds_{};
    float x_ = 0;
    float y_ = 0;
    bool configured_ = false;
};

}  // namespace tg::engine
