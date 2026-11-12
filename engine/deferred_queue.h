#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace tg::engine {

enum class DeferredQueueError {
    None,
    Full,
};

template <typename T, std::size_t Capacity>
class DeferredQueue {
    static_assert(Capacity > 0);

public:
    static constexpr std::size_t capacity() { return Capacity; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    DeferredQueueError enqueue(const T& value) {
        return enqueueValue(value);
    }

    DeferredQueueError enqueue(T&& value) {
        return enqueueValue(std::move(value));
    }

    template <typename Visitor>
    std::size_t drainBatch(Visitor&& visitor) {
        const std::size_t batch_size = size_;
        for (std::size_t i = 0; i < batch_size; ++i) {
            T value = std::move(*items_[head_]);
            items_[head_].reset();
            head_ = (head_ + 1) % Capacity;
            --size_;
            visitor(std::move(value));
        }
        return batch_size;
    }

private:
    template <typename Value>
    DeferredQueueError enqueueValue(Value&& value) {
        if (size_ == Capacity) return DeferredQueueError::Full;
        items_[tail_].emplace(std::forward<Value>(value));
        tail_ = (tail_ + 1) % Capacity;
        ++size_;
        return DeferredQueueError::None;
    }

    std::array<std::optional<T>, Capacity> items_{};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;
};

}  // namespace tg::engine
