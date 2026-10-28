#pragma once

#include "devtools.h"
#include "imu.h"
#include "input.h"
#include "tg_types.h"

namespace input_adapter {

struct Evidence {
    std::int32_t total = 0;
    std::uint32_t clicks = 0;
    input::ButtonEvent last_event = input::ButtonEvent::None;
};

class Adapter {
public:
    tg::Input read(const input::State& hw, const imu::Reading& motion,
                   const devtools::Overrides& dev, std::uint32_t now,
                   bool left_handed = false) {
        tg::Input in;
        in.left = encoder(hw.left, dev.left, left_, now);
        in.right = encoder(hw.right, dev.right, right_, now);
        record(left_evidence_, in.left, hw.left.button.events | dev.left.events);
        record(right_evidence_, in.right, hw.right.button.events | dev.right.events);
        // Main encoder: right by default, left when left handed. The action
        // button fields merge both switches.
        in.main = left_handed ? in.left : in.right;
        in.down = in.left.down || in.right.down;
        in.pressed = in.left.pressed || in.right.pressed;
        in.released = in.left.released || in.right.released;
        in.click = in.left.click || in.right.click;
        in.held_ms = in.left.held_ms > in.right.held_ms ? in.left.held_ms : in.right.held_ms;
        in.accel_g = motion.accel_g;
        in.gyro_dps = motion.gyro_dps;
        in.tilt_x = dev.tilt_active ? dev.tilt_x : motion.tilt_x;
        in.tilt_y = dev.tilt_active ? dev.tilt_y : motion.tilt_y;
        return in;
    }
    const Evidence& leftEvidence() const { return left_evidence_; }
    const Evidence& rightEvidence() const { return right_evidence_; }

private:
    static void record(Evidence& evidence, const tg::EncoderInput& in, std::uint8_t events) {
        evidence.total = input::boundedAdd(evidence.total, in.delta);
        if (in.click) input::increment(evidence.clicks);
        if (events & input::EVENT_LONG_PRESS) evidence.last_event = input::ButtonEvent::LongPress;
        else if (in.click) evidence.last_event = input::ButtonEvent::ShortPress;
        else if (in.pressed) evidence.last_event = input::ButtonEvent::Pressed;
        else if (in.released && evidence.last_event != input::ButtonEvent::LongPress)
            evidence.last_event = input::ButtonEvent::Released;
    }
    struct Held {
        bool down = false;
        std::uint32_t since = 0;
    };
    static tg::EncoderInput encoder(const input::Encoder& hw,
                                    const devtools::EncoderOverride& dev,
                                    Held& held, std::uint32_t now) {
        tg::EncoderInput out;
        const auto events = hw.button.events | dev.events;
        out.delta = input::boundedAdd(hw.delta, dev.delta);
        out.down = hw.button.down || dev.down;
        out.pressed = out.down && !held.down;
        out.released = !out.down && (held.down || (events & input::EVENT_RELEASED));
        out.click = (events & input::EVENT_SHORT_PRESS) != 0;
        if (out.down && !held.down) held.since = now;
        out.held_ms = (out.down || held.down) ? now - held.since : hw.button.held_ms;
        held.down = out.down;
        return out;
    }
    Held left_;
    Held right_;
    Evidence left_evidence_;
    Evidence right_evidence_;
};

}  // namespace input_adapter
