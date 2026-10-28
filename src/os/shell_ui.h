#pragma once

#include <lvgl.h>

#include <cstdint>

#include "imu.h"
#include "input.h"
#include "input_adapter.h"
#include "shell_state.h"

// Draws the OS screens (welcome, launcher, system pages, pause menu) for the
// current shell::State. Each page is its own LVGL screen, built when it opens
// and deleted when it closes. Games draw themselves.
namespace shell_ui {

struct Live {
    const input::State& input;
    const imu::Reading& imu;
    bool imu_present;
    bool imu_ok;
    const input_adapter::Evidence& effective_left;
    const input_adapter::Evidence& effective_right;
    bool accepting_input;
    const char* wake_cause;
};

// Call every loop after the OS has applied the shell's Action.
// `game_screen` is the running game's screen (or nullptr).
void update(const shell::State& state, lv_obj_t* game_screen, const Live& live,
            std::uint32_t now_ms);

}  // namespace shell_ui
