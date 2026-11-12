#pragma once

#include <lvgl.h>

#include "draw_frame.h"
#include "presentation_text.h"

namespace os::lua_presenter {

struct Viewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    tg::engine::WorldSize world;
};

class Presenter {
public:
    explicit Presenter(lv_obj_t* parent, const Viewport& viewport);

    bool ready() const { return ready_; }
    const tg::engine::WorldSize& worldSize() const { return viewport_.world; }
    void present(const tg::engine::DrawFrame& frame);
    void hide();

private:
    bool validViewport(const Viewport& viewport) const;
    void configure(lv_obj_t* object);

    lv_obj_t* objects_[tg::engine::DrawFrame::MAX_COMMANDS] = {};
    tg::engine::PresentationText text_;
    Viewport viewport_;
    const lv_font_t* text_font_ = nullptr;
    double scale_ = 0.0;
    double offset_x_ = 0.0;
    double offset_y_ = 0.0;
    bool ready_ = false;
};

}  // namespace os::lua_presenter
