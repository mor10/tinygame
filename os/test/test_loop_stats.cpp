// New in v2: tests for os/loop_stats.h.
#include "../../engine/test/tg_test.h"

#include <cstring>

#include "loop_stats.h"

TG_TEST(averages_maxima_and_rates) {
    loop_stats::Stats stats;
    stats.begin(1000);
    stats.ticks(300);
    for (int i = 0; i < 150; ++i) stats.frame(i == 7 ? 4000 : 2000, i == 9 ? 16000 : 15000);
    CHECK(!stats.due(5999));
    CHECK(stats.due(6000));
    const loop_stats::Report r = stats.take(6000);
    CHECK_EQ(r.ticks_per_10s, 600u);
    CHECK_EQ(r.frames_per_10s, 300u);
    CHECK_EQ(r.draw_avg_us, 2013u);
    CHECK_EQ(r.draw_max_us, 4000u);
    CHECK_EQ(r.present_avg_us, 15006u);
    CHECK_EQ(r.present_max_us, 16000u);
}

TG_TEST(take_starts_a_fresh_period_and_survives_a_clock_wrap) {
    loop_stats::Stats stats;
    stats.begin(0xFFFFFF00u);
    stats.ticks(10);
    CHECK(stats.due(0xFFFFFF00u + 5000));
    stats.take(0xFFFFFF00u + 5000);
    CHECK(!stats.due(0xFFFFFF00u + 5001));
    const loop_stats::Report empty = stats.take(0xFFFFFF00u + 6000);
    CHECK_EQ(empty.frames_per_10s, 0u);
    CHECK_EQ(empty.draw_avg_us, 0u);
}

TG_TEST(the_line_has_every_field) {
    loop_stats::Report r;
    r.ticks_per_10s = 600;
    r.frames_per_10s = 295;
    r.draw_avg_us = 2100;
    r.draw_max_us = 2600;
    r.present_avg_us = 14800;
    r.present_max_us = 15100;
    char line[200];
    loop_stats::format(line, sizeof(line), r, 212344);
    CHECK_EQ(std::string(line),
             std::string("[TIME] ticks/s=60.0 frames/s=29.5 draw_us avg=2100 max=2600 "
                         "present_us avg=14800 max=15100 heap=212344"));
}

int main() { return tg::test::run(); }
