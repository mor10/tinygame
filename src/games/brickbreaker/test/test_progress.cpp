#include <cassert>
#include <initializer_list>

#include "game.h"
#include "level.h"

int main() {
    using namespace brickbreaker;
    for (const std::uint32_t previous : {255U, UINT32_MAX}) {
        game::Game game;
        game.newGame(0.5f);
        auto& state = game.mutableState();
        state.level = previous;
        state.launched = true;
        state.bricks_remaining = 0;
        for (auto& row : state.bricks) {
            for (auto& hits : row) hits = 0;
        }
        assert(game.step(0.5f) & game::EVENT_LEVEL_CLEARED);
        assert(state.level == (previous == UINT32_MAX ? previous : previous + 1));
        assert(state.ball_speed == config::BALL_MAX_SPEED);
        for (std::uint8_t row = 0; row < config::BRICK_ROWS; ++row) {
            for (std::uint8_t col = 0; col < config::BRICK_COLUMNS; ++col) {
                assert(state.bricks[row][col] == level::brickHits(state.level - 1, row, col));
            }
        }
    }
}
