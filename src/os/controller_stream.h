#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace controller_stream {

inline constexpr std::uint32_t INTERVAL_MS = 20;
inline constexpr std::uint32_t HOST_TIMEOUT_MS = 3000;

struct Sample {
    std::int32_t left_total = 0;
    bool left_down = false;
    std::int32_t right_total = 0;
    bool right_down = false;
    float tilt_x = 0;
    float tilt_y = 0;
};

// Physical totals are sent unchanged; handedness is a host-side choice.
inline bool format(char* buffer, std::size_t capacity, const Sample& sample) {
    if (capacity == 0 || !std::isfinite(sample.tilt_x) || !std::isfinite(sample.tilt_y))
        return false;
    const int size = std::snprintf(buffer, capacity, "IN %ld %d %ld %d %.6f %.6f\n",
                                  static_cast<long>(sample.left_total), sample.left_down,
                                  static_cast<long>(sample.right_total), sample.right_down,
                                  static_cast<double>(sample.tilt_x),
                                  static_cast<double>(sample.tilt_y));
    return size >= 0 && static_cast<std::size_t>(size) < capacity;
}

class State {
public:
    void start(std::uint32_t now) {
        active_ = true;
        sent_ = false;
        heartbeat_ = now;
    }
    void stop() { active_ = false; }
    bool active() const { return active_; }
    bool ping(std::uint32_t now) {
        if (!active_) return false;
        heartbeat_ = now;
        return true;
    }
    bool check(std::uint32_t now, bool connected) {
        if (!connected || now - heartbeat_ >= HOST_TIMEOUT_MS) stop();
        return active_;
    }
    bool due(std::uint32_t now, const Sample& sample) const {
        return active_ && (!sent_ || now - last_sent_ >= INTERVAL_MS ||
                           sample.left_down != previous_.left_down ||
                           sample.right_down != previous_.right_down);
    }
    void sent(std::uint32_t now, const Sample& sample) {
        sent_ = true;
        last_sent_ = now;
        previous_ = sample;
    }

private:
    bool active_ = false;
    bool sent_ = false;
    std::uint32_t heartbeat_ = 0;
    std::uint32_t last_sent_ = 0;
    Sample previous_;
};

}  // namespace controller_stream
