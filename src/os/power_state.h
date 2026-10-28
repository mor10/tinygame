#pragma once

#include "input_logic.h"

namespace power_state {

inline constexpr std::uint32_t IDLE_TIMEOUT_MS = 3UL * 60 * 1000;
inline constexpr std::uint32_t PANEL_SETTLE_MS = 120;

class State {
public:
    void initialize(std::uint32_t now) {
        *this = State{};
        last_activity_ = released_at_ = now;
    }
    void update(std::uint32_t now, bool activity, bool either_pressed) {
        now_ = now;
        if (either_pressed || was_pressed_) released_at_ = now;
        was_pressed_ = either_pressed;
        released_ = !either_pressed && now - released_at_ >= input::DEBOUNCE_MS;
        if (!accepting_) {
            accepting_ = released_;
            last_activity_ = now;
        }
        if (activity || either_pressed) last_activity_ = now;
        if (accepting_ && now - last_activity_ >= IDLE_TIMEOUT_MS) request();
    }
    void request() { pending_ = true; }
    void cancel(std::uint32_t now) {
        pending_ = settling_ = false;
        last_activity_ = now;
    }
    void begin(std::uint32_t now) {
        settling_ = true;
        started_at_ = now;
    }
    bool acceptingInput() const { return accepting_; }
    bool pending() const { return pending_; }
    bool canPrepare() const { return pending_ && released_; }
    bool settling() const { return settling_; }
    bool ready() const {
        return settling_ && released_ && now_ - started_at_ >= PANEL_SETTLE_MS;
    }
private:
    bool accepting_ = false;
    bool pending_ = false;
    bool settling_ = false;
    bool was_pressed_ = false;
    bool released_ = false;
    std::uint32_t now_ = 0;
    std::uint32_t last_activity_ = 0;
    std::uint32_t released_at_ = 0;
    std::uint32_t started_at_ = 0;
};

}  // namespace power_state
