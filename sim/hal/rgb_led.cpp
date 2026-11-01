#include "rgb_led.h"

#include "sim_hal.h"

namespace {
std::uint32_t rgb = 0;
}
namespace rgb_led {
void initialize() { rgb = 0; }
void set(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    rgb = (static_cast<std::uint32_t>(r) << 16) |
          (static_cast<std::uint32_t>(g) << 8) | b;
}
}  // namespace rgb_led
namespace sim {
std::uint32_t led() { return rgb; }
}
