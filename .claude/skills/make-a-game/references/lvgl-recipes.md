# LVGL 9 recipes for TinyGame

Every object a game creates gets the TinyGame theme automatically: plain objects are square SURFACE blocks with no scrolling, and labels use `Font::M` in TEXT. Coordinates are pixels from the top-left of the parent.

## Move, hide, recolour

```cpp
lv_obj_set_pos(obj, x, y);            // or lv_obj_set_x / lv_obj_set_y
lv_obj_set_size(obj, w, h);
lv_obj_set_hidden(obj, true);         // hide (false = show)
lv_obj_set_style_bg_color(obj, lv_color_hex(tg::color::RED), LV_PART_MAIN);
lv_obj_move_foreground(obj);          // draw on top of its siblings
tg::ui::text(label, "LIVES %d", lives);
```

Only call these when a value changes. If you keep the last drawn position in your class and compare against it, LVGL redraws less.

## Pool of things (bullets, enemies, particles)

```cpp
static constexpr int MAX_BULLETS = 8;
struct Bullet { lv_obj_t* obj; float x, y; bool alive; };
Bullet bullets_[MAX_BULLETS];          // class member, not a global

// start(): create them all hidden
for (auto& b : bullets_) {
    b.obj = tg::ui::block(screen, 0, 0, 2, 6, tg::color::PEACH);
    b.alive = false;
    lv_obj_set_hidden(b.obj, true);
}
// fire: reuse the first dead one
for (auto& b : bullets_) if (!b.alive) { b.alive = true; b.x = x; b.y = y;
    lv_obj_set_hidden(b.obj, false); break; }
```

## Sprite from text art (chunky 8-bit characters)

```cpp
// Each character is one 2x2 "fat pixel"; '.' = empty.
constexpr const char* SHIP[] = {
    "...##...",
    "..####..",
    ".##..##.",
    "########",
};
constexpr int PX = 2;
lv_obj_t* makeSprite(lv_obj_t* parent, const char* const* rows, int h, std::uint32_t rgb) {
    lv_obj_t* group = lv_obj_create(parent);              // transparent holder
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, LV_PART_MAIN);
    int w = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; rows[y][x] != '\0'; ++x) {
            if (rows[y][x] != '.') tg::ui::block(group, x * PX, y * PX, PX, PX, rgb);
            if (x + 1 > w) w = x + 1;
        }
    }
    lv_obj_set_size(group, w * PX, h * PX);
    return group;                                         // move the group as one
}
```

Every lit pixel is an object, so keep sprites small (under about 40 pixels each). For bigger art, use a canvas.

## Canvas (pixel art, trails, drawing)

```cpp
// start():
canvas_buf_ = lv_draw_buf_create(80, 60, LV_COLOR_FORMAT_RGB565, 0);
canvas_ = lv_canvas_create(screen);
lv_canvas_set_draw_buf(canvas_, canvas_buf_);
lv_canvas_fill_bg(canvas_, lv_color_hex(tg::color::BG), LV_OPA_COVER);
lv_obj_set_pos(canvas_, 10, 30);
lv_obj_set_style_transform_scale(canvas_, 512, LV_PART_MAIN);  // 2x (256 = 1x)
// update():
lv_canvas_set_px(canvas_, x, y, lv_color_hex(tg::color::LIME), LV_OPA_COVER);
lv_obj_invalidate(canvas_);
// destructor (memory only, the OS deletes the canvas object):
~MyGame() override { if (canvas_buf_) lv_draw_buf_destroy(canvas_buf_); }
```

Members: `lv_draw_buf_t* canvas_buf_ = nullptr; lv_obj_t* canvas_ = nullptr;`

## Lines

```cpp
lv_point_precise_t pts_[2] = {{0, 0}, {100, 50}};   // class member: must outlive the line
lv_obj_t* line = lv_line_create(screen);
lv_line_set_points(line, pts_, 2);
lv_obj_set_style_line_color(line, lv_color_hex(tg::color::SKY), LV_PART_MAIN);
lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
```

## Progress bar

```cpp
lv_obj_t* bar = lv_bar_create(screen);   // themed: flat LINE track, ACCENT fill
lv_obj_set_pos(bar, 8, 160); lv_obj_set_size(bar, 100, 6);
lv_bar_set_range(bar, 0, 100); lv_bar_set_value(bar, 40, LV_ANIM_OFF);
```

## Random numbers

Include `tg_random.h` and keep a `std::uint32_t seed_` member. Set it to
`lv_tick_get()` in `start()`, then use `tg::randomNext(seed_) % n` for a nonzero
range `n`. Zero seeds are remapped safely. Use a fixed seed in tests. Avoid
`std::srand`/`std::rand`, which share mutable global state across games.

## LVGL 8 → 9 (online examples are often LVGL 8)

| LVGL 8 (don't use) | LVGL 9 (use) |
|---|---|
| `lv_scr_act()` | `lv_screen_active()` (but use the `screen` passed to `start`) |
| `lv_obj_del(o)` | `lv_obj_delete(o)` (usually not needed) |
| `lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN)` | `lv_obj_set_hidden(o, true)` |
| `lv_obj_clear_flag(...)` | `lv_obj_remove_flag(...)` |
| `lv_img_*` | `lv_image_*` |
| `lv_btn_*` | `lv_button_*` (but games have no touch; avoid buttons) |
| `LV_IMG_CF_*` | `LV_COLOR_FORMAT_*` |
| `lv_canvas_set_buffer` | `lv_canvas_set_draw_buf` + `lv_draw_buf_create` |
| `lv_disp_*` | `lv_display_*` |

Enabled widgets: obj, label, bar, image, canvas, line. To use another one, enable it in `include/lv_conf.h`. That's a console change, so mention it in the PR.
