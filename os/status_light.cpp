#include "status_light.h"

namespace status_light {

namespace {

Rgb scale(Rgb c, std::uint32_t numerator, std::uint32_t denominator) {
    return {static_cast<std::uint8_t>(c.r * numerator / denominator),
            static_cast<std::uint8_t>(c.g * numerator / denominator),
            static_cast<std::uint8_t>(c.b * numerator / denominator)};
}

// Triangle wave between 20% and 100%, quantized to ~30 Hz steps.
Rgb breathe(Rgb c, std::uint32_t now_ms) {
    const std::uint32_t t = (now_ms / 33U * 33U) % BREATHE_PERIOD_MS;
    const std::uint32_t half = BREATHE_PERIOD_MS / 2U;
    const std::uint32_t ramp = t < half ? t : BREATHE_PERIOD_MS - t;
    return scale(c, 20U + 80U * ramp / half, 100U);
}

}  // namespace

void StatusLight::setBase(Rgb color, Pattern pattern) {
    base_ = color;
    pattern_ = pattern;
}

void StatusLight::flash(Rgb color, std::uint32_t duration_ms, std::uint32_t now_ms) {
    const std::uint32_t until = now_ms + duration_ms;
    if (flashing_ && static_cast<std::int32_t>(flash_until_ms_ - until) > 0 &&
        static_cast<std::int32_t>(flash_until_ms_ - now_ms) > 0) {
        return;
    }
    flash_color_ = color;
    flash_until_ms_ = until;
    flashing_ = true;
}

Rgb StatusLight::output(std::uint32_t now_ms, std::uint8_t percent) const {
    Rgb full = base_;
    if (flashing_ && static_cast<std::int32_t>(flash_until_ms_ - now_ms) > 0) {
        full = flash_color_;
    } else if (pattern_ == Pattern::Breathe) {
        full = breathe(base_, now_ms);
    } else if (pattern_ == Pattern::Blink && (now_ms % BLINK_PERIOD_MS) >= BLINK_PERIOD_MS / 2U) {
        full = Rgb{};
    }
    const std::uint32_t p = percent > 100 ? 100U : percent;
    return scale(full, MAX_CHANNEL * p, 255U * 100U);
}

}  // namespace status_light
