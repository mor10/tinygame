#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "lua_runtime.h"

using tg::engine::DrawCommand;
using tg::engine::DrawKind;
using tg::engine::LuaRuntime;
using tg::engine::WorldSize;

std::string readRules() {
    std::ifstream stream("games/brickbreaker/rules.lua");
    assert(stream.good());
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

std::string readRulesWithTestHooks() {
    std::string source = readRules();
    source += R"(
function test_set_level(level_number)
    level = level_number
    load_level()
    reset_ball()
end

function test_prepare_two_hit_brick()
    for index = 1, BRICK_ROWS * BRICK_COLUMNS do bricks[index] = 0 end
    bricks[1] = 2
end

function test_hit_prepared_brick()
    local x, y, width = brick_rect(1)
    local hit_x = x + width / 2
    local hit_y = y + BRICK_HEIGHT / 2
    hit_brick(1, hit_x, hit_y, hit_x, hit_y)
    update_bricks()
end
)";
    return source;
}

float paddleX(const tg::engine::DrawFrame& frame, const WorldSize& world) {
    const float paddle_y = world.height - 13.0f;
    for (std::size_t index = 0; index < frame.size(); ++index) {
        const DrawCommand& command = frame.at(index);
        if (command.kind == DrawKind::Rectangle && command.y == paddle_y &&
            command.height == 3.0f) {
            return command.x;
        }
    }
    assert(false && "paddle was not drawn");
    return 0.0f;
}

bool hasText(const tg::engine::DrawFrame& frame, const char* text) {
    for (std::size_t index = 0; index < frame.size(); ++index) {
        const DrawCommand& command = frame.at(index);
        if (command.kind == DrawKind::Text && std::string(command.text) == text) return true;
    }
    return false;
}

float ballCenterX(const tg::engine::DrawFrame& frame) {
    for (std::size_t index = 0; index < frame.size(); ++index) {
        const DrawCommand& command = frame.at(index);
        if (command.kind == DrawKind::Rectangle && command.width == 3.0f &&
            command.height == 3.0f && command.palette_index == 7) {
            return command.x + command.width / 2.0f;
        }
    }
    assert(false && "ball was not drawn");
    return 0.0f;
}

float ballCenterY(const tg::engine::DrawFrame& frame) {
    for (std::size_t index = 0; index < frame.size(); ++index) {
        const DrawCommand& command = frame.at(index);
        if (command.kind == DrawKind::Rectangle && command.width == 3.0f &&
            command.height == 3.0f && command.palette_index == 7) {
            return command.y + command.height / 2.0f;
        }
    }
    assert(false && "ball was not drawn");
    return 0.0f;
}

int brickCount(const tg::engine::DrawFrame& frame) {
    int count = 0;
    for (std::size_t index = 0; index < frame.size(); ++index) {
        const DrawCommand& command = frame.at(index);
        if (command.kind == DrawKind::Rectangle && command.y >= 12.0f &&
            command.y < 12.0f + 5.0f * 5.0f && command.height == 4.0f) {
            ++count;
        }
    }
    return count;
}

bool hasLives(const tg::engine::DrawFrame& frame, int lives) {
    const std::string expected = "L1  " + std::to_string(lives) + "L";
    return hasText(frame, expected.c_str());
}

void missUntilLives(LuaRuntime& game, const WorldSize& world, int lives) {
    tg::Input input;
    input.click = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    input.click = false;
    for (int tick = 0; tick < 1800 && !hasLives(game.frame(), lives); ++tick) {
        input.main.delta = ballCenterX(game.frame()) < world.width / 2.0f ? 1000 : -1000;
        assert(game.callUpdate(1.0f / 60.0f, input, world));
    }
    assert(hasLives(game.frame(), lives));
}

void paddleFollowsBothKnobDirectionsAndClamps() {
    const std::string source = readRules();
    LuaRuntime game(17);
    const WorldSize world{152.0f, 86.0f};
    assert(game.load(source.data(), source.size()));
    if (!game.callUpdate(1.0f / 60.0f, tg::Input{}, world)) {
        std::fprintf(stderr, "Brick Breaker runtime error: %s\n", game.error());
        assert(false);
    }
    const float center = paddleX(game.frame(), world);

    tg::Input input;
    input.main.delta = 1;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    const float right = paddleX(game.frame(), world);
    assert(std::fabs(right - center - 1.1875f) < 0.001f);

    input.main.delta = -1;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(paddleX(game.frame(), world) < right);

    input.main.delta = -1000;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(paddleX(game.frame(), world) == 0.0f);

    input.main.delta = 1000;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(paddleX(game.frame(), world) == world.width - 22.0f);
}

