#pragma once

#include "tinygame.h"
#include "model.h"

namespace maze {

class View {
public:
    void build(lv_obj_t* screen);
    void render(const State& state);

private:
    lv_obj_t* walls_[WALLS] = {};
    lv_obj_t* ball_[3] = {};
    lv_obj_t* goal_[2] = {};
    lv_obj_t* lasers_[MAX_HAZARDS][3] = {};
    lv_obj_t* traps_[MAX_HAZARDS][3] = {};
    lv_obj_t* gates_[MAX_HAZARDS][3] = {};
    int laser_phase_[MAX_HAZARDS] = {-1, -1, -1, -1};
    lv_obj_t* hud_ = nullptr;
    lv_obj_t* picker_ = nullptr;
    lv_obj_t* description_ = nullptr;
    lv_obj_t* popup_ = nullptr;
    tg::ui::Menu menu_;
    std::uint32_t revision_ = 0;
    int ball_x_ = -1;
    int ball_y_ = -1;
    int selected_ = -1;
    int seconds_ = -1;
    std::uint32_t completed_ = UINT32_MAX;
    Stage stage_ = Stage::Over;
    bool protected_ = false;
};

}  // namespace maze
