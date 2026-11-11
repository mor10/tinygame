#pragma once

#include <cstdint>

// Pure (host-testable) logic for the onboard RGB status LED: a base colour
// with an optional pattern, plus short event flashes on top.
namespace status_light {

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

inline constexpr Rgb fromHex(std::uint32_t hex) {
    return {static_cast<std::uint8_t>(hex >> 16), static_cast<std::uint8_t>(hex >> 8),
            static_cast<std::uint8_t>(hex)};
}

enum class Pattern : std::uint8_t { Solid, Breathe, Blink };

// Upper bound for any channel at 100% (the LED is very bright).
inline constexpr std::uint8_t MAX_CHANNEL = 64;
inline constexpr std::uint32_t BLINK_PERIOD_MS = 1000;
inline constexpr std::uint32_t BREATHE_PERIOD_MS = 2400;

class StatusLight {
public:
    void setBase(Rgb color, Pattern pattern = Pattern::Solid);
    // Shows `color` for `duration_ms`. Ignored if the current flash has more
    // time left, so a short flash can't cut off a more important long one.
    void flash(Rgb color, std::uint32_t duration_ms, std::uint32_t now_ms);
    // Output colour at `now_ms`, scaled to `percent` (0 = off) of MAX_CHANNEL.
    Rgb output(std::uint32_t now_ms, std::uint8_t percent) const;

private:
    Rgb base_;
    Pattern pattern_ = Pattern::Solid;
    Rgb flash_color_;
    std::uint32_t flash_until_ms_ = 0;
    bool flashing_ = false;
};

}  // namespace status_light
