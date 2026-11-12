#include <cassert>
#include <cstring>

#include "draw_frame.h"
#include "presentation_text.h"

int main() {
    tg::engine::DrawFrame frame;
    tg::engine::PresentationText presented;
    assert(frame.text(0, 0, "SCORE 1", 7, 1));
    const char* label = presented.capture(0, frame.at(0));
    assert(std::strcmp(label, "SCORE 1") == 0);

    frame.clear();
    assert(frame.rectangle(0, 0, 1, 1, 1));
    assert(std::strcmp(label, "SCORE 1") == 0);
    frame.clear();
    assert(frame.text(0, 0, "SCORE 2", 7, 1));
    assert(std::strcmp(label, "SCORE 1") == 0);
    assert(presented.capture(0, frame.at(0)) == label);
    assert(std::strcmp(label, "SCORE 2") == 0);

    assert(frame.text(0, 0, "OTHER", 5, 1));
    const char* other = presented.capture(1, frame.at(1));
    assert(std::strcmp(other, "OTHER") == 0);
    assert(std::strcmp(label, "SCORE 2") == 0);
}
