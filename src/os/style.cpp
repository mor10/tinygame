#include "tg_style.h"

#include <cstdarg>
#include <cstdio>

// Implementation of the TinyGame style library (see include/tg_style.h).

namespace tg {

namespace {

// ---- Pixel font ------------------------------------------------------------------
// LVGL's built-in public-domain "unscii 8" 8x8 font, enlarged by whole pixels
// (nearest neighbour) so every glyph pixel becomes a crisp square block.

struct ScaledFont {
    const lv_font_t* base;
    std::uint8_t scale;
};

constexpr ScaledFont SCALED[] = {
    {&lv_font_unscii_8, 2},  // M
    {&lv_font_unscii_8, 3},  // L
    {&lv_font_unscii_8, 4},  // XL
};
lv_font_t scaled_fonts[3];
bool fonts_ready = false;

bool scaledGlyphDsc(const lv_font_t* font, lv_font_glyph_dsc_t* dsc, std::uint32_t letter,
                    std::uint32_t letter_next) {
    const auto* s = static_cast<const ScaledFont*>(font->dsc);
    if (!s->base->get_glyph_dsc(s->base, dsc, letter, letter_next)) {
        return false;
    }
    const std::uint8_t k = s->scale;
    dsc->adv_w = static_cast<std::uint16_t>(dsc->adv_w * k);
    dsc->box_w = static_cast<std::uint16_t>(dsc->box_w * k);
    dsc->box_h = static_cast<std::uint16_t>(dsc->box_h * k);
    dsc->ofs_x = static_cast<std::int16_t>(dsc->ofs_x * k);
    dsc->ofs_y = static_cast<std::int16_t>(dsc->ofs_y * k);
    // `stride` keeps the base font's 1-bit row stride for scaledGlyphBitmap;
    // the renderer uses the draw buffer's own stride for A8 output.
    dsc->format = LV_FONT_GLYPH_FORMAT_A8;
    return true;
}

const void* scaledGlyphBitmap(lv_font_glyph_dsc_t* dsc, lv_draw_buf_t* out) {
    if (out == nullptr) {
        return nullptr;
    }
    const auto* s = static_cast<const ScaledFont*>(dsc->resolved_font->dsc);
    const std::uint8_t k = s->scale;

    lv_font_glyph_dsc_t base = *dsc;
    base.resolved_font = s->base;
    base.box_w = static_cast<std::uint16_t>(dsc->box_w / k);
    base.box_h = static_cast<std::uint16_t>(dsc->box_h / k);
    base.format = LV_FONT_GLYPH_FORMAT_A1;
    base.req_raw_bitmap = 1;
    const auto* bits = static_cast<const std::uint8_t*>(s->base->get_glyph_bitmap(&base, nullptr));
    if (bits == nullptr) {
        return nullptr;
    }

    const std::uint32_t out_stride = out->header.stride;
    for (std::uint16_t y = 0; y < base.box_h; ++y) {
        std::uint8_t* row = out->data + static_cast<std::uint32_t>(y) * k * out_stride;
        for (std::uint16_t x = 0; x < base.box_w; ++x) {
            const std::uint32_t bit =
                base.stride ? y * base.stride * 8U + x : static_cast<std::uint32_t>(y) * base.box_w + x;
            const bool on = (bits[bit >> 3] >> (7 - (bit & 7))) & 1;
            for (std::uint8_t i = 0; i < k; ++i) {
                row[x * k + i] = on ? 0xFF : 0x00;
            }
        }
        for (std::uint8_t i = 1; i < k; ++i) {
            lv_memcpy(row + i * out_stride, row, dsc->box_w);
        }
    }
    return out;
}

void initFonts() {
    for (int i = 0; i < 3; ++i) {
        const ScaledFont& s = SCALED[i];
        lv_font_t& f = scaled_fonts[i];
        lv_memzero(&f, sizeof(f));
        f.dsc = &s;
        f.get_glyph_dsc = scaledGlyphDsc;
        f.get_glyph_bitmap = scaledGlyphBitmap;
        f.line_height = s.base->line_height * s.scale;
        f.base_line = s.base->base_line * s.scale;
        f.static_bitmap = 0;
    }
    fonts_ready = true;
}

// ---- Styles and theme --------------------------------------------------------------

lv_style_t screen_style;
lv_style_t block_style;
lv_style_t panel_style;
lv_style_t selected_style;
lv_style_t bar_style;
lv_style_t bar_indicator_style;
lv_theme_t* theme = nullptr;

void initStyles() {
    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, lv_color_hex(color::BG));
    lv_style_set_bg_opa(&screen_style, LV_OPA_COVER);
    lv_style_set_text_color(&screen_style, lv_color_hex(color::TEXT));
    lv_style_set_text_font(&screen_style, font(Font::M));

    lv_style_init(&block_style);
    lv_style_set_bg_color(&block_style, lv_color_hex(color::SURFACE));
    lv_style_set_bg_opa(&block_style, LV_OPA_COVER);
    lv_style_set_radius(&block_style, 0);
    lv_style_set_border_width(&block_style, 0);
    lv_style_set_pad_all(&block_style, 0);

