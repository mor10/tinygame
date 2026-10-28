#include <cassert>
#include <climits>
#include <cstdio>

#include "shell_state.h"
#include "tg_types.h"

using shell::Action;
using shell::Screen;

shell::State launcher(std::uint8_t games = 2) {
    shell::State s;
    s.begin(games, {});
    s.update(100, true, false);
    assert(s.screen() == Screen::Launcher && s.selected() == 0);
    return s;
}

void relativeMovementIsClampedAndKeepsFractions() {
    auto s = launcher();
    s.update(3, false, false);
    assert(s.selected() == 0);
    s.update(1, false, false);
    assert(s.selected() == 1);
    s.update(-3, false, false);
    assert(s.selected() == 1);
    s.update(-1, false, false);
    assert(s.selected() == 0);
    s.update(INT32_MIN, false, false);
    s.update(4, false, false);
    assert(s.selected() == 1);
    s.update(INT32_MAX, false, false);
    assert(s.selected() == 2);
    s.update(3, false, false);
    s.update(-4, false, false);
    assert(s.selected() == 1);
    s.update(0, false, true);
    assert(s.screen() == Screen::Welcome);
}

shell::State paused() {
    auto s = launcher();
    assert(s.update(0, true, false) == Action::Launch);
    assert(s.update(100, false, true) == Action::Pause);
    assert(s.selected() == shell::PAUSE_RESUME);
    return s;
}

void pauseSupportsAllActionsAndIgnoresOpeningMovement() {
    auto s = paused();
    assert(s.update(0, true, false) == Action::Resume);
    s = paused();
    assert(s.update(0, false, true) == Action::Resume);
    s = paused();
    s.update(4, false, false);
    assert(s.selected() == shell::PAUSE_RESTART);
    assert(s.update(0, true, false) == Action::Restart);
    s = paused();
    s.update(8, false, false);
    assert(s.update(0, true, false) == Action::Quit);
    assert(s.screen() == Screen::Launcher && s.selected() == 0);
}

shell::State system() {
    auto s = launcher(0);
    assert(s.itemCount() == 1);
    s.update(0, true, false);
    assert(s.screen() == Screen::System);
    return s;
}

void systemPagesAndDiagnosticsClicks() {
    auto s = system();
    s.update(4, true, false);
    assert(s.screen() == Screen::Diagnostics);
    s.update(100, true, false);
    assert(s.screen() == Screen::Diagnostics);
    s.update(0, false, true);
    assert(s.screen() == Screen::System);
    s.update(8, true, false);
    assert(s.screen() == Screen::Style);
    s.update(0, true, false);
    assert(s.screen() == Screen::System);
    s.update(12, true, false);
    assert(s.screen() == Screen::System);
    assert(s.update(0, true, false) == Action::Sleep);
    s.update(4, true, false);
    assert(s.screen() == Screen::Launcher);
}

void settingsAreRelativeAndCancelRestoresValue() {
    auto s = system();
    s.update(0, true, false);
    assert(s.screen() == Screen::Settings);
    const auto original = s.settings();
    s.update(0, true, false);
    assert(s.editing());
    s.update(-4, false, false);
    assert(s.settings().brightness() == 80);
    s.update(0, false, true);
    assert(!s.editing() && s.settings() == original);
    s.update(0, true, false);
    s.update(4, false, false);
    assert(s.settings().brightness() == 100);
    s.update(0, true, false);
    assert(!s.editing());
    s.update(4, false, false);
    assert(s.selected() == settings::LED);
    s.update(0, true, false);
    s.update(-4, false, false);
    assert(s.settings().led() == 25);
    s.update(0, true, false);
    s.update(4, false, false);
    assert(s.selected() == settings::HANDEDNESS && !s.settings().leftHanded());
    s.update(0, true, false);
    s.update(4, false, false);
    assert(s.settings().leftHanded());
    s.update(0, true, false);
    s.update(4, false, false);
    s.update(0, true, false);
    assert(s.screen() == Screen::System);
}

void modeChangesResetFractionalMovement() {
    auto s = system();
    s.update(0, true, false);
    s.update(3, false, false);
    s.update(0, true, false);
    s.update(1, false, false);
    assert(s.settings().brightness() == 90);
    s.update(0, true, false);
    s.update(3, false, false);
    assert(s.selected() == 0);
}

void launchAndSettingsRemainValid() {
    auto s = launcher();
    s.update(4, true, false);
    assert(s.screen() == Screen::Game && s.game() == 1);
    assert(s.update(100, true, false) == Action::None);
    assert(s.launch(99) == Action::None);
    assert(s.launch(0) == Action::Launch);
    settings::Settings bad;
    bad.value[settings::BRIGHTNESS] = 0;
    assert(settings::sanitize(bad) == settings::Settings{});
}

int main() {
    relativeMovementIsClampedAndKeepsFractions();
    pauseSupportsAllActionsAndIgnoresOpeningMovement();
    systemPagesAndDiagnosticsClicks();
    settingsAreRelativeAndCancelRestoresValue();
    modeChangesResetFractionalMovement();
    launchAndSettingsRemainValid();
    std::puts("shell_state tests passed");
}
