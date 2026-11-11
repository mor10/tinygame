#pragma once

// A scrolling test level: the first thing that uses the stage layout, tile
// cache, parallax layers and camera together. A character walks back and
// forth along a level wider than the screen and the camera follows.
// It has no game rules; it exists to prove and fingerprint the renderer.

#include <cstdint>
#include <vector>

#include "../gfx/canvas.h"
#include "../world/camera.h"
#include "../world/stage.h"
#include "../world/tilemap.h"

namespace tg::demo {

class ScrollDemo {
public:
    static constexpr int LEVEL_COLS = 48;
    static constexpr int LEVEL_ROWS = STAGE_ROWS;
    static constexpr int SCALE = 2;

    bool init(int screen_w, int screen_h, View view, int palette);
    void tick();                       // one 60 Hz step
    void draw(gfx::Canvas& canvas) const;

    std::uint32_t ticks() const { return ticks_; }
    const Camera& camera() const { return camera_; }
    const StageLayout& layout() const { return layout_; }

private:
    int heroX() const { return hero_x16_ / 16; }  // world pixels
    void updateCamera();

    int palette_ = 0;
    StageLayout layout_;
    TileCache tiles_;
    std::vector<std::uint8_t> ground_;
    std::vector<std::uint8_t> sky_;
    Camera camera_;
    int hero_x16_ = 0;   // world x in 1/16 pixel, so speed is exact without floats
    int hero_dir_ = 1;
    std::uint32_t ticks_ = 0;
};

}  // namespace tg::demo