    lv_style_init(&panel_style);
    lv_style_set_bg_color(&panel_style, lv_color_hex(color::SURFACE));
    lv_style_set_bg_opa(&panel_style, LV_OPA_COVER);
    lv_style_set_border_color(&panel_style, lv_color_hex(color::LINE));
    lv_style_set_border_width(&panel_style, BORDER);
    lv_style_set_radius(&panel_style, 0);

    lv_style_init(&selected_style);
    lv_style_set_bg_color(&selected_style, lv_color_hex(color::ACCENT));
    lv_style_set_bg_opa(&selected_style, LV_OPA_COVER);
    lv_style_set_text_color(&selected_style, lv_color_hex(color::BG));

    lv_style_init(&bar_style);
    lv_style_set_bg_color(&bar_style, lv_color_hex(color::LINE));
    lv_style_set_bg_opa(&bar_style, LV_OPA_COVER);
    lv_style_set_radius(&bar_style, 0);

    lv_style_init(&bar_indicator_style);
    lv_style_set_bg_color(&bar_indicator_style, lv_color_hex(color::ACCENT));
    lv_style_set_bg_opa(&bar_indicator_style, LV_OPA_COVER);
    lv_style_set_radius(&bar_indicator_style, 0);
}

// Gives every new LVGL object the TinyGame look.
void applyTheme(lv_theme_t*, lv_obj_t* obj) {
    if (lv_obj_get_parent(obj) == nullptr) {
        lv_obj_add_style(obj, &screen_style, 0);
        lv_obj_set_scrollable(obj, false);
    } else if (lv_obj_check_type(obj, &lv_obj_class)) {
        lv_obj_add_style(obj, &block_style, 0);
        lv_obj_set_scrollable(obj, false);
    } else if (lv_obj_check_type(obj, &lv_bar_class)) {
        lv_obj_add_style(obj, &bar_style, LV_PART_MAIN);
        lv_obj_add_style(obj, &bar_indicator_style, LV_PART_INDICATOR);
    }
}

void setTextColor(lv_obj_t* obj, std::uint32_t rgb) {
    lv_obj_set_style_text_color(obj, lv_color_hex(rgb), LV_PART_MAIN);
}

}  // namespace

const lv_font_t* font(Font size) {
    if (!fonts_ready) {
        initFonts();
    }
    switch (size) {
        case Font::S:
            return &lv_font_unscii_8;
        case Font::M:
            return &scaled_fonts[0];
        case Font::L:
            return &scaled_fonts[1];
        case Font::XL:
            return &scaled_fonts[2];
    }
    return &scaled_fonts[0];
}

namespace style {
lv_style_t* screen() { return &screen_style; }
lv_style_t* block() { return &block_style; }
lv_style_t* panel() { return &panel_style; }
lv_style_t* selected() { return &selected_style; }
}  // namespace style

void styleInit(lv_display_t* display) {
    initFonts();
    initStyles();
    theme = lv_theme_create();
    lv_theme_set_apply_cb(theme, applyTheme);
    lv_display_set_theme(display, theme);
}

namespace ui {

lv_obj_t* label(lv_obj_t* parent, const char* text, Font size, std::uint32_t rgb) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font(size), LV_PART_MAIN);
    setTextColor(l, rgb);
    return l;
}

void text(lv_obj_t* label, const char* fmt, ...) {
    char buffer[96];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    lv_label_set_text(label, buffer);
}

lv_obj_t* block(lv_obj_t* parent, int x, int y, int w, int h, std::uint32_t rgb) {
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(rgb), LV_PART_MAIN);
    return b;
}

lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_add_style(p, &panel_style, 0);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    return p;
}

lv_obj_t* header(lv_obj_t* parent, const char* title) {
    lv_obj_t* l = label(parent, title, Font::M, color::ACCENT);
    lv_obj_set_pos(l, 20, GAP_M);
    block(parent, GAP_M, GAP_M, 4, 4, color::ACCENT);
    block(parent, GAP_M, GAP_M + 8, 4, 4, color::ACCENT);
    block(parent, 0, HEADER_H - BORDER, WIDTH, BORDER, color::LINE);
    block(parent, GAP_M, HEADER_H - BORDER, 40, BORDER, color::ACCENT);
    return l;
}

lv_obj_t* footer(lv_obj_t* parent, const char* hint) {
    lv_obj_t* l = label(parent, hint, Font::S, color::TEXT_DIM);
    lv_obj_set_pos(l, GAP_M, FOOTER_Y);
    lv_obj_set_width(l, WIDTH - 2 * GAP_M);
    block(parent, GAP_M, HEIGHT - BORDER, WIDTH - 2 * GAP_M, BORDER, color::LINE);
    return l;
}

