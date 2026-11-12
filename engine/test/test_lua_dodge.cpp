#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "bundle_script.h"

std::vector<std::uint8_t> readHex(const char* path) {
    std::ifstream stream(path);
    assert(stream.good());
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    std::vector<std::uint8_t> bytes;
    int high = -1;
    for (const unsigned char character : text) {
        if (character == ' ' || character == '\n' || character == '\r' || character == '\t') {
            continue;
        }
        const int value = character >= '0' && character <= '9' ? character - '0' :
                          character >= 'a' && character <= 'f' ? character - 'a' + 10 :
                          character >= 'A' && character <= 'F' ? character - 'A' + 10 : -1;
        assert(value >= 0);
        if (high < 0) {
            high = value;
        } else {
            bytes.push_back(static_cast<std::uint8_t>((high << 4) | value));
            high = -1;
        }
    }
    assert(high < 0);
    return bytes;
}

void knobDodgeMovesAndRecoversFromCollision() {
    const auto bytes = readHex("tests/fixtures/tgb_v1_knob_dodge.hex");
    tg::engine::BundleScript game(17);
    assert(game.load(bytes.data(), bytes.size()));

    tg::Input input;
    assert(game.update(1.0f / 60.0f, input));
    const float start_x = game.frame().at(1).x;
    input.main.delta = 4;
    assert(game.update(1.0f / 60.0f, input));
    const float right_x = game.frame().at(1).x;
    assert(right_x > start_x);
    input.main.delta = -4;
    assert(game.update(1.0f / 60.0f, input));
    assert(game.frame().at(1).x < right_x);

    input.main.delta = 0;
    for (int tick = 0; tick < 26; ++tick) {
        assert(game.update(0.1f, input));
    }
    assert(std::string(game.frame().at(5).text) == "CLICK TO RESET");

    input.click = true;
    assert(game.update(1.0f / 60.0f, input));
    assert(game.frame().size() == 4);
    assert(game.frame().at(1).x == start_x);
}

void knobDodgeUsesBoardSpecificWorldBounds() {
    const auto bytes = readHex("tests/fixtures/tgb_v1_knob_dodge.hex");
    tg::engine::BundleScript game(17);
    assert(game.load(bytes.data(), bytes.size()));
    const tg::engine::WorldSize board_b_view{120.0f, 108.0f};
    assert(game.update(1.0f / 60.0f, tg::Input{}, board_b_view));
    assert(game.frame().at(0).width == 120.0f);
    assert(game.frame().at(0).height == 108.0f);
    assert(game.frame().at(1).x == 56.0f);
    assert(game.frame().at(1).y == 98.0f);
}

int main() {
    knobDodgeMovesAndRecoversFromCollision();
    knobDodgeUsesBoardSpecificWorldBounds();
    std::puts("knob dodge tests passed");
}
