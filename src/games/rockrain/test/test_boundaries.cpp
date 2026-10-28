#include <cassert>
#include <initializer_list>

#include "logic.h"

using namespace rockrain;

logic::Game quietGame() {
    logic::Game game;
    game.newGame(1, 0.5f);
    auto& state = game.mutableState();
    state.spawn_timer = state.capsule_timer = state.boss_timer = 1000;
    state.wave_rows_left = 0;
    return game;
}

int main() {
    for (int piece = 0; piece < 6; ++piece) {
        auto game = quietGame();
        auto& state = game.mutableState();
        for (auto& brick : state.bricks) {
            brick.active = true;
            brick.hits_left = 1;
            brick.rect = {0, 40, 16, 8};
        }
        state.bricks[0].rect = {120, 60, 96, 8};
        state.bolts[0] = {true, {127.0f + piece * 16, 60, 2, 6}};
        const auto events = game.step(0.5f);
        const bool edge = piece == 0 || piece == 5;
        assert(state.bricks[0].rect.w == (edge ? 80 : 96));
        assert(state.score == (edge ? config::SCORE_SMALL_BRICK : 0));
        assert(bool(events & logic::EVENT_BRICK_BROKEN) == edge);
    }
    auto game = quietGame();
    auto& state = game.mutableState();
    state.lives = 1;
    state.bricks[0] = {true, false, 1, game.robotHitbox(), 0};
    state.capsules[0] = {true, logic::PowerUp::Life, game.robotHitbox()};
    assert(game.step(0.5f) & logic::EVENT_GAME_OVER);
    assert(state.game_over && state.lives == 0 && state.capsules[0].active);

    for (const std::uint32_t previous : {255U, UINT32_MAX}) {
        game = quietGame();
        auto& wave = game.mutableState();
        wave.waves_done = previous;
        wave.wave_rows_left = 1;
        wave.wave_timer = 0;
        game.step(0.5f);
        assert(wave.waves_done == (previous == UINT32_MAX ? previous : previous + 1));
        assert(game.waveSpeed() == config::WAVE_FALL_SPEED_MAX);
    }
}
