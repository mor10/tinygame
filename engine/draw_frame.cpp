#include "draw_frame.h"

#include <cmath>
#include <cstring>

namespace tg::engine {
namespace {

constexpr float MAX_WORLD_COORDINATE = 1'000'000.0f;

bool validCoordinate(float value) {
    return std::isfinite(value) && value >= -MAX_WORLD_COORDINATE &&
           value <= MAX_WORLD_COORDINATE;
}

}  // namespace

void DrawFrame::clear() {
    size_ = 0;
}

bool DrawFrame::canAdd(float x, float y, std::uint8_t palette_index) const {
    return size_ < MAX_COMMANDS && validCoordinate(x) && validCoordinate(y) &&
           palette_index < 16;
}

bool DrawFrame::rectangle(float x, float y, float width, float height,
                          std::uint8_t palette_index) {
    if (!canAdd(x, y, palette_index) || !validCoordinate(width) || !validCoordinate(height) ||
        width <= 0.0f || height <= 0.0f || !validCoordinate(x + width) ||
        !validCoordinate(y + height)) {
        return false;
    }
    commands_[size_++] = {DrawKind::Rectangle, x, y, width, height, palette_index, {}};
    return true;
}

bool DrawFrame::text(float x, float y, const char* value, std::size_t size,
                     std::uint8_t palette_index) {
    if (!canAdd(x, y, palette_index) || value == nullptr || size == 0 ||
        size > MAX_TEXT_BYTES) {
        return false;
    }
    for (std::size_t i = 0; i < size; ++i) {
        const unsigned char character = static_cast<unsigned char>(value[i]);
        if (character < 32 || character > 126) return false;
    }
    DrawCommand& command = commands_[size_++];
    command = {DrawKind::Text, x, y, 0.0f, 0.0f, palette_index, {}};
    std::memcpy(command.text, value, size);
    command.text[size] = '\0';
    return true;
}

const DrawCommand& DrawFrame::at(std::size_t index) const {
    static const DrawCommand empty = {};
    return index < size_ ? commands_[index] : empty;
}

}  // namespace tg::engine
