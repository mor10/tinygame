#pragma once

#include <cstdint>

#include "settings.h"

// The console's menu logic: which screen is showing, what is selected, and
// when to start, pause, restart, or quit a game. Pure C++ (host-tested); the
// OS turns the returned Actions into real work and shell_ui.cpp draws it.
namespace shell {

enum class Screen : std::uint8_t {
    Welcome,
    Launcher,
    Game,
    Pause,
    System,
    Settings,
    Diagnostics,
    Style,
};

enum class Action : std::uint8_t { None, Launch, Restart, Quit, Pause, Resume, Sleep };

enum SystemItem : std::uint8_t { SYS_SETTINGS, SYS_DIAGNOSTICS, SYS_STYLE, SYS_SLEEP, SYS_BACK, SYS_COUNT };
enum PauseItem : std::uint8_t { PAUSE_RESUME, PAUSE_RESTART, PAUSE_QUIT, PAUSE_COUNT };

class State {
public:
    void begin(std::uint8_t game_count, const settings::Settings& initial);

    // One frame of input. `click` = short press, `hold` = long press.
    Action update(std::int32_t transitions, bool click, bool hold);
    // Jumps straight into a game (used by the dev tools).
    Action launch(std::uint8_t game_index);

    Screen screen() const { return screen_; }
    std::uint8_t selected() const { return selected_; }
    bool editing() const { return editing_; }
    // The running game, or the last one played.
    std::uint8_t game() const { return game_; }
    std::uint8_t gameCount() const { return game_count_; }
    const settings::Settings& settings() const { return settings_; }

    // Number of choices on the current screen (0 if it has no menu).
    std::uint8_t itemCount() const;

private:
    void go(Screen screen);
    Action click();
    Action hold();
    void updateSettings(std::int32_t transitions, bool click, bool hold);
    std::uint8_t move(std::uint8_t current, std::uint8_t count,
                      std::int32_t transitions);

    Screen screen_ = Screen::Welcome;
    std::uint8_t selected_ = 0;
    bool editing_ = false;
    std::uint8_t game_ = 0;
    std::uint8_t game_count_ = 0;
    settings::Settings settings_;
    settings::Settings settings_before_edit_;
    std::int32_t remainder_ = 0;
};

const char* screenName(Screen screen);

}  // namespace shell
