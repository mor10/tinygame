// New in v2: tests for os/frame_pacer.h.
#include "../../engine/test/tg_test.h"

#include "frame_pacer.h"

TG_TEST(at_most_thirty_frames_a_second) {
    frame_pacer::Pacer pacer;
    int frames = 0;
    for (std::uint32_t t = 1000; t < 1000 + 1'000'000; t += 500) frames += pacer.due(t) ? 1 : 0;
    CHECK(frames >= 30 && frames <= 31);
}

TG_TEST(first_call_is_due_and_the_beat_does_not_drift) {
    frame_pacer::Pacer pacer;
    CHECK(pacer.due(5000));
    CHECK(!pacer.due(5000 + 33'332));
    CHECK(pacer.due(5000 + 33'400));   // late by 67 us
    CHECK(!pacer.due(5000 + 66'665));  // next beat is at +66'666, not +66'733
    CHECK(pacer.due(5000 + 66'666));
}

TG_TEST(a_long_stall_gives_one_frame_not_a_burst) {
    frame_pacer::Pacer pacer;
    CHECK(pacer.due(0));
    CHECK(pacer.due(500'000));
    CHECK(!pacer.due(500'001));
    CHECK_EQ(pacer.untilDue(500'000 + 10'000), 23'333u);
}

TG_TEST(works_across_the_clock_wrap) {
    frame_pacer::Pacer pacer;
    const std::uint32_t t0 = 0xFFFFFFFFu - 10'000;
    CHECK(pacer.due(t0));
    CHECK(!pacer.due(t0 + 20'000));
    CHECK(pacer.due(t0 + 33'333));  // wrapped past zero
}

int main() { return tg::test::run(); }
