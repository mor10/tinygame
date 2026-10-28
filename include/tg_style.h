#pragma once

// TinyGame style library: the one place the console's look is defined.
//
//   * dark mode, always
//   * blocky 8-bit: square corners, solid fills, chunky pixel font
//   * Pocket Arcade: cartridge rows, pixel badges and framed score panels
//
// Change a token here and the OS and every game follow. Games should use these
// tokens and helpers; game-specific art (sprite colours, special effects) may
// use its own values inside the game's folder. Don't edit this file to suit
// one game; bump STYLE_VERSION when you change shared tokens.

#include <lvgl.h>

#include <cstdint>

#include "tg_types.h"

namespace tg {

inline constexpr int STYLE_VERSION = 2;

// ---- Colours (0xRRGGBB) -----------------------------------------------------
namespace color {
// Interface
inline constexpr std::uint32_t BG = 0x0B0E1A;        // screen background
inline constexpr std::uint32_t SURFACE = 0x161B2E;   // panels, HUD strip
inline constexpr std::uint32_t LINE = 0x2A3150;      // borders, dividers, empty bars
inline constexpr std::uint32_t TEXT = 0xE8ECF8;      // normal text
inline constexpr std::uint32_t TEXT_DIM = 0x7A84A6;  // hints, labels
inline constexpr std::uint32_t ACCENT = 0x3DE0C8;    // titles, selection, highlights
inline constexpr std::uint32_t GOOD = 0x5BE36B;      // success, extra life
inline constexpr std::uint32_t WARN = 0xFFC94A;      // editing, caution, best score
inline constexpr std::uint32_t BAD = 0xFF4F64;       // danger, life lost

// Retro game palette: 16 bright colours that read well on the small screen.
inline constexpr std::uint32_t BLACK = 0x0B0E1A;
inline constexpr std::uint32_t NAVY = 0x1E2A56;
inline constexpr std::uint32_t PLUM = 0x7B2D6B;
inline constexpr std::uint32_t FOREST = 0x1F8A5B;
inline constexpr std::uint32_t BROWN = 0x9A5A3C;
inline constexpr std::uint32_t SLATE = 0x56607A;
inline constexpr std::uint32_t SILVER = 0xB8C4D9;
inline constexpr std::uint32_t WHITE = 0xF4F6FF;
inline constexpr std::uint32_t RED = 0xFF4F64;
inline constexpr std::uint32_t ORANGE = 0xFF9F1C;
inline constexpr std::uint32_t YELLOW = 0xFFD166;
inline constexpr std::uint32_t LIME = 0x5BE36B;
inline constexpr std::uint32_t SKY = 0x3A86FF;
inline constexpr std::uint32_t MINT = 0x3DE0C8;
inline constexpr std::uint32_t PINK = 0xFF6FD8;
inline constexpr std::uint32_t PEACH = 0xFFB38A;
}  // namespace color

inline constexpr std::uint32_t PALETTE[16] = {
    color::BLACK, color::NAVY,   color::PLUM,   color::FOREST, color::BROWN, color::SLATE,
    color::SILVER, color::WHITE, color::RED,    color::ORANGE, color::YELLOW, color::LIME,
    color::SKY,   color::MINT,   color::PINK,   color::PEACH,
};

// ---- Spacing ----------------------------------------------------------------
// Use a 4 px grid and 2 px panel borders. Menu outlines are 1 px to fit text.
inline constexpr int GRID = 4;
inline constexpr int GAP_S = 4;
inline constexpr int GAP_M = 8;
inline constexpr int GAP_L = 16;
inline constexpr int BORDER = 2;
inline constexpr int HEADER_H = 28;  // tg::ui::header strip
inline constexpr int FOOTER_Y = HEIGHT - 14;

// ---- Fonts --------------------------------------------------------------------
// One 8x8 pixel face, scaled by whole pixels so it always stays crisp.
// Characters per line: S 40, M 20, L 13, XL 10. ASCII only.
enum class Font : std::uint8_t {
    S,   //  8 px: hints, HUD details
    M,   // 16 px: normal text, menus (default)
    L,   // 24 px: titles, big numbers
    XL,  // 32 px: splash titles
};
const lv_font_t* font(Font size);

// ---- Shared styles ---------------------------------------------------------------
// These are applied automatically by the TinyGame theme: screens are dark,
// plain objects (lv_obj_create) are square SURFACE blocks with no scrolling,
// labels use Font::M in TEXT, bars are flat. Reuse them with
// lv_obj_add_style(obj, tg::style::panel(), 0) when building your own widgets.
namespace style {
lv_style_t* screen();    // BG fill, default font and text colour
lv_style_t* block();     // square solid SURFACE fill, no border/padding
lv_style_t* panel();     // SURFACE fill + LINE border
lv_style_t* selected();  // inverted 8-bit cursor: ACCENT fill, BG text
}  // namespace style

// ---- Helpers ------------------------------------------------------------------------
namespace ui {

// A text label. `text` is copied. Returns the label so you can move it.
lv_obj_t* label(lv_obj_t* parent, const char* text, Font size = Font::M,
                std::uint32_t rgb = color::TEXT);
// Changes a label's text, printf-style: ui::text(score_label, "SCORE %d", score)
void text(lv_obj_t* label, const char* fmt, ...);
// A solid square block: walls, players, bricks, pixels.
lv_obj_t* block(lv_obj_t* parent, int x, int y, int w, int h, std::uint32_t rgb);
// A framed SURFACE panel.
lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h);
// Page title strip at the top (HEADER_H tall) with a divider line.
lv_obj_t* header(lv_obj_t* parent, const char* title);
// Small dim hint text along the bottom edge, e.g. "TURN: MOVE  CLICK: JUMP".
lv_obj_t* footer(lv_obj_t* parent, const char* hint);
// Centred message box (title + optional value + hint), e.g. for GAME OVER.
// Hide/show with lv_obj_set_hidden; access labels with popupTitle/Value/Hint.
lv_obj_t* popup(lv_obj_t* parent, const char* title, const char* hint,
                std::uint32_t title_rgb = color::ACCENT);
lv_obj_t* popupTitle(lv_obj_t* popup);
lv_obj_t* popupHint(lv_obj_t* popup);
lv_obj_t* popupValue(lv_obj_t* popup);
// Shared results layout: a large score, saved best, then the restart hint.
void result(lv_obj_t* popup, std::uint32_t score, std::uint32_t best, bool new_best);

// A vertical, encoder-driven list (launcher, system and pause menus use it).
// Feed it the items and the selected index whenever either changes; it
// scrolls to keep the selection visible.
struct MenuItem {
    const char* name;
    const char* value = nullptr;      // optional right-aligned value
    std::uint32_t color = color::ACCENT;  // highlight colour when selected
    bool chip = false;                // draw a coloured letter badge before the name
};

class Menu {
public:
    static constexpr std::uint8_t MAX_ROWS = 6;
    static constexpr int ROW_H = 20;

    void create(lv_obj_t* parent, int x, int y, int w, std::uint8_t rows);
    void show(const MenuItem* items, std::uint8_t count, std::uint8_t selected,
              bool editing = false);

private:
    lv_obj_t* rows_[MAX_ROWS] = {};
    lv_obj_t* chips_[MAX_ROWS] = {};
    lv_obj_t* badges_[MAX_ROWS] = {};
    lv_obj_t* names_[MAX_ROWS] = {};
    lv_obj_t* values_[MAX_ROWS] = {};
    lv_obj_t* more_up_ = nullptr;
    lv_obj_t* more_down_ = nullptr;
    std::uint8_t row_count_ = 0;
    std::uint8_t first_ = 0;
};

}  // namespace ui

// Installs the theme and styles. Called once by the OS at boot.
void styleInit(lv_display_t* display);

}  // namespace tg
