#include "view.h"

#include <cmath>

namespace maze {

void View::build(lv_obj_t* screen) {
    tg::ui::block(screen, 0, 0, tg::WIDTH, 18, tg::color::SURFACE);
    tg::ui::block(screen, 0, 16, 32, 2, tg::color::MINT);
    hud_ = tg::ui::label(screen, "", tg::Font::S);
    lv_obj_set_pos(hud_, 8, 4);
    for (int w = 0; w < WALLS; ++w) {
        const Rect r = Model::wallRect(w);
        walls_[w] = tg::ui::block(screen, r.x, r.y, r.w, r.h, tg::color::SLATE);
        lv_obj_set_hidden(walls_[w], true);
    }
    goal_[0] = tg::ui::block(screen, 0, 0, 12, 12, tg::color::GOOD);
    goal_[1] = tg::ui::block(screen, 0, 0, 8, 8, tg::color::BG);
    for (int i = 0; i < MAX_HAZARDS; ++i) {
        for (int part = 0; part < 3; ++part) {
            lasers_[i][part] = tg::ui::block(screen, 0, 0, 4, 4, tg::color::LINE);
            lv_obj_set_hidden(lasers_[i][part], true);
        }
        traps_[i][0] = tg::ui::block(screen, 0, 0, 12, 12, tg::color::BAD);
        traps_[i][1] = tg::ui::block(screen, 0, 0, 10, 10, tg::color::BG);
        traps_[i][2] = tg::ui::label(screen, "X", tg::Font::S, tg::color::BAD);
        for (auto* part : traps_[i]) lv_obj_set_hidden(part, true);
        gates_[i][0] = tg::ui::block(screen, 0, 0, 2, 5, tg::color::PINK);
        gates_[i][1] = tg::ui::block(screen, 0, 0, 2, 5, tg::color::PINK);
        gates_[i][2] = tg::ui::label(screen, "", tg::Font::S, tg::color::PINK);
        for (auto* part : gates_[i]) lv_obj_set_hidden(part, true);
    }
    ball_[0] = tg::ui::block(screen, 0, 0, 8, 4, tg::color::WHITE);
    ball_[1] = tg::ui::block(screen, 0, 0, 4, 8, tg::color::WHITE);
    ball_[2] = tg::ui::block(screen, 0, 0, 2, 2, tg::color::MINT);
    popup_ = tg::ui::popup(screen, "TIME UP", "HOLD: PAUSE > RESTART", tg::color::BAD);
    lv_obj_set_hidden(popup_, true);
    picker_ = tg::ui::block(screen, 0, 0, tg::WIDTH, tg::HEIGHT, tg::color::BG);
    tg::ui::header(picker_, "MAZE");
    tg::ui::block(picker_, 32, 32, 256, 2, tg::color::LINE);
    menu_.create(picker_, 32, 44, 264, 2);
    description_ = tg::ui::label(picker_, "", tg::Font::S, tg::color::TEXT_DIM);
    lv_obj_set_pos(description_, 32, 104);
    tg::ui::footer(picker_, "TURN: MODE  CLICK: START");
}

void View::render(const State& s) {
    if (s.stage != stage_) {
        lv_obj_set_hidden(picker_, s.stage != Stage::Picker);
        lv_obj_set_hidden(popup_, s.stage != Stage::Over && s.stage != Stage::Transition);
        if (s.stage == Stage::Over || s.stage == Stage::Transition) {
            lv_label_set_text(tg::ui::popupTitle(popup_),
                              s.stage == Stage::Over ? "TIME UP" : "NICE!");
            lv_obj_set_style_text_color(tg::ui::popupTitle(popup_),
                lv_color_hex(s.stage == Stage::Over ? tg::color::BAD : tg::color::GOOD), 0);
            lv_label_set_text(tg::ui::popupHint(popup_),
                s.stage == Stage::Over ? "HOLD: PAUSE > RESTART" : "NEXT MAZE...");
            tg::ui::text(tg::ui::popupValue(popup_), "%lu", static_cast<unsigned long>(s.completed));
        }
        stage_ = s.stage;
    }
    if (s.stage == Stage::Picker) {
        const int selection = static_cast<int>(s.mode);
        if (selected_ != selection) {
            const tg::ui::MenuItem items[] = {{"CHILL"}, {"THRILL", nullptr, tg::color::WARN}};
            menu_.show(items, 2, selection);
            lv_label_set_text(description_, selection == 0
                ? "TILT: ROLL\nNO TIMER. JUST MAZES."
                : "TILT: ROLL\nBEAT THE CLOCK. DODGE HAZARDS.");
            selected_ = selection;
        }
        return;
    }
    if (revision_ != s.revision) {
        for (int wall = 0; wall < WALLS; ++wall) {
            lv_obj_set_hidden(walls_[wall], !s.walls[wall]);
        }
        const Vec2 goal = Model::center(s.goal);
        lv_obj_set_pos(goal_[0], goal.x - 6, goal.y - 6);
        lv_obj_set_pos(goal_[1], goal.x - 4, goal.y - 4);
        for (int i = 0; i < MAX_HAZARDS; ++i) {
            for (auto* part : gates_[i]) lv_obj_set_hidden(part, i >= s.gate_count);
            if (i < s.gate_count) {
                const Gate& gate = s.gates[i];
                lv_obj_set_hidden(walls_[gate.wall], true);
                for (int end = 0; end < 2; ++end) {
                    const Rect r = Model::gatePost(gate.wall, end != 0);
                    lv_obj_set_pos(gates_[i][end], r.x, r.y);
                    lv_obj_set_size(gates_[i][end], r.w, r.h);
                }
                const Vec2 a = Model::center(gate.from);
                const Vec2 b = Model::center(gate.to);
                lv_label_set_text(gates_[i][2],
                    b.x > a.x ? ">" : b.x < a.x ? "<" : b.y > a.y ? "v" : "^");
                lv_obj_set_pos(gates_[i][2], (a.x + b.x) / 2 - 4, (a.y + b.y) / 2 - 4);
            }
            for (auto* part : traps_[i]) lv_obj_set_hidden(part, i >= s.trap_count);
            if (i < s.trap_count) {
                const Vec2 trap = Model::center(s.traps[i]);
                lv_obj_set_pos(traps_[i][0], trap.x - 6, trap.y - 6);
                lv_obj_set_pos(traps_[i][1], trap.x - 5, trap.y - 5);
                lv_obj_set_pos(traps_[i][2], trap.x - 4, trap.y - 4);
            }
            for (auto* part : lasers_[i]) lv_obj_set_hidden(part, i >= s.laser_count);
            laser_phase_[i] = -1;
            if (i < s.laser_count) {
                const Rect r = Model::wallRect(s.lasers[i].wall);
                const bool horizontal = r.w > r.h;
                const int x = r.x + r.w / 2;
                const int y = r.y + r.h / 2;
                lv_obj_set_pos(lasers_[i][1],
                               horizontal ? r.x - 1 : x - 2,
                               horizontal ? y - 2 : r.y - 1);
                lv_obj_set_pos(lasers_[i][2],
                               horizontal ? r.x + r.w - 3 : x - 2,
                               horizontal ? y - 2 : r.y + r.h - 3);
            }
        }
        revision_ = s.revision;
    }
    for (int i = 0; i < s.laser_count; ++i) {
        const LaserPhase phase = Model::laserPhase(s.lasers[i], s.clock);
        if (laser_phase_[i] == static_cast<int>(phase)) continue;
        const Rect r = Model::wallRect(s.lasers[i].wall);
        const bool horizontal = r.w > r.h;
        const bool active = phase == LaserPhase::Active;
        const std::uint32_t color = active ? tg::color::BAD
            : phase == LaserPhase::Warning ? tg::color::WARN : tg::color::LINE;
        lv_obj_set_hidden(lasers_[i][0], phase == LaserPhase::Off);
        lv_obj_set_pos(lasers_[i][0],
            active ? r.x : r.x + (horizontal ? 7 : 0),
            active ? r.y : r.y + (horizontal ? 0 : 7));
        lv_obj_set_size(lasers_[i][0], active ? r.w : horizontal ? 12 : 1,
                        active ? r.h : horizontal ? 1 : 12);
        for (auto* part : lasers_[i]) {
            lv_obj_set_style_bg_color(part, lv_color_hex(color), 0);
        }
        laser_phase_[i] = static_cast<int>(phase);
    }
    const int x = static_cast<int>(std::lround(s.ball.x));
    const int y = static_cast<int>(std::lround(s.ball.y));
    if (x != ball_x_ || y != ball_y_) {
        lv_obj_set_pos(ball_[0], x - 4, y - 2);
        lv_obj_set_pos(ball_[1], x - 2, y - 4);
        lv_obj_set_pos(ball_[2], x - 2, y - 2);
        ball_x_ = x;
        ball_y_ = y;
    }
    const int seconds = static_cast<int>(std::ceil(s.remaining));
    const bool protection = s.protection > 0;
    if (protection != protected_) {
        for (int part = 0; part < 2; ++part) {
            lv_obj_set_style_bg_color(ball_[part],
                lv_color_hex(protection ? tg::color::SKY : tg::color::WHITE), 0);
        }
    }
    if (completed_ != s.completed || seconds_ != seconds || protection != protected_) {
        if (s.mode == Mode::Chill) {
            tg::ui::text(hud_, "CHILL  MAZES %lu", static_cast<unsigned long>(s.completed));
        } else {
            tg::ui::text(hud_, "THRILL MAZES %lu TIME %d%s",
                         static_cast<unsigned long>(s.completed), seconds, protection ? " SAFE" : "");
        }
        protected_ = protection;
        completed_ = s.completed;
        seconds_ = seconds;
    }
}

}  // namespace maze
