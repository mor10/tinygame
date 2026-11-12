#pragma once

#include <cstddef>
#include <cstdint>

namespace tg::engine {

struct WorldSize {
    float width = 152.0f;
    float height = 86.0f;
};

enum class DrawKind : std::uint8_t {
    Rectangle,
    Text,
};

struct DrawCommand {
    DrawKind kind = DrawKind::Rectangle;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    std::uint8_t palette_index = 0;
    char text[33] = {};
};

class DrawFrame {
public:
    static constexpr std::size_t MAX_COMMANDS = 128;
    static constexpr std::size_t MAX_TEXT_BYTES = 32;

    void clear();
    bool rectangle(float x, float y, float width, float height, std::uint8_t palette_index);
    bool text(float x, float y, const char* value, std::size_t size,
              std::uint8_t palette_index);

    std::size_t size() const { return size_; }
    const DrawCommand& at(std::size_t index) const;

private:
    bool canAdd(float x, float y, std::uint8_t palette_index) const;

    DrawCommand commands_[MAX_COMMANDS] = {};
    std::size_t size_ = 0;
};

}  // namespace tg::engine
