#pragma once

#include <cstddef>
#include <cstring>

#include "draw_frame.h"

namespace tg::engine {

class PresentationText {
public:
    const char* capture(std::size_t index, const DrawCommand& command) {
        std::memcpy(text_[index], command.text, sizeof(command.text));
        return text_[index];
    }

private:
    char text_[DrawFrame::MAX_COMMANDS][DrawFrame::MAX_TEXT_BYTES + 1] = {};
};

}  // namespace tg::engine
