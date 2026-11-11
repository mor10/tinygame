#pragma once

// New in v2. Counts what the main loop did so it can print one line of
// timings every few seconds. Pure arithmetic: no clock or serial port in here.

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace loop_stats {

inline constexpr std::uint32_t REPORT_INTERVAL_MS = 5000;

struct Report {
    std::uint32_t ticks_per_10s = 0;   // tenths: 600 means 60.0 per second
    std::uint32_t frames_per_10s = 0;
    std::uint32_t draw_avg_us = 0;
    std::uint32_t draw_max_us = 0;
    std::uint32_t present_avg_us = 0;
    std::uint32_t present_max_us = 0;
};

class Stats {
public:
    void begin(std::uint32_t now_ms) {
        *this = Stats{};
        started_ms_ = now_ms;
    }
    void ticks(int count) { ticks_ += static_cast<std::uint32_t>(count); }
    void frame(std::uint32_t draw_us, std::uint32_t present_us) {
        ++frames_;
        draw_sum_ += draw_us;
        present_sum_ += present_us;
        if (draw_us > draw_max_) draw_max_ = draw_us;
        if (present_us > present_max_) present_max_ = present_us;
    }
    bool due(std::uint32_t now_ms) const { return now_ms - started_ms_ >= REPORT_INTERVAL_MS; }

    // Summarises the period since begin() or the last take(), then starts a new one.
    Report take(std::uint32_t now_ms) {
        const std::uint64_t elapsed_ms = now_ms - started_ms_;
        Report r;
        if (elapsed_ms != 0) {
            r.ticks_per_10s = static_cast<std::uint32_t>(ticks_ * 10000ULL / elapsed_ms);
            r.frames_per_10s = static_cast<std::uint32_t>(frames_ * 10000ULL / elapsed_ms);
        }
        if (frames_ != 0) {
            r.draw_avg_us = static_cast<std::uint32_t>(draw_sum_ / frames_);
            r.present_avg_us = static_cast<std::uint32_t>(present_sum_ / frames_);
        }
        r.draw_max_us = draw_max_;
        r.present_max_us = present_max_;
        begin(now_ms);
        return r;
    }

private:
    std::uint32_t started_ms_ = 0;
    std::uint64_t ticks_ = 0;
    std::uint64_t frames_ = 0;
    std::uint64_t draw_sum_ = 0;
    std::uint64_t present_sum_ = 0;
    std::uint32_t draw_max_ = 0;
    std::uint32_t present_max_ = 0;
};

// "[TIME] ticks/s=60.0 frames/s=30.0 draw_us avg=2100 max=2600 present_us avg=14800 max=15100 heap=212344"
inline int format(char* out, std::size_t size, const Report& r, std::uint32_t free_heap) {
    return std::snprintf(out, size,
                         "[TIME] ticks/s=%lu.%lu frames/s=%lu.%lu draw_us avg=%lu max=%lu "
                         "present_us avg=%lu max=%lu heap=%lu",
                         static_cast<unsigned long>(r.ticks_per_10s / 10),
                         static_cast<unsigned long>(r.ticks_per_10s % 10),
                         static_cast<unsigned long>(r.frames_per_10s / 10),
                         static_cast<unsigned long>(r.frames_per_10s % 10),
                         static_cast<unsigned long>(r.draw_avg_us),
                         static_cast<unsigned long>(r.draw_max_us),
                         static_cast<unsigned long>(r.present_avg_us),
                         static_cast<unsigned long>(r.present_max_us),
                         static_cast<unsigned long>(free_heap));
}

}  // namespace loop_stats
