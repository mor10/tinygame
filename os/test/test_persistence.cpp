// Carried over from v1.0 tests/test_persistence.cpp. Converted to the v2 harness (engine/test/tg_test.h):
// assert() became CHECK(), so one failure no longer stops the others.
#include "../../engine/test/tg_test.h"

#include "persistence.h"

TG_TEST(best_score_and_settings_save_rules) {
    persistence::BestScore best{10};
    int writes = 0;
    const auto fail = [&](std::uint32_t) { ++writes; return false; };
    CHECK(!best.submit(9, fail) && writes == 0);
    CHECK(!best.submit(20, fail) && best.value == 10 && writes == 1);
    CHECK(best.submit(20, [](std::uint32_t) { return true; }) && best.value == 20);
    CHECK(!best.submit(20, fail) && writes == 1);

    persistence::SettingsSave save;
    settings::Settings original;
    save.begin(original);
    auto changed = original;
    changed.value[settings::BRIGHTNESS] = 50;
    CHECK(!save.needed(changed, true));
    CHECK(save.needed(changed, false));
    save.attempted(changed, false);
    CHECK(!save.needed(changed, false));  // Failed flash writes never spin in the loop.
    CHECK(!save.needed(changed, true));
    CHECK(save.needed(changed, false));  // A deliberate confirmation retries.
    save.attempted(changed, true);
    CHECK(!save.needed(changed, false));
    CHECK(!save.needed(original, true));
    CHECK(!save.needed(changed, false)); // Cancel returns to the saved value.
}

int main() { return tg::test::run(); }
