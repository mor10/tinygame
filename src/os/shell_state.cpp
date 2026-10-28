#include "shell_state.h"

namespace shell {

void State::begin(std::uint8_t game_count, const settings::Settings& initial) {
    *this = State{};
    game_count_ = game_count;
    settings_ = settings::sanitize(initial);
}

std::uint8_t State::itemCount() const {
    switch (screen_) {
        case Screen::Launcher: return static_cast<std::uint8_t>(game_count_ + 1);
        case Screen::System: return SYS_COUNT;
        case Screen::Pause: return PAUSE_COUNT;
        case Screen::Settings: return settings::ITEM_COUNT + 1;
        default: return 0;
    }
}

std::uint8_t State::move(std::uint8_t current, std::uint8_t count,
                         std::int32_t transitions) {
    if (count == 0) return 0;
    const std::int64_t total = static_cast<std::int64_t>(remainder_) + transitions;
    const std::int64_t next = current + total / 4;
    remainder_ = static_cast<std::int32_t>(total % 4);
    if (next <= 0) {
        if (next < 0 || remainder_ < 0) remainder_ = 0;
        return 0;
    }
    if (next >= count - 1) {
        if (next > count - 1 || remainder_ > 0) remainder_ = 0;
        return count - 1;
    }
    return static_cast<std::uint8_t>(next);
}

void State::go(Screen screen) {
    screen_ = screen;
    editing_ = false;
    selected_ = 0;
    remainder_ = 0;
}

Action State::launch(std::uint8_t game_index) {
    if (game_index >= game_count_) return Action::None;
    game_ = game_index;
    go(Screen::Game);
    return Action::Launch;
}

Action State::update(std::int32_t transitions, bool click_event, bool hold_event) {
    if (screen_ == Screen::Settings) {
        updateSettings(transitions, click_event, hold_event);
        return Action::None;
    }
    if (hold_event) return hold();
    selected_ = move(selected_, itemCount(), transitions);
    if (click_event) return click();
    return Action::None;
}

Action State::hold() {
    switch (screen_) {
        case Screen::Launcher: go(Screen::Welcome); break;
        case Screen::Game: go(Screen::Pause); return Action::Pause;
        case Screen::Pause: go(Screen::Game); return Action::Resume;
        case Screen::System: go(Screen::Launcher); break;
        case Screen::Diagnostics:
        case Screen::Style: go(Screen::System); break;
        default: break;
    }
    return Action::None;
}

Action State::click() {
    switch (screen_) {
        case Screen::Welcome: go(Screen::Launcher); break;
        case Screen::Launcher:
            if (selected_ < game_count_) return launch(selected_);
            go(Screen::System);
            break;
        case Screen::Pause:
            if (selected_ == PAUSE_RESUME) {
                go(Screen::Game);
                return Action::Resume;
            }
            if (selected_ == PAUSE_RESTART) {
                go(Screen::Game);
                return Action::Restart;
            }
            go(Screen::Launcher);
            return Action::Quit;
        case Screen::System:
            switch (selected_) {
                case SYS_SETTINGS: go(Screen::Settings); break;
                case SYS_DIAGNOSTICS: go(Screen::Diagnostics); break;
                case SYS_STYLE: go(Screen::Style); break;
                case SYS_SLEEP: return Action::Sleep;
                default: go(Screen::Launcher); break;
            }
            break;
        case Screen::Style: go(Screen::System); break;
        default: break;
    }
    return Action::None;
}

void State::updateSettings(std::int32_t transitions, bool click_event, bool hold_event) {
    if (editing_) {
        if (hold_event) {
            settings_ = settings_before_edit_;
            editing_ = false;
            remainder_ = 0;
            return;
        }
        const auto index = move(settings::valueIndex(settings_, selected_),
                                settings::valueCount(selected_), transitions);
        settings::setValueIndex(settings_, selected_, index);
        if (click_event) {
            editing_ = false;
            remainder_ = 0;
        }
        return;
    }
    if (hold_event) {
        go(Screen::System);
        return;
    }
    selected_ = move(selected_, itemCount(), transitions);
    if (click_event) {
        if (selected_ == settings::ITEM_COUNT) {
            go(Screen::System);
        } else {
            settings_before_edit_ = settings_;
            editing_ = true;
            remainder_ = 0;
        }
    }
}

const char* screenName(Screen screen) {
    switch (screen) {
        case Screen::Welcome: return "WELCOME";
        case Screen::Launcher: return "LAUNCHER";
        case Screen::Game: return "GAME";
        case Screen::Pause: return "PAUSE";
        case Screen::System: return "SYSTEM";
        case Screen::Settings: return "SETTINGS";
        case Screen::Diagnostics: return "DIAGNOSTICS";
        case Screen::Style: return "STYLE";
    }
    return "?";
}

}  // namespace shell
