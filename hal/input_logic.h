#pragma once

#include <cstdint>

namespace input {

inline constexpr std::uint32_t DEBOUNCE_MS = 25;
inline constexpr std::uint32_t LONG_PRESS_MS = 650;
inline constexpr std::uint8_t EVENT_PRESSED = 1U << 0;
inline constexpr std::uint8_t EVENT_RELEASED = 1U << 1;
inline constexpr std::uint8_t EVENT_SHORT_PRESS = 1U << 2;
inline constexpr std::uint8_t EVENT_LONG_PRESS = 1U << 3;

enum class ButtonEvent : std::uint8_t { None, Pressed, Released, ShortPress, LongPress };

inline std::int32_t boundedAdd(std::int32_t a, std::int32_t b) {
    const std::int64_t sum = static_cast<std::int64_t>(a) + b;
    return sum > INT32_MAX ? INT32_MAX : (sum < INT32_MIN ? INT32_MIN :
                                        static_cast<std::int32_t>(sum));
}

inline void increment(std::uint32_t& count) {
    if (count != UINT32_MAX) ++count;
}

struct Quadrature {
    std::uint8_t ab = 0;
    std::uint32_t invalid = 0;

    void initialize(std::uint8_t initial) {
        ab = initial;
        invalid = 0;
    }

    __attribute__((always_inline)) inline int sample(std::uint8_t next) {
        const auto changed = ab ^ next;
        ab = next;
        if (changed == 0) return 0;
        if (changed == 3) {
            if (invalid != UINT32_MAX) ++invalid;
            return 0;
        }
        const bool a = (next & 2) != 0;
        const bool b = (next & 1) != 0;
        return changed == 1 ? (a != b ? 1 : -1) : (a == b ? 1 : -1);
    }
};

struct Button {
    bool down = false;
    std::uint32_t held_ms = 0;
    std::uint8_t events = 0;
    ButtonEvent last_event = ButtonEvent::None;
    std::uint32_t clicks = 0;

    void initialize(bool pressed, std::uint32_t now) {
        *this = Button{};
        down = candidate_ = pressed;
        changed_at_ = pressed_at_ = now;
        armed_ = !pressed;
    }

    void update(bool pressed, std::uint32_t now) {
        events = 0;
        if (pressed != candidate_) {
            candidate_ = pressed;
            changed_at_ = now;
        }
        if (now - changed_at_ >= DEBOUNCE_MS && down != candidate_) {
            down = candidate_;
            if (!armed_) {
                if (!down) {
                    armed_ = true;
                    held_ms = 0;
                }
                return;
            }
            if (down) {
                pressed_at_ = changed_at_;
                held_ms = 0;
                long_sent_ = false;
                events = EVENT_PRESSED;
                last_event = ButtonEvent::Pressed;
            } else {
                held_ms = changed_at_ - pressed_at_;
                events = EVENT_RELEASED;
                if (!long_sent_ && held_ms < LONG_PRESS_MS) {
                    events |= EVENT_SHORT_PRESS;
                    last_event = ButtonEvent::ShortPress;
                    increment(clicks);
                } else if (!long_sent_) {
                    long_sent_ = true;
                    events |= EVENT_LONG_PRESS;
                    last_event = ButtonEvent::LongPress;
                }
            }
        }
        if (armed_ && down && candidate_) {
            held_ms = now - pressed_at_;
            if (!long_sent_ && held_ms >= LONG_PRESS_MS) {
                long_sent_ = true;
                events |= EVENT_LONG_PRESS;
                last_event = ButtonEvent::LongPress;
            }
        }
    }

private:
    bool candidate_ = false;
    bool armed_ = true;
    bool long_sent_ = false;
    std::uint32_t changed_at_ = 0;
    std::uint32_t pressed_at_ = 0;
};

}  // namespace input
