#include <cassert>

#include "persistence.h"

int main() {
    persistence::BestScore best{10};
    int writes = 0;
    const auto fail = [&](std::uint32_t) { ++writes; return false; };
    assert(!best.submit(9, fail) && writes == 0);
    assert(!best.submit(20, fail) && best.value == 10 && writes == 1);
    assert(best.submit(20, [](std::uint32_t) { return true; }) && best.value == 20);
    assert(!best.submit(20, fail) && writes == 1);

    persistence::SettingsSave save;
    settings::Settings original;
    save.begin(original);
    auto changed = original;
    changed.value[settings::BRIGHTNESS] = 50;
    assert(!save.needed(changed, true));
    assert(save.needed(changed, false));
    save.attempted(changed, false);
    assert(!save.needed(changed, false));  // Failed flash writes never spin in the loop.
    assert(!save.needed(changed, true));
    assert(save.needed(changed, false));  // A deliberate confirmation retries.
    save.attempted(changed, true);
    assert(!save.needed(changed, false));
    assert(!save.needed(original, true));
    assert(!save.needed(changed, false)); // Cancel returns to the saved value.
}
