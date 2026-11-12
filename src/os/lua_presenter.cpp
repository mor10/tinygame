#include "lua_presenter.h"

#include <algorithm>
#include <cmath>

#include "tg_style.h"

namespace os::lua_presenter {
namespace {

tg::Font fontForScale(double scale) {
    if (scale < 1.5f) return tg::Font::S;
    if (scale < 2.5f) return tg::Font::M;
    if (scale < 3.5f) return tg::Font::L;
    return tg::Font::XL;
}

int pixelFloor(double value, int lower, int upper) {
    if (value <= lower) return lower;
    if (value >= upper) return upper;
    return static_cast<int>(std::floor(value));
}

int pixelCeil(double value, int lower, int upper) {
    if (value <= lower) return lower;
    if (value >= upper) return upper;
    return static_cast<int>(std::ceil(value));
}

}  // namespace

Presenter::Presenter(lv_obj_t* parent, const Viewport& viewport) : viewport_(viewport) {
    if (parent == nullptr || !validViewport(viewport)) return;

    scale_ = std::min(static_cast<double>(viewport.width) / viewport.world.width,
                      static_cast<double>(viewport.height) / viewport.world.height);
    const double drawn_width = viewport.world.width * scale_;
    const double drawn_height = viewport.world.height * scale_;
    offset_x_ = viewport.x + (viewport.width - drawn_width) * 0.5;
    offset_y_ = viewport.y + (viewport.height - drawn_height) * 0.5;
    text_font_ = tg::font(fontForScale(scale_));

    lv_obj_t* clip = lv_obj_create(parent);
    if (clip == nullptr) return;
    lv_obj_remove_style_all(clip);
    lv_obj_set_pos(clip, viewport.x, viewport.y);
    lv_obj_set_size(clip, viewport.width, viewport.height);
    lv_obj_set_scrollable(clip, false);

    for (lv_obj_t*& object : objects_) {
        object = lv_label_create(clip);
        if (object == nullptr) {
            hide();
            return;
        }
        configure(object);
    }
    ready_ = true;
}

bool Presenter::validViewport(const Viewport& viewport) const {
    if (viewport.x < 0 || viewport.y < 0 || viewport.width <= 0 ||
        viewport.height <= 0 || viewport.width > 1024 || viewport.height > 1024 ||
        viewport.x > 1024 - viewport.width || viewport.y > 1024 - viewport.height ||
        !std::isfinite(viewport.world.width) || !std::isfinite(viewport.world.height) ||
        viewport.world.width <= 0.0f || viewport.world.height <= 0.0f) {
        return false;
    }
    const double scale = std::min(static_cast<double>(viewport.width) / viewport.world.width,
                                  static_cast<double>(viewport.height) / viewport.world.height);
    return std::isfinite(scale) && scale > 0.0;
}

void Presenter::configure(lv_obj_t* object) {
    lv_label_set_text_static(object, "");
    lv_label_set_long_mode(object, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_scrollable(object, false);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(object, text_font_, LV_PART_MAIN);
    lv_obj_set_hidden(object, true);
}

void Presenter::present(const tg::engine::DrawFrame& frame) {
    if (!ready_) return;

    const std::size_t count = std::min(frame.size(), tg::engine::DrawFrame::MAX_COMMANDS);
    for (std::size_t i = 0; i < count; ++i) {
        const tg::engine::DrawCommand& command = frame.at(i);
        lv_obj_t* object = objects_[i];
        const double left = offset_x_ + command.x * scale_;
        const double top = offset_y_ + command.y * scale_;
        const int x1 = pixelFloor(left, viewport_.x, viewport_.x + viewport_.width);
        const int y1 = pixelFloor(top, viewport_.y, viewport_.y + viewport_.height);
        if (command.palette_index == 0 || command.palette_index >= 16 ||
            x1 >= viewport_.x + viewport_.width ||
            y1 >= viewport_.y + viewport_.height) {
            lv_obj_set_hidden(object, true);
            continue;
        }

        if (command.kind == tg::engine::DrawKind::Rectangle) {
            const int x2 = pixelCeil(left + command.width * scale_, viewport_.x,
                                     viewport_.x + viewport_.width);
            const int y2 = pixelCeil(top + command.height * scale_, viewport_.y,
                                     viewport_.y + viewport_.height);
            if (x2 <= x1 || y2 <= y1) {
                lv_obj_set_hidden(object, true);
                continue;
            }
            lv_obj_set_pos(object, x1 - viewport_.x, y1 - viewport_.y);
            lv_label_set_text_static(object, "");
            lv_obj_set_size(object, x2 - x1, y2 - y1);
            lv_obj_set_style_bg_color(object, lv_color_hex(tg::PALETTE[command.palette_index]),
                                      LV_PART_MAIN);
            lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            const double local_x = left - viewport_.x;
            const double local_y = top - viewport_.y;
            const int max_text_width =
                tg::engine::DrawFrame::MAX_TEXT_BYTES * text_font_->line_height;
            if (local_x <= -max_text_width || local_y <= -text_font_->line_height) {
                lv_obj_set_hidden(object, true);
                continue;
            }
            const int text_x = pixelFloor(local_x, -max_text_width, viewport_.width);
            const int text_y = pixelFloor(local_y, -text_font_->line_height, viewport_.height);
            lv_obj_set_pos(object, text_x, text_y);
            lv_label_set_text_static(object, text_.capture(i, command));
            lv_obj_set_size(object, viewport_.width - text_x, text_font_->line_height);
            lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(object,
                                        lv_color_hex(tg::PALETTE[command.palette_index]),
                                        LV_PART_MAIN);
        }
        lv_obj_set_hidden(object, false);
    }
    for (std::size_t i = count; i < tg::engine::DrawFrame::MAX_COMMANDS; ++i) {
        lv_obj_set_hidden(objects_[i], true);
    }
}

void Presenter::hide() {
    for (lv_obj_t* object : objects_) {
        if (object != nullptr) lv_obj_set_hidden(object, true);
    }
}

}  // namespace os::lua_presenter
