#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "camera.h"
#include "fixed_step_runner.h"

int main() {
    tg::engine::FixedStepRunner runner;
    tg::engine::Camera wide;
    tg::engine::Camera tall;
    assert(wide.configure({100, 80, 20, 10, 4, 2, 6, 8}));
    assert(tall.configure({100, 80, 16, 14, 3, 3, 5, 11}));
    std::uint32_t tick = 0;
    float position = 0.0f;
    auto update = [&](const tg::Input& input) {
        position += static_cast<float>(input.main.delta) * 0.25f;
        position += 3.0f * tg::engine::FixedStepRunner::STEP_SECONDS;
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(position));
        std::memcpy(&bits, &position, sizeof(bits));
        assert(wide.follow(position + 10.0f, 4.0f));
        assert(tall.follow(position + 10.0f, 4.0f));
        std::uint32_t wide_bits = 0, tall_bits = 0;
        const float wide_x = wide.x(), tall_x = tall.x();
        std::memcpy(&wide_bits, &wide_x, sizeof(wide_bits));
        std::memcpy(&tall_bits, &tall_x, sizeof(tall_bits));
        std::printf("tick %u %d %u %u %u %08x\n", ++tick, input.main.delta,
                    input.main.down ? 1U : 0U, input.main.pressed ? 1U : 0U,
                    input.main.released ? 1U : 0U, bits);
        std::printf("camera %08x %08x\n", wide_bits, tall_bits);
        return true;
    };

    for (unsigned frame = 0; frame < 100; ++frame) {
        tg::Input input{};
        input.main.down = frame >= 10 && frame < 20;
        if (frame == 10) input.main.pressed = true;
        if (frame == 20) input.main.released = true;
        if (frame == 4) input.main.delta = 7;
        if (frame == 31) input.main.delta = -9;
        if (frame == 40) runner.discardInput();
        const std::uint64_t elapsed = frame == 30 ? 200'000 :
                                      frame % 2 == 0 ? 8'000 : 16'000;
        const auto result = runner.advance(elapsed, input, update);
        assert(result.error == tg::engine::StepError::None);
        std::printf("frame %u %u %llu\n", frame, result.steps,
                    static_cast<unsigned long long>(result.pending_steps));
    }
    assert(tick > 60);
}
