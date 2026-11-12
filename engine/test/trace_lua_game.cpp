#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bundle_script.h"
#include "fixed_step_runner.h"
#include "trace_bundle.h"

std::uint32_t bits(float value) {
    std::uint32_t result;
    static_assert(sizeof(result) == sizeof(value));
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

int main() {
    constexpr char random_source[] =
        "function update() "
        "local gameplay = math.random(1, 100) "
        "local cosmetic = tg.cosmetic_random(1, 100) "
        "tg.text(0, 0, gameplay .. ':' .. cosmetic, 1) end";
    tg::engine::LuaRuntime random(1);
    assert(random.load(random_source, sizeof(random_source) - 1));
    for (unsigned i = 0; i < 32; ++i) {
        assert(random.callUpdate(1.0f / 60.0f, tg::Input{}));
        assert(random.frame().size() == 1);
        std::printf("random %s\n", random.frame().at(0).text);
    }

    for (const tg::engine::WorldSize world : {
             tg::engine::WorldSize{152.0f, 86.0f},
             tg::engine::WorldSize{120.0f, 108.0f},
         }) {
        tg::engine::BundleScript game(17);
        assert(game.load(trace_bundle, sizeof(trace_bundle)));
        tg::engine::FixedStepRunner runner;
        unsigned tick = 0;
        bool saw_game_over = false;
        bool saw_reset = false;
        bool saw_score = false;
        auto update = [&](const tg::Input& input) {
            if (!game.update(tg::engine::FixedStepRunner::STEP_SECONDS, input, world)) {
                std::fprintf(stderr, "Lua trace failed: %s\n", game.error());
                return false;
            }
            const auto& frame = game.frame();
            if (frame.size() > 4 && std::strcmp(frame.at(4).text, "GAME OVER") == 0) {
                saw_game_over = true;
            } else if (saw_game_over && frame.size() == 4) {
                saw_reset = true;
            }
            if (std::strcmp(frame.at(3).text, "SCORE 1") == 0) saw_score = true;
            std::printf("tick %u %zu\n", ++tick, frame.size());
            for (std::size_t i = 0; i < frame.size(); ++i) {
                const auto& command = frame.at(i);
                std::printf("%u %08x %08x %08x %08x %u %s\n",
                            static_cast<unsigned>(command.kind), bits(command.x), bits(command.y),
                            bits(command.width), bits(command.height),
                            static_cast<unsigned>(command.palette_index), command.text);
            }
            return true;
        };
        for (unsigned frame = 0; frame < 650; ++frame) {
            tg::Input input{};
            if (frame == 4) input.main.delta = 8;
            if (frame == 80) input.main.delta = -8;
            if (frame == 300) input.click = true;
            if (frame == 320) input.main.delta = 40;
            const auto result = runner.advance(frame == 30 ? 200'000 : 16'667, input, update);
            assert(result.error == tg::engine::StepError::None);
        }
        assert(tick > 650 && saw_game_over && saw_reset && saw_score);
    }
}