void fastTurnsMoveTheLegacyScaledDistance() {
    const std::string source = readRules();
    LuaRuntime game(17);
    const WorldSize world{152.0f, 86.0f};
    assert(game.load(source.data(), source.size()));
    assert(game.callUpdate(1.0f / 60.0f, tg::Input{}, world));
    const float center = paddleX(game.frame(), world);

    tg::Input input;
    input.main.delta = 12;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(std::fabs(paddleX(game.frame(), world) - center - 45.6f) < 0.01f);
}

void clickLaunchesTheBallAndBreaksBricks() {
    const std::string source = readRules();
    LuaRuntime game(17);
    const WorldSize world{152.0f, 86.0f};
    tg::Input input;
    assert(game.load(source.data(), source.size()));
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(hasText(game.frame(), "CLICK TO LAUNCH"));

    input.click = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    input.click = false;
    bool scored = false;
    for (int tick = 0; tick < 180 && !scored; ++tick) {
        assert(game.callUpdate(1.0f / 60.0f, input, world));
        scored = hasText(game.frame(), "SCORE 100");
    }
    assert(scored);
}

void layoutAdaptsToTheSmallerBoardViewport() {
    const std::string source = readRules();
    LuaRuntime game(17);
    const WorldSize world{120.0f, 108.0f};
    assert(game.load(source.data(), source.size()));
    assert(game.callUpdate(1.0f / 60.0f, tg::Input{}, world));
    assert(game.frame().at(0).width == world.width);
    assert(game.frame().at(0).height == world.height);
    assert(game.frame().size() <= 60);
    assert(paddleX(game.frame(), world) >= 0.0f);
}

void missedBallsConsumeLivesAndRestartAfterGameOver() {
    const std::string source = readRules();
    LuaRuntime game(17);
    const WorldSize world{152.0f, 86.0f};
    assert(game.load(source.data(), source.size()));
    assert(game.callUpdate(1.0f / 60.0f, tg::Input{}, world));
    assert(hasLives(game.frame(), 3));

    missUntilLives(game, world, 2);
    assert(hasText(game.frame(), "CLICK TO LAUNCH"));
    const float attached_x = ballCenterX(game.frame());
    tg::Input input;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(std::fabs(ballCenterX(game.frame()) - attached_x) < 0.001f);

    input.click = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(ballCenterX(game.frame()) != attached_x);
    missUntilLives(game, world, 1);
    missUntilLives(game, world, 0);
    assert(hasText(game.frame(), "GAME OVER"));

    input.click = true;
    assert(game.callUpdate(1.0f / 60.0f, input, world));
    assert(hasLives(game.frame(), 3));
    assert(hasText(game.frame(), "SCORE 0"));
    assert(hasText(game.frame(), "CLICK TO LAUNCH"));
}

void twoHitBricksScoreOnBothHits() {
    const std::string source = readRulesWithTestHooks();
    LuaRuntime game(17);
    const WorldSize world{152.0f, 86.0f};
    assert(game.load(source.data(), source.size()));
    assert(game.callUpdate(0.0f, tg::Input{}, world));
    assert(game.call("test_prepare_two_hit_brick"));

    assert(game.call("test_hit_prepared_brick"));
    assert(game.callUpdate(0.0f, tg::Input{}, world));
    assert(hasText(game.frame(), "SCORE 100"));
    assert(brickCount(game.frame()) == 1);

    assert(game.call("test_hit_prepared_brick"));
    assert(game.callUpdate(0.0f, tg::Input{}, world));
    assert(hasText(game.frame(), "SCORE 200"));
    assert(hasText(game.frame(), "L2  3L"));
}

void launchSpeedStopsAtTheScaledLegacyMaximum() {
    const std::string source = readRulesWithTestHooks();
    LuaRuntime game(17);
    const WorldSize world{152.0f, 86.0f};
    assert(game.load(source.data(), source.size()));
    assert(game.callUpdate(0.0f, tg::Input{}, world));
    assert(game.call("test_set_level", 100.0f));

    tg::Input input;
    input.click = true;
    assert(game.callUpdate(0.0f, input, world));
    input.click = false;
    const float start_x = ballCenterX(game.frame());
    const float start_y = ballCenterY(game.frame());
    assert(game.callUpdate(1.0f / 120.0f, input, world));

    const float dx = ballCenterX(game.frame()) - start_x;
    const float dy = ballCenterY(game.frame()) - start_y;
    const float speed = std::sqrt(dx * dx + dy * dy) * 120.0f;
    const float maximum = 220.0f * world.width / 320.0f;
    assert(std::fabs(speed - maximum) < 0.1f);
}

int main() {
    paddleFollowsBothKnobDirectionsAndClamps();
    fastTurnsMoveTheLegacyScaledDistance();
    clickLaunchesTheBallAndBreaksBricks();
    layoutAdaptsToTheSmallerBoardViewport();
    missedBallsConsumeLivesAndRestartAfterGameOver();
    twoHitBricksScoreOnBothHits();
    launchSpeedStopsAtTheScaledLegacyMaximum();
    std::puts("brick breaker Lua tests passed");
}
