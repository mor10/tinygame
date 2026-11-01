#pragma once

#include <cstdint>

namespace sim {
void turn(int side, std::int32_t delta);
void button(int side, bool down);
void tilt(float x, float y);
const std::uint16_t* framebuffer();
bool sleeping();
std::uint8_t brightness();
std::uint32_t led();
}  // namespace sim