lv_obj_t* popup(lv_obj_t* parent, const char* title, const char* hint,
                std::uint32_t title_rgb) {
    constexpr int W = 272;
    constexpr int H = 104;
    lv_obj_t* p = panel(parent, (WIDTH - W) / 2, (HEIGHT - H) / 2, W, H);
    lv_obj_t* t = label(p, title, Font::L, title_rgb);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, GAP_M);
    lv_obj_t* h = label(p, hint, Font::S, color::TEXT_DIM);
    lv_obj_set_width(h, W - 2 * GAP_M - 2 * BORDER);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -GAP_M - 2);
    lv_obj_t* value = label(p, "", Font::M, color::TEXT);
    lv_obj_align(value, LV_ALIGN_TOP_MID, 0, 40);
    block(p, 0, 0, 32, BORDER, title_rgb);
    block(p, W - 2 * BORDER - 32, H - 3 * BORDER, 32, BORDER, title_rgb);
    return p;
}

lv_obj_t* popupTitle(lv_obj_t* popup) { return lv_obj_get_child(popup, 0); }
lv_obj_t* popupHint(lv_obj_t* popup) { return lv_obj_get_child(popup, 1); }
lv_obj_t* popupValue(lv_obj_t* popup) { return lv_obj_get_child(popup, 2); }

void result(lv_obj_t* popup, std::uint32_t score, std::uint32_t best, bool new_best) {
    lv_obj_t* title = popupTitle(popup);
    lv_label_set_text(title, new_best ? "NEW BEST!" : "GAME OVER");
    setTextColor(title, new_best ? color::WARN : color::ACCENT);
    text(popupValue(popup), "%lu", static_cast<unsigned long>(score));
    text(popupHint(popup), "BEST %lu\nCLICK: AGAIN", static_cast<unsigned long>(best));
}

void Menu::create(lv_obj_t* parent, int x, int y, int w, std::uint8_t rows) {
    row_count_ = rows > MAX_ROWS ? MAX_ROWS : rows;
    first_ = 0;
    for (std::uint8_t i = 0; i < row_count_; ++i) {
        lv_obj_t* row = block(parent, x, y + i * ROW_H, w - 12, ROW_H, color::SURFACE);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(color::LINE), LV_PART_MAIN);
        chips_[i] = block(row, GAP_S, 2, 12, 12, color::ACCENT);
        badges_[i] = label(chips_[i], "", Font::S, color::BG);
        lv_obj_center(badges_[i]);
        names_[i] = label(row, "", Font::M);
        lv_obj_align(names_[i], LV_ALIGN_LEFT_MID, GAP_S, 0);
        values_[i] = label(row, "", Font::M, color::TEXT_DIM);
        lv_obj_align(values_[i], LV_ALIGN_RIGHT_MID, -GAP_S, 0);
        rows_[i] = row;
    }
    more_up_ = label(parent, "^", Font::S, color::TEXT_DIM);
    lv_obj_set_pos(more_up_, x + w - 8, y + 2);
    more_down_ = label(parent, "v", Font::S, color::TEXT_DIM);
    lv_obj_set_pos(more_down_, x + w - 8, y + row_count_ * ROW_H - 10);
}

void Menu::show(const MenuItem* items, std::uint8_t count, std::uint8_t selected,
                bool editing) {
    if (selected < first_) {
        first_ = selected;
    } else if (selected >= first_ + row_count_) {
        first_ = static_cast<std::uint8_t>(selected - row_count_ + 1);
    }
    for (std::uint8_t i = 0; i < row_count_; ++i) {
        const std::uint8_t index = static_cast<std::uint8_t>(first_ + i);
        lv_obj_set_hidden(rows_[i], index >= count);
        if (index >= count) {
            continue;
        }
        const MenuItem& item = items[index];
        const bool is_selected = index == selected;
        const std::uint32_t highlight = is_selected && editing ? color::WARN : item.color;

        lv_obj_set_style_bg_color(rows_[i],
                                  lv_color_hex(is_selected ? highlight : color::SURFACE), LV_PART_MAIN);
        lv_obj_set_style_border_color(rows_[i], lv_color_hex(is_selected ? highlight : color::LINE),
                                      LV_PART_MAIN);
        lv_obj_set_hidden(chips_[i], !item.chip);
        lv_obj_set_style_bg_color(chips_[i], lv_color_hex(is_selected ? color::BG : item.color),
                                  LV_PART_MAIN);
        const char badge[] = {item.name[0], '\0'};
        lv_label_set_text(badges_[i], badge);
        setTextColor(badges_[i], is_selected ? item.color : color::BG);
        lv_label_set_text(names_[i], item.name);
        lv_obj_align(names_[i], LV_ALIGN_LEFT_MID, item.chip ? 24 : GAP_S, 0);
        setTextColor(names_[i], is_selected ? color::BG : color::TEXT);
        lv_label_set_text(values_[i], item.value ? item.value : "");
        setTextColor(values_[i], is_selected ? color::BG : color::TEXT_DIM);
    }
    lv_obj_set_hidden(more_up_, first_ == 0);
    lv_obj_set_hidden(more_down_, first_ + row_count_ >= count);
}

}  // namespace ui

}  // namespace tg
