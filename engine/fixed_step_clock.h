#pragma once

#include <cstdint>

namespace tg::engine {

struct TickBatch {
    std::uint32_t steps_to_run = 0;
    std::uint64_t pending_steps = 0;
    bool overloaded = false;
};

class FixedStepClock {
public:
    static constexpr std::uint32_t HZ = 60;
    static constexpr std::uint32_t CANDIDATE_MAX_STEPS_PER_ADVANCE = 5;

    bool advance(std::uint64_t elapsed_us, TickBatch& batch);
    void reset();

private:
    static constexpr std::uint64_t UNITS_PER_SECOND = 1'000'000;
    std::uint64_t phase_ = 0;
    std::uint64_t pending_steps_ = 0;
};

}  // namespace tg::engine
