#include "shell_ui.h"

#include <Arduino.h>

#include <cstdio>

#include "registry.h"
#include "storage.h"
#include "tg_style.h"

namespace shell_ui {

namespace {

using shell::Screen;
using tg::Font;
namespace color = tg::color;
namespace ui = tg::ui;

constexpr std::uint32_t BLINK_MS = 500;
constexpr std::uint32_t DIAGNOSTICS_REFRESH_MS = 50;
constexpr int MENU_Y = tg::HEADER_H + tg::GAP_S;
constexpr std::uint8_t MENU_ROWS = 5;

// What is on screen now, so work is only done when something changes.
bool has_shown = false;
Screen shown_screen = Screen::Welcome;
std::uint8_t shown_selected = 0;
bool shown_editing = false;
settings::Settings shown_settings;

lv_obj_t* page = nullptr;     // current OS screen (not used while in a game)
lv_obj_t* overlay = nullptr;  // pause menu, drawn on top of the game screen
ui::Menu menu;
lv_obj_t* blink_label = nullptr;
lv_obj_t* footer_label = nullptr;
lv_obj_t* best_label = nullptr;

// Launcher data, loaded when the launcher opens.
ui::MenuItem launcher_items[registry::MAX_GAMES + 1];
std::uint32_t launcher_best[registry::MAX_GAMES];

// Settings value text, e.g. "< 90% >".
char settings_values[settings::ITEM_COUNT][14];

// Diagnostics widgets.
struct Diagnostics {
    lv_obj_t* lines[11] = {};
    lv_obj_t* tilt_dot = nullptr;
    lv_obj_t* tilt_label = nullptr;
    std::uint32_t refreshed_at = 0;
} diag;
constexpr int TILT_BOX_X = 236;
constexpr int TILT_BOX_Y = 36;
constexpr int TILT_BOX = 72;
constexpr int TILT_DOT = 8;

// ---- Page builders ------------------------------------------------------------

void buildWelcome(lv_obj_t* scr) {
    ui::panel(scr, 16, 20, 288, 132);
    ui::block(scr, 32, 18, 48, 4, color::ACCENT);
    ui::block(scr, 240, 150, 48, 4, color::ACCENT);
    lv_obj_t* edition = ui::label(scr, "SYSTEM 01", Font::S, color::TEXT_DIM);
    lv_obj_set_pos(edition, 32, 28);
    for (int i = 0; i < 3; ++i) {
        ui::block(scr, 268 + (i % 2) * 8, 28 + (i / 2) * 8, 4, 4, color::ACCENT);
    }
    lv_obj_t* title = ui::label(scr, "TINYGAME", Font::XL, color::ACCENT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_t* sub = ui::label(scr, "POCKET GAME CONSOLE", Font::S, color::TEXT_DIM);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 96);
    blink_label = ui::label(scr, "CLICK EITHER KNOB", Font::M, color::TEXT);
    lv_obj_align(blink_label, LV_ALIGN_TOP_MID, 0, 120);
}

void buildLauncher(lv_obj_t* scr) {
    const std::uint8_t count = registry::count();
    ui::header(scr, "CARTRIDGES");
    char counter[16];
    std::snprintf(counter, sizeof(counter), "%u", static_cast<unsigned>(count));
    lv_obj_t* n = ui::label(scr, counter, Font::M, color::TEXT_DIM);
    lv_obj_align(n, LV_ALIGN_TOP_RIGHT, -tg::GAP_M, tg::GAP_M);

    for (std::uint8_t i = 0; i < count; ++i) {
        const tg::GameInfo& g = registry::get(i);
        launcher_items[i] = {g.name, nullptr, g.color, true};
        launcher_best[i] = storage::loadBest(g.id);
    }
    launcher_items[count] = {"SYSTEM", nullptr, color::SILVER, false};

    menu.create(scr, tg::GAP_S, MENU_Y, tg::WIDTH - 2 * tg::GAP_S, MENU_ROWS);
    if (count == 0) {
        lv_obj_t* empty = ui::label(scr, "NO GAMES YET", Font::S, color::TEXT_DIM);
        lv_obj_set_pos(empty, tg::GAP_M, MENU_Y + ui::Menu::ROW_H + tg::GAP_M);
    }
    footer_label = ui::footer(scr, "TURN: CHOOSE  CLICK: OPEN");
    ui::block(scr, tg::GAP_M, 140, 4, 8, color::WARN);
    best_label = ui::label(scr, "", Font::S, color::WARN);
    lv_obj_set_pos(best_label, 20, 140);
}

void buildSystem(lv_obj_t* scr) {
    ui::header(scr, "SYSTEM");
    menu.create(scr, tg::GAP_S, MENU_Y, tg::WIDTH - 2 * tg::GAP_S, MENU_ROWS);
    ui::footer(scr, "TURN: CHOOSE  CLICK: OPEN  HOLD: BACK");
}

void buildSettings(lv_obj_t* scr) {
    ui::header(scr, "SETTINGS");
    menu.create(scr, tg::GAP_S, MENU_Y, tg::WIDTH - 2 * tg::GAP_S, MENU_ROWS);
    footer_label = ui::footer(scr, "");
}

void buildDiagnostics(lv_obj_t* scr) {
    ui::header(scr, "DIAGNOSTICS");
    constexpr int TOP = tg::HEADER_H + tg::GAP_S;
    constexpr int STEP = (tg::FOOTER_Y - TOP - tg::GAP_M) / 10;
    for (int i = 0; i < 11; ++i) {
        diag.lines[i] = ui::label(scr, "", Font::S, i < 8 ? color::TEXT : color::TEXT_DIM);
        lv_obj_set_pos(diag.lines[i], tg::GAP_M, TOP + i * STEP);
        lv_obj_set_width(diag.lines[i], i < 8 ? TILT_BOX_X - tg::GAP_M - tg::GAP_S :
                                               tg::WIDTH - 2 * tg::GAP_M);
        lv_label_set_long_mode(diag.lines[i], LV_LABEL_LONG_CLIP);
    }

    ui::panel(scr, TILT_BOX_X, TILT_BOX_Y, TILT_BOX, TILT_BOX);
    ui::block(scr, TILT_BOX_X + TILT_BOX / 2 - 1, TILT_BOX_Y + 4, 2, TILT_BOX - 8, color::LINE);
    ui::block(scr, TILT_BOX_X + 4, TILT_BOX_Y + TILT_BOX / 2 - 1, TILT_BOX - 8, 2, color::LINE);
    diag.tilt_dot = ui::block(scr, 0, 0, TILT_DOT, TILT_DOT, color::ACCENT);
    diag.tilt_label = ui::label(scr, "TILT", Font::S, color::TEXT_DIM);
    lv_obj_set_pos(diag.tilt_label, TILT_BOX_X, TILT_BOX_Y + TILT_BOX + tg::GAP_S);
    ui::footer(scr, "HOLD: BACK  IDLE 3 MIN: SLEEP");
    diag.refreshed_at = 0;
}

void buildStyle(lv_obj_t* scr) {
    char title[16];
    std::snprintf(title, sizeof(title), "STYLE V%d", tg::STYLE_VERSION);
    ui::header(scr, title);
    for (int i = 0; i < 16; ++i) {
        lv_obj_t* swatch =
            ui::block(scr, tg::GAP_M + (i % 8) * 38, 34 + (i / 8) * 18, 34, 14, tg::PALETTE[i]);
        lv_obj_set_style_border_color(swatch, lv_color_hex(color::LINE), LV_PART_MAIN);
        lv_obj_set_style_border_width(swatch, i == 0 ? tg::BORDER : 0, LV_PART_MAIN);
    }
    lv_obj_t* s = ui::label(scr, "SMALL 8PX: HINTS AND HUD", Font::S, color::TEXT_DIM);
    lv_obj_set_pos(s, tg::GAP_M, 74);
    lv_obj_t* m = ui::label(scr, "MEDIUM 16PX", Font::M);
    lv_obj_set_pos(m, tg::GAP_M, 86);
    lv_obj_t* l = ui::label(scr, "LARGE 24", Font::L, color::ACCENT);
    lv_obj_set_pos(l, tg::GAP_M, 106);
    lv_obj_t* xl = ui::label(scr, "XL", Font::XL, color::WARN);
    lv_obj_set_pos(xl, 236, 78);

    lv_obj_t* sel = ui::block(scr, tg::GAP_M, 134, 140, 20, color::ACCENT);
    lv_obj_t* sel_text = ui::label(sel, "SELECTED", Font::M, color::BG);
    lv_obj_align(sel_text, LV_ALIGN_LEFT_MID, tg::GAP_S, 0);
    lv_obj_t* p = ui::panel(scr, 160, 132, 152, 24);
    lv_obj_t* p_text = ui::label(p, "PANEL", Font::M);
    lv_obj_center(p_text);
    ui::footer(scr, "CLICK: BACK");
}

void buildPause(lv_obj_t* game_screen) {
    constexpr int W = 232;
    constexpr int H = 128;
    overlay = ui::panel(game_screen, (tg::WIDTH - W) / 2, (tg::HEIGHT - H) / 2, W, H);
    lv_obj_set_style_border_color(overlay, lv_color_hex(color::ACCENT), LV_PART_MAIN);
    lv_obj_t* title = ui::label(overlay, "PAUSED", Font::L, color::ACCENT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, tg::GAP_M);
    menu.create(overlay, tg::GAP_S, 36, W - 2 * tg::GAP_S, shell::PAUSE_COUNT);
    lv_obj_t* hint = ui::label(overlay, "HOLD: RESUME", Font::S, color::TEXT_DIM);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -tg::GAP_S - 2);
}

// ---- Page refreshers -------------------------------------------------------------

void showLauncher(std::uint8_t selected) {
    const std::uint8_t count = registry::count();
    menu.show(launcher_items, static_cast<std::uint8_t>(count + 1), selected);
    if (selected < count && launcher_best[selected] > 0) {
        ui::text(best_label, "BEST %lu", static_cast<unsigned long>(launcher_best[selected]));
    } else {
        lv_label_set_text(best_label, selected < count ? "SET YOUR FIRST RECORD" : "CONSOLE CONTROLS");
    }
    lv_label_set_text(footer_label, selected < count ? "TURN: CHOOSE  CLICK: PLAY"
                                                     : "TURN: CHOOSE  CLICK: OPEN");
}

void showSystem(std::uint8_t selected) {
    static const ui::MenuItem items[shell::SYS_COUNT] = {
        {"SETTINGS"}, {"DIAGNOSTICS"}, {"STYLE"}, {"SLEEP", nullptr, color::PLUM},
        {"BACK", nullptr, color::SILVER}};
    menu.show(items, shell::SYS_COUNT, selected);
}

void showSettings(const shell::State& state) {
    ui::MenuItem items[settings::ITEM_COUNT + 1];
    for (std::uint8_t i = 0; i < settings::ITEM_COUNT; ++i) {
        const bool editing = state.editing() && state.selected() == i;
        if (i == settings::HANDEDNESS) {
            std::snprintf(settings_values[i], sizeof(settings_values[i]),
                          editing ? "<%s>" : "%s", state.settings().leftHanded() ? "LEFT" : "RIGHT");
        } else {
            std::snprintf(settings_values[i], sizeof(settings_values[i]),
                          editing ? "<%u%s>" : "%u%s",
                          static_cast<unsigned>(state.settings().value[i]), settings::SPECS[i].unit);
        }
        items[i] = {settings::SPECS[i].name, settings_values[i]};
    }
    items[settings::ITEM_COUNT] = {"BACK", nullptr, color::SILVER};
    menu.show(items, settings::ITEM_COUNT + 1, state.selected(), state.editing());
    lv_label_set_text(footer_label, state.editing() ? "TURN: CHANGE  CLICK: OK  HOLD: CANCEL"
                                                    : "TURN: CHOOSE  CLICK: EDIT  HOLD: BACK");
}

void showPause(std::uint8_t selected) {
    static const ui::MenuItem items[shell::PAUSE_COUNT] = {
        {"RESUME", nullptr, color::GOOD}, {"RESTART", nullptr, color::WARN},
        {"QUIT", nullptr, color::BAD}};
    menu.show(items, shell::PAUSE_COUNT, selected);
}

void refreshDiagnostics(const Live& live, std::uint32_t now_ms) {
    if (diag.refreshed_at != 0 && now_ms - diag.refreshed_at < DIAGNOSTICS_REFRESH_MS) {
        return;
    }
    diag.refreshed_at = now_ms;
    const input::State& in = live.input;
    const imu::Reading& m = live.imu;
    for (int side = 0; side < 2; ++side) {
        const auto& e = side == 0 ? in.left : in.right;
        const int row = side * 4;
        ui::text(diag.lines[row], "%s HW %ld AB%d%d %s", side == 0 ? "L" : "R",
                 static_cast<long>(e.total), (e.ab >> 1) & 1, e.ab & 1,
                 e.last_direction > 0 ? "CW" : (e.last_direction < 0 ? "CCW" : "--"));
        ui::text(diag.lines[row + 1], "%s HELD %lu MS", e.button.down ? "DOWN" : "UP",
                 static_cast<unsigned long>(e.button.held_ms));
        ui::text(diag.lines[row + 2], "C%lu LAST %s",
                 static_cast<unsigned long>(e.button.clicks), input::eventName(e.button.last_event));
        ui::text(diag.lines[row + 3], "ERR%lu O%lu",
                 static_cast<unsigned long>(e.invalid), static_cast<unsigned long>(e.overruns));
    }
    ui::text(diag.lines[8], "IDLE 3 MIN %s WAKE %s",
             live.accepting_input ? "READY" : "WAIT", live.wake_cause);
    ui::text(diag.lines[9], "E L %ld C%lu %s",
             static_cast<long>(live.effective_left.total),
             static_cast<unsigned long>(live.effective_left.clicks),
             input::eventName(live.effective_left.last_event));
    ui::text(diag.lines[10], "E R %ld C%lu %s",
             static_cast<long>(live.effective_right.total),
             static_cast<unsigned long>(live.effective_right.clicks),
             input::eventName(live.effective_right.last_event));
    lv_label_set_text(diag.tilt_label,
                      live.imu_ok ? "TILT" : (live.imu_present ? "IMU ERROR" : "NO IMU"));
    const int travel = TILT_BOX - TILT_DOT - 8;
    lv_obj_set_pos(diag.tilt_dot,
                   TILT_BOX_X + 4 + static_cast<int>((m.tilt_x + 1.0f) / 2.0f * travel),
                   TILT_BOX_Y + 4 + static_cast<int>((m.tilt_y + 1.0f) / 2.0f * travel));
}

// ---- Screen switching -----------------------------------------------------------

void openPage(Screen screen) {
    lv_obj_t* old = page;
    page = lv_obj_create(nullptr);
    blink_label = nullptr;
    switch (screen) {
        case Screen::Welcome:
            buildWelcome(page);
            break;
        case Screen::Launcher:
            buildLauncher(page);
            break;
        case Screen::System:
            buildSystem(page);
            break;
        case Screen::Settings:
            buildSettings(page);
            break;
        case Screen::Diagnostics:
            buildDiagnostics(page);
            break;
        case Screen::Style:
            buildStyle(page);
            break;
        default:
            break;
    }
    lv_screen_load(page);
    if (old != nullptr) {
        lv_obj_delete(old);
    }
}

void closePage() {
    if (page != nullptr) {
        lv_obj_delete(page);
        page = nullptr;
    }
}

void changeScreen(Screen from, Screen to, lv_obj_t* game_screen) {
    if (to == Screen::Pause) {
        buildPause(game_screen);
        return;
    }
    if (from == Screen::Pause && overlay != nullptr) {
        // On Resume the game screen lives on: remove the menu. On Restart or
        // Quit the OS deletes the old game screen, and the overlay with it.
        if (to == Screen::Game && lv_obj_get_parent(overlay) == game_screen) {
            lv_obj_delete(overlay);
        }
        overlay = nullptr;
    }
    if (to == Screen::Game) {
        closePage();  // the OS has already loaded the game screen
    } else {
        openPage(to);
    }
}

}  // namespace

