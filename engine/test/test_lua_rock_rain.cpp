#include <cassert>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "lua_runtime.h"

namespace {

std::string readRules() {
    std::ifstream file("games/rock_rain/rules.lua");
    assert(file.good());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

float robotX(const tg::engine::DrawFrame& frame) {
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const auto& command = frame.at(i);
        if (command.kind == tg::engine::DrawKind::Rectangle &&
            command.palette_index == 11 && command.y > 60.0f) {
            return command.x;
        }
    }
    assert(false && "robot body was not drawn");
    return 0.0f;
}

bool hasText(const tg::engine::DrawFrame& frame, const char* text) {
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const auto& command = frame.at(i);
        if (command.kind == tg::engine::DrawKind::Text &&
            std::strcmp(command.text, text) == 0) {
            return true;
        }
    }
    return false;
}

int displayedScore(const tg::engine::DrawFrame& frame) {
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const auto& command = frame.at(i);
        if (command.kind == tg::engine::DrawKind::Text &&
            std::strncmp(command.text, "S ", 2) == 0) {
            return std::atoi(command.text + 2);
        }
    }
    assert(false && "score was not drawn");
    return 0;
}

bool displaysAmmo(const tg::engine::DrawFrame& frame, int ammo) {
    const std::string expected = " A " + std::to_string(ammo);
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const auto& command = frame.at(i);
        if (command.kind == tg::engine::DrawKind::Text &&
            std::strstr(command.text, expected.c_str()) != nullptr) {
            return true;
        }
    }
    return false;
}

void movementAndShootingUseKnobInput() {
    tg::engine::LuaRuntime game(17);
    const std::string rules = readRules();
    const tg::engine::WorldSize world{152.0f, 86.0f};
    tg::Input input;
    assert(game.load(rules.data(), rules.size(), "rock_rain"));
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    const float start_x = robotX(game.frame());

    input.main.delta = 8;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    const float moved_x = robotX(game.frame());
    assert(moved_x > start_x);
    input.main.delta = -1000;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(robotX(game.frame()) < 1.0f);
    input.main.delta = 1000;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(robotX(game.frame()) + 10.0f * (152.0f / 320.0f) <= world.width);

    input.main.delta = 0;
    input.pressed = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(hasText(game.frame(), "S 0 L 3 A 4"));
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(hasText(game.frame(), "S 0 L 3 A 4"));
    bool bolt_drawn = false;
    for (std::size_t i = 0; i < game.frame().size(); ++i) {
        const auto& command = game.frame().at(i);
        bolt_drawn = bolt_drawn ||
            (command.kind == tg::engine::DrawKind::Rectangle && command.palette_index == 12);
    }
    assert(bolt_drawn);
}

void ammoRunsOutAndCannotBeSpentTwiceDuringCooldown() {
    tg::engine::LuaRuntime game(31);
    const std::string rules = readRules();
    const tg::engine::WorldSize world{152.0f, 86.0f};
    tg::Input input;
    assert(game.load(rules.data(), rules.size(), "rock_rain"));
    assert(game.callUpdate(1.0f / 60.0f, input, world));

    for (int shot = 0; shot < 5; ++shot) {
        input.pressed = true;
        assert(game.callUpdate(1.0f / 60.0f, input, world));
        input.pressed = false;
        for (int tick = 0; tick < 10; ++tick) {
            assert(game.callUpdate(1.0f / 60.0f, input, world));
        }
    }
    assert(displaysAmmo(game.frame(), 0));

    input.pressed = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(displaysAmmo(game.frame(), 0));
}

void waveHasGapsAndCanBeOpenedWithAShot() {
    tg::engine::LuaRuntime game(23);
    const std::string rules = readRules();
    const tg::engine::WorldSize world{152.0f, 86.0f};
    tg::Input input;
    assert(game.load(rules.data(), rules.size(), "rock_rain"));

    for (int tick = 0; tick < 60; ++tick) {
        assert(game.callUpdate(1.0f / 60.0f, input, world));
    }

    float row_width = 0.0f;
    float target_left = -1.0f;
    for (std::size_t i = 0; i < game.frame().size(); ++i) {
        const auto& command = game.frame().at(i);
        if (command.kind != tg::engine::DrawKind::Rectangle ||
            command.palette_index != 9 || command.y >= 24.0f) {
            continue;
        }
        row_width += command.width;
        if (target_left < 0.0f) {
            target_left = command.x + command.width / 2.0f -
                          14.0f * (152.0f / 320.0f) / 2.0f;
        }
    }
    assert(row_width > world.width * 0.7f && row_width < world.width * 0.95f);
    assert(target_left >= 0.0f);

    const float robot_left = robotX(game.frame()) - 2.0f * (152.0f / 320.0f);
    input.main.delta = static_cast<std::int32_t>((target_left - robot_left) * 2.0f);
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    input.main.delta = 0;
    input.pressed = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    input.pressed = false;
    for (int tick = 0; tick < 45; ++tick) {
        assert(game.callUpdate(1.0f / 60.0f, input, world));
    }
    assert(displaysAmmo(game.frame(), 4));
    assert(displayedScore(game.frame()) >= 60);
}

void gameOverCanBeRestarted() {
    tg::engine::LuaRuntime game(17);
    const std::string rules = readRules();
    const tg::engine::WorldSize world{152.0f, 86.0f};
    tg::Input input;
    assert(game.load(rules.data(), rules.size(), "rock_rain"));

    bool game_over = false;
    for (int tick = 0; tick < 60 * 120 && !game_over; ++tick) {
        assert(game.callUpdate(1.0f / 60.0f, input, world));
        game_over = hasText(game.frame(), "GAME OVER");
    }
    assert(game_over);
    assert(hasText(game.frame(), "CLICK TO PLAY"));

    input.click = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(!hasText(game.frame(), "GAME OVER"));
    assert(hasText(game.frame(), "S 0 L 3 A 5"));
}

}  // namespace

int main() {
    movementAndShootingUseKnobInput();
    ammoRunsOutAndCannotBeSpentTwiceDuringCooldown();
    waveHasGapsAndCanBeOpenedWithAShot();
    gameOverCanBeRestarted();
    std::cout << "Lua Rock Rain tests passed\n";
}
