#include "fixed_step_runner.h"

#include <limits>
#include <utility>

namespace tg::engine {
namespace {

void clearEncoder(tg::EncoderInput& input) {
    input.delta = 0;
    input.pressed = input.released = input.click = false;
}

StepError mergeEncoder(tg::EncoderInput& accumulated, const tg::EncoderInput& input) {
    const std::int64_t delta = static_cast<std::int64_t>(accumulated.delta) + input.delta;
    if (delta < std::numeric_limits<std::int32_t>::min() ||
        delta > std::numeric_limits<std::int32_t>::max()) {
        return StepError::DeltaOverflow;
    }
    if ((accumulated.pressed && input.pressed) || (accumulated.released && input.released) ||
        (accumulated.click && input.click)) {
        return StepError::DuplicateEdge;
    }
    const bool pressed = accumulated.pressed || input.pressed;
    const bool released = accumulated.released || input.released;
    const bool click = accumulated.click || input.click;
    accumulated = input;
    accumulated.delta = static_cast<std::int32_t>(delta);
    accumulated.pressed = pressed;
    accumulated.released = released;
    accumulated.click = click;
    return StepError::None;
}

}  // namespace

void FixedStepRunner::clearEdges(tg::Input& input) {
    clearEncoder(input.main);
    clearEncoder(input.left);
    clearEncoder(input.right);
    input.pressed = input.released = input.click = false;
}

StepError FixedStepRunner::merge(tg::Input& accumulated, const tg::Input& input) {
    tg::Input merged = input;
    merged.main = accumulated.main;
    merged.left = accumulated.left;
    merged.right = accumulated.right;
    for (auto pair : {std::pair{&merged.main, &input.main},
                      std::pair{&merged.left, &input.left},
                      std::pair{&merged.right, &input.right}}) {
        const StepError error = mergeEncoder(*pair.first, *pair.second);
        if (error != StepError::None) return error;
    }
    if ((accumulated.pressed && input.pressed) || (accumulated.released && input.released) ||
        (accumulated.click && input.click)) {
        return StepError::DuplicateEdge;
    }
    merged.pressed = accumulated.pressed || input.pressed;
    merged.released = accumulated.released || input.released;
    merged.click = accumulated.click || input.click;
    accumulated = merged;
    return StepError::None;
}

StepError FixedStepRunner::enqueue(std::uint64_t tick, const tg::Input& input) {
    if (size_ > 0) {
        InputBatch& tail = queue_[(head_ + size_ - 1) % queue_.size()];
        if (tail.tick == tick) return merge(tail.input, input);
    }
    if (size_ == queue_.size()) return StepError::InputQueueFull;
    queue_[(head_ + size_) % queue_.size()] = {tick, input};
    ++size_;
    return StepError::None;
}

void FixedStepRunner::discardInput() {
    head_ = 0;
    size_ = 0;
    current_ = {};
}

void FixedStepRunner::reset() {
    clock_.reset();
    completed_ticks_ = 0;
    error_ = StepError::None;
    discardInput();
}

const char* stepErrorMessage(StepError error) {
    switch (error) {
        case StepError::None: return "ok";
        case StepError::TimeOverflow: return "Game clock overflow";
        case StepError::BacklogLimit: return "Game could not keep up";
        case StepError::InputQueueFull: return "Game input queue is full";
        case StepError::DuplicateEdge: return "Too many button events in one tick";
        case StepError::DeltaOverflow: return "Knob input overflow";
        case StepError::CallbackFailed: return "Game update failed";
    }
    return "Unknown game timing error";
}

}  // namespace tg::engine