void update(const shell::State& state, lv_obj_t* game_screen, const Live& live,
            std::uint32_t now_ms) {
    const Screen screen = state.screen();
    const bool screen_changed = !has_shown || screen != shown_screen;
    if (screen_changed) {
        changeScreen(has_shown ? shown_screen : Screen::Game, screen, game_screen);
    }
    const bool content_changed = screen_changed || state.selected() != shown_selected ||
                                 state.editing() != shown_editing ||
                                 state.settings() != shown_settings;
    has_shown = true;
    shown_screen = screen;
    shown_selected = state.selected();
    shown_editing = state.editing();
    shown_settings = state.settings();

    if (content_changed) {
        switch (screen) {
            case Screen::Launcher:
                showLauncher(state.selected());
                break;
            case Screen::System:
                showSystem(state.selected());
                break;
            case Screen::Settings:
                showSettings(state);
                break;
            case Screen::Pause:
                showPause(state.selected());
                break;
            default:
                break;
        }
    }
    if (screen == Screen::Welcome && blink_label != nullptr) {
        lv_obj_set_hidden(blink_label, (now_ms / BLINK_MS) % 2 == 1);
    }
    if (screen == Screen::Diagnostics) {
        refreshDiagnostics(live, now_ms);
    }
}

}  // namespace shell_ui
