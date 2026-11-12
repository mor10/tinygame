#include "fixed_step_clock.h"

#include <algorithm>
#include <limits>

namespace tg::engine {

bool FixedStepClock::advance(std::uint64_t elapsed_us, TickBatch& batch) {
    if (elapsed_us > (std::numeric_limits<std::uint64_t>::max() - phase_) / HZ) {
        return false;
    }

    const std::uint64_t units = phase_ + elapsed_us * HZ;
    const std::uint64_t accrued = units / UNITS_PER_SECOND;
    if (accrued > std::numeric_limits<std::uint64_t>::max() - pending_steps_) {
        return false;
    }

    phase_ = units % UNITS_PER_SECOND;
    pending_steps_ += accrued;
    const auto steps = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(pending_steps_, CANDIDATE_MAX_STEPS_PER_ADVANCE));
    pending_steps_ -= steps;

    batch.steps_to_run = steps;
    batch.pending_steps = pending_steps_;
    batch.overloaded = pending_steps_ != 0;
    return true;
}

void FixedStepClock::reset() {
    phase_ = 0;
    pending_steps_ = 0;
}

}  // namespace tg::engine
