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
    assert(s.selected() == settings::AUTO_OFF && s.settings().autoOff());
    s.update(0, true, false);
    s.update(-4, false, false);
    assert(!s.settings().autoOff());
    s.update(0, false, true);
    assert(s.settings().autoOff());
    s.update(0, true, false);
    s.update(-4, false, false);
    s.update(0, true, false);
    assert(!s.settings().autoOff() && !s.editing());
    s.update(4, false, false);
    assert(s.selected() == settings::AUTO_OFF_TIME);
    s.update(0, true, false);
    s.update(100, false, false);
    assert(s.settings().autoOffTimeoutMs() == 1800000);
    s.update(0, false, true);
    assert(s.settings().autoOffTimeoutMs() == 300000);
    s.update(0, true, false);
    s.update(-4, false, false);
    s.update(0, true, false);
    assert(s.settings().autoOffTimeoutMs() == 60000);
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

void sleepConfirmationPreservesGamesAndMenus() {
    auto s = launcher();
    assert(s.launch(1) == Action::Launch);
    assert(s.confirmSleep() == Action::Pause);
    assert(s.sleepConfirming() && s.sleepSelected() == 0);
    assert(s.screen() == Screen::Game && s.game() == 1);
    assert(s.confirmSleep() == Action::None); // No resetting or double-pausing.
    assert(s.update(3, false, false) == Action::None);
    assert(s.sleepSelected() == 0);
    assert(s.update(1, true, false) == Action::Resume);
    assert(!s.sleepConfirming() && s.screen() == Screen::Game && s.game() == 1);
    assert(s.confirmSleep() == Action::Pause);
    assert(s.update(0, false, false) == Action::None); // No automatic Yes.
    assert(s.update(0, true, false) == Action::Sleep);
    assert(s.sleepConfirming()); // Failed sleep preparation can retry or cancel.
    assert(s.update(4, true, false) == Action::Resume);

    s = paused();
    s.update(4, false, false);
    assert(s.confirmSleep() == Action::None);
    assert(s.update(4, true, false) == Action::None);
    assert(s.screen() == Screen::Pause && s.selected() == shell::PAUSE_RESTART);

    s = system();
    s.update(0, true, false); // Settings.
    s.update(0, true, false); // Editing brightness.
    s.update(-4, false, false);
    const auto edited = s.settings();
    s.update(3, false, false); // Preserve partial knob movement too.
    assert(s.confirmSleep() == Action::None);
    assert(s.update(4, true, false) == Action::None);
    assert(s.screen() == Screen::Settings && s.editing() && s.settings() == edited);
    s.update(1, false, false);
    assert(s.settings().brightness() == edited.brightness() + 10);
    assert(s.confirmSleep() == Action::None);
    s.update(0, false, true); // A single hold cancels; it never confirms Yes.
    assert(!s.sleepConfirming() && s.editing());

    s.begin(2, {});
    assert(s.confirmSleep() == Action::None);
    s.update(4, true, false);
    assert(s.screen() == Screen::Welcome);
    s = launcher();
    s.update(4, false, false);
    s.confirmSleep();
    s.update(4, true, false);
    assert(s.screen() == Screen::Launcher && s.selected() == 1);
    s = system();
    s.update(4, true, false); // Diagnostics.
    s.confirmSleep();
    s.update(4, true, false);
    assert(s.screen() == Screen::Diagnostics);
}

int main() {
    relativeMovementIsClampedAndKeepsFractions();
    pauseSupportsAllActionsAndIgnoresOpeningMovement();
    systemPagesAndDiagnosticsClicks();
    settingsAreRelativeAndCancelRestoresValue();
    modeChangesResetFractionalMovement();
    launchAndSettingsRemainValid();
    sleepConfirmationPreservesGamesAndMenus();
    std::puts("shell_state tests passed");
}
