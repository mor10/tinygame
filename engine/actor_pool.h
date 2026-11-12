#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace tg::engine {

struct ActorHandle {
    std::uint32_t slot = std::numeric_limits<std::uint32_t>::max();
    std::uint64_t generation = 0;

    bool valid() const {
        return slot != std::numeric_limits<std::uint32_t>::max() && generation != 0;
    }

    bool operator==(const ActorHandle& other) const {
        return slot == other.slot && generation == other.generation;
    }
};

enum class PoolError : std::uint8_t {
    None,
    Full,
    InvalidHandle,
};

template <typename T>
struct PoolCreateResult {
    PoolError error = PoolError::Full;
    ActorHandle handle;

    explicit operator bool() const { return error == PoolError::None; }
};

template <typename T>
struct PoolAccessResult {
    PoolError error = PoolError::InvalidHandle;
    T* value = nullptr;

    explicit operator bool() const { return error == PoolError::None; }
};

template <typename T>
struct ConstPoolAccessResult {
    PoolError error = PoolError::InvalidHandle;
    const T* value = nullptr;

    explicit operator bool() const { return error == PoolError::None; }
};

template <typename T, std::size_t Capacity>
class GenerationalPool {
    static_assert(Capacity > 0);
    static_assert(Capacity < std::numeric_limits<std::uint32_t>::max());

public:
    static constexpr std::size_t capacity() { return Capacity; }
    std::size_t size() const { return size_; }

    PoolCreateResult<T> create(const T& value) {
        for (std::size_t index = 0; index < Capacity; ++index) {
            Slot& slot = slots_[index];
            if (slot.value.has_value()) continue;
            slot.value.emplace(value);
            ++size_;
            return {PoolError::None,
                    {static_cast<std::uint32_t>(index), slot.generation}};
        }
        return {PoolError::Full, {}};
    }

    PoolError erase(ActorHandle handle) {
        Slot* slot = find(handle);
        if (slot == nullptr) return PoolError::InvalidHandle;
        slot->value.reset();
        ++slot->generation;
        if (slot->generation == 0) ++slot->generation;
        --size_;
        return PoolError::None;
    }

    PoolAccessResult<T> get(ActorHandle handle) {
        Slot* slot = find(handle);
        return slot == nullptr
            ? PoolAccessResult<T>{PoolError::InvalidHandle, nullptr}
            : PoolAccessResult<T>{PoolError::None, &*slot->value};
    }

    ConstPoolAccessResult<T> get(ActorHandle handle) const {
        const Slot* slot = find(handle);
        return slot == nullptr
            ? ConstPoolAccessResult<T>{PoolError::InvalidHandle, nullptr}
            : ConstPoolAccessResult<T>{PoolError::None, &*slot->value};
    }

    ActorHandle handleAt(std::size_t index) const {
        if (index >= Capacity || !slots_[index].value.has_value()) return {};
        return {static_cast<std::uint32_t>(index), slots_[index].generation};
    }

    template <typename Visitor>
    void forEach(Visitor&& visitor) {
        // Visits occupied slots in order; a new entry in a later slot is visited this pass.
        for (std::size_t index = 0; index < Capacity; ++index) {
            const ActorHandle handle = handleAt(index);
            if (handle.valid()) visitor(handle);
        }
    }

    template <typename Visitor>
    void forEach(Visitor&& visitor) const {
        for (std::size_t index = 0; index < Capacity; ++index) {
            const ActorHandle handle = handleAt(index);
            if (handle.valid()) visitor(handle);
        }
    }

private:
    struct Slot {
        std::optional<T> value;
        std::uint64_t generation = 1;
    };

    Slot* find(ActorHandle handle) {
        if (!handle.valid() || handle.slot >= Capacity) return nullptr;
        Slot& slot = slots_[handle.slot];
        return slot.generation == handle.generation && slot.value.has_value() ? &slot : nullptr;
    }

    const Slot* find(ActorHandle handle) const {
        if (!handle.valid() || handle.slot >= Capacity) return nullptr;
        const Slot& slot = slots_[handle.slot];
        return slot.generation == handle.generation && slot.value.has_value() ? &slot : nullptr;
    }

    Slot slots_[Capacity];
    std::size_t size_ = 0;
};

}  // namespace tg::engine
