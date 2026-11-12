#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "fixed_step_clock.h"
#include "tg_types.h"

namespace tg::engine {

enum class StepError {
    None,
    TimeOverflow,
    BacklogLimit,
    InputQueueFull,
    DuplicateEdge,
    DeltaOverflow,
    CallbackFailed,
};

struct StepResult {
    StepError error = StepError::None;
    std::uint32_t steps = 0;
    std::uint64_t pending_steps = 0;
};

class FixedStepRunner {
public:
    static constexpr std::size_t MAX_INPUT_BATCHES = 32;
    static constexpr std::uint64_t MAX_PENDING_STEPS = 120;
    static constexpr float STEP_SECONDS = 1.0f / FixedStepClock::HZ;

    template <typename Update>
    StepResult advance(std::uint64_t elapsed_us, const tg::Input& input, Update&& update) {
        if (error_ != StepError::None) return {error_};
        TickBatch batch;
        if (!clock_.advance(elapsed_us, batch)) return {error_ = StepError::TimeOverflow};
        if (batch.pending_steps > MAX_PENDING_STEPS) return {error_ = StepError::BacklogLimit};
        const std::uint64_t due = batch.pending_steps + batch.steps_to_run;
        if (due >= std::numeric_limits<std::uint64_t>::max() - completed_ticks_) {
            return {error_ = StepError::TimeOverflow};
        }
        const StepError error = enqueue(completed_ticks_ + due + 1, input);
        if (error != StepError::None) return {error_ = error};

        StepResult result{StepError::None, 0, batch.pending_steps};
        for (std::uint32_t i = 0; i < batch.steps_to_run; ++i) {
            ++completed_ticks_;
            clearEdges(current_);
            if (size_ > 0 && queue_[head_].tick == completed_ticks_) {
                current_ = queue_[head_].input;
                head_ = (head_ + 1) % queue_.size();
                --size_;
            }
            if (!update(current_)) {
                result.error = error_ = StepError::CallbackFailed;
                return result;
            }
            ++result.steps;
        }
        return result;
    }

    void discardInput();
    void reset();

private:
    struct InputBatch {
        std::uint64_t tick = 0;
        tg::Input input;
    };

    static void clearEdges(tg::Input& input);
    static StepError merge(tg::Input& accumulated, const tg::Input& input);
    StepError enqueue(std::uint64_t tick, const tg::Input& input);

    FixedStepClock clock_;
    std::array<InputBatch, MAX_INPUT_BATCHES> queue_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::uint64_t completed_ticks_ = 0;
    tg::Input current_{};
    StepError error_ = StepError::None;
};

const char* stepErrorMessage(StepError error);

}  // namespace tg::engine
