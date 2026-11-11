// Carried over from v1.0 tests/test_input.cpp. Converted to the v2 harness (engine/test/tg_test.h):
// assert() became CHECK(), so one failure no longer stops the others.
#include "../../engine/test/tg_test.h"
#include <climits>
#include <initializer_list>

#include "input_logic.h"

using namespace input;

TG_TEST(quadratureTracksElectricalMovement) {
    Quadrature q;
    q.initialize(3);
    int total = 0;
    for (int revolution = 0; revolution < 20; ++revolution) {
        for (auto ab : {2, 0, 1, 3}) total += q.sample(ab);
    }
    CHECK(total == 80);
    for (auto ab : {1, 0, 2, 3}) total += q.sample(ab);
    CHECK(total == 76);
    CHECK(q.sample(3) == 0);
    CHECK(q.sample(0) == 0);
    CHECK(q.invalid == 1);
    CHECK(q.sample(1) == 1);
    const int cycle[] = {0, 1, 3, 2};
    for (int phase = 0; phase < 4; ++phase) {
        q.initialize(cycle[phase]);
        int movement = 0;
        for (int edge = 1; edge <= 4; ++edge)
            movement += q.sample(cycle[(phase + edge) % 4]);
        CHECK(movement == 4);
    }
    q.initialize(3);
    q.invalid = UINT32_MAX;
    CHECK(q.sample(0) == 0 && q.invalid == UINT32_MAX);
}

TG_TEST(bounceAndPartialReversalsCancel) {
    Quadrature q;
    q.initialize(3);
    int total = 0;
    for (auto ab : {2, 3, 2, 0, 2, 3}) total += q.sample(ab);
    CHECK(total == 0);
    for (int initial = 0; initial < 4; ++initial) {
        q.initialize(initial);
        CHECK(q.sample(initial) == 0);
    }
    CHECK(boundedAdd(INT32_MAX, 1) == INT32_MAX);
    CHECK(boundedAdd(INT32_MIN, -1) == INT32_MIN);
    CHECK(boundedAdd(INT32_MIN, INT32_MAX) == -1);
}

TG_TEST(buttonsDebounceAndOwnTheirEvents) {
    Button b;
    b.initialize(false, 0);
    b.update(true, 1);
    b.update(false, 10);
    b.update(true, 20);
    b.update(true, 44);
    CHECK(b.events == 0);
    b.update(true, 45);
    CHECK(b.events == EVENT_PRESSED && b.down);
    b.update(false, 100);
    b.update(false, 125);
    CHECK(b.events == (EVENT_RELEASED | EVENT_SHORT_PRESS));
    CHECK(b.clicks == 1 && !b.down);
    b.update(false, 126);
    CHECK(b.events == 0 && b.clicks == 1);

    b.update(true, 200);
    b.update(true, 225);
    b.update(true, 850);
    CHECK(b.events == EVENT_LONG_PRESS);
    b.update(true, 851);
    CHECK(b.events == 0);
    b.update(false, 900);
    b.update(false, 925);
    CHECK(b.events == EVENT_RELEASED && b.clicks == 1);
    CHECK(b.last_event == ButtonEvent::LongPress);
}

TG_TEST(initialHoldsAreIgnoredUntilReleased) {
    Button b;
    b.initialize(true, 1000);
    b.update(false, 1100);
    b.update(false, 1125);
    CHECK(b.events == 0 && !b.down);
    b.update(true, 1200);
    b.update(true, 1225);
    CHECK(b.events == EVENT_PRESSED);
    b.update(false, 1300);
    b.update(false, 1325);
    CHECK(b.events & EVENT_SHORT_PRESS);

    Button other;
    other.initialize(false, 0);
    other.update(false, 1000);
    CHECK(other.events == 0 && other.clicks == 0);
}

// The 32-bit millisecond clock wraps after about 49 days. Every comparison must
// use subtraction, so debounce, held time and long press must survive the wrap.
TG_TEST(timingSurvivesTheClockWrap) {
    const std::uint32_t t0 = UINT32_MAX - 20;  // 21 ms before the wrap
    Button b;
    b.initialize(false, t0 - 1000);
    b.update(true, t0);
    b.update(true, t0 + 24);
    CHECK(b.events == 0 && !b.down);
    b.update(true, t0 + 25);  // the wrap happens between t0 and here
    CHECK(b.events == EVENT_PRESSED && b.down);
    b.update(true, t0 + 649);
    CHECK(b.events == 0 && b.held_ms == 649);
    b.update(true, t0 + 650);
    CHECK(b.events == EVENT_LONG_PRESS && b.held_ms == 650);
    b.update(false, t0 + 700);
    b.update(false, t0 + 725);
    CHECK(b.events == EVENT_RELEASED && b.clicks == 0);

    Button c;  // a short click that starts before the wrap and ends after it
    c.initialize(false, t0 - 1000);
    c.update(true, t0);
    c.update(true, t0 + 25);
    c.update(false, t0 + 60);
    c.update(false, t0 + 85);
    CHECK(c.events == (EVENT_RELEASED | EVENT_SHORT_PRESS) && c.clicks == 1);
}

TG_TEST(aLongPressSurvivesASlowConsumer) {
    Button b;
    b.initialize(false, 0);
    b.update(true, 1);
    b.update(true, 26);
    b.update(false, 800);
    b.update(false, 825);
    CHECK(b.events == (EVENT_RELEASED | EVENT_LONG_PRESS));
    CHECK(b.last_event == ButtonEvent::LongPress && b.clicks == 0);
}

int main() { return tg::test::run(); }
