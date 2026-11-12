#include <cassert>
#include <limits>

#include "camera.h"

int main() {
    using tg::engine::Camera;
    using tg::engine::CameraBounds;
    Camera camera;
    assert(!camera.follow(0, 0));
    assert(!camera.reset());
    const CameraBounds bounds{100, 80, 20, 10, 5, 2, 15, 8};
    assert(camera.configure(bounds));
    assert(camera.follow(10, 5));
    assert(camera.x() == 0 && camera.y() == 0);
    assert(camera.follow(16.5f, 9.5f));
    assert(camera.x() == 1.5f && camera.y() == 1.5f);
    assert(camera.follow(6, 3));
    assert(camera.x() == 1 && camera.y() == 1);
    assert(camera.follow(100, 80));
    assert(camera.x() == 80 && camera.y() == 70);
    assert(camera.follow(-100, -100));
    assert(camera.x() == 0 && camera.y() == 0);
    assert(camera.reset(200, 200));
    assert(camera.x() == 80 && camera.y() == 70);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    assert(!camera.follow(nan, 0));
    assert(!camera.reset(0, nan));
    assert(!camera.configure({100, 80, 20, 10, 5, 2, 21, 8}));
    assert(camera.x() == 80 && camera.y() == 70);
    assert(camera.configure({8, 6, 20, 10, 5, 2, 15, 8}));
    assert(camera.x() == 0 && camera.y() == 0);
    assert(camera.follow(8, 6));
    assert(camera.x() == 0 && camera.y() == 0);
}
