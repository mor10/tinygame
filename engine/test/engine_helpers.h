#pragma once

// Shared helpers for tests that run a whole engine on a small test level.

#include <string>
#include <vector>

#include "../engine.h"
#include "bundle_builder.h"

namespace tg::test {

// A screen with guard pixels all around, to catch drawing outside the canvas.
struct GuardedScreen {
    static constexpr gfx::Color GUARD = 0xDEAD;
    static constexpr int PAD = 4;
    int width, height, stride;
    std::vector<gfx::Color> memory;
    GuardedScreen(int w, int h)
        : width(w), height(h), stride(w + PAD),
          memory(static_cast<std::size_t>(stride) * (h + 2 * PAD), GUARD) {}
    gfx::Surface surface() { return {memory.data() + PAD * stride, width, height, stride}; }
    bool intact() const {
        for (int y = 0; y < height + 2 * PAD; ++y)
            for (int x = 0; x < stride; ++x) {
                const bool visible = y >= PAD && y < height + PAD && x < width;
                if (!visible && memory[static_cast<std::size_t>(y * stride + x)] != GUARD) return false;
            }
        return true;
    }
    // The visible pixels, row by row.
    std::vector<gfx::Color> pixels() const {
        std::vector<gfx::Color> out;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                out.push_back(memory[static_cast<std::size_t>((y + PAD) * stride + x)]);
        return out;
    }
};

// An engine loaded with one small level, with helpers to run it.
struct Arena {
    Engine engine;
    bool ok = false;
    explicit Arena(const std::vector<std::string>& rows, int w = 320, int h = 172) {
        const Bytes b = arenaBundle(rows);
        ok = engine.load(b.data(), b.size(), w, h);
    }
    ActorProbe player() const {
        ActorProbe p;
        engine.player(p);
        return p;
    }
    void run(int ticks, const Input& input = {}) {
        for (int i = 0; i < ticks; ++i) engine.tick(input);
    }
};

inline Input tilt(float x) {
    Input in;
    in.tilt_x = x;
    return in;
}
inline Input press() {
    Input in;
    in.pressed = true;
    in.down = true;
    return in;
}
inline Input release() {
    Input in;
    in.released = true;
    return in;
}

// A 20 x 10 level: ground on the bottom row, and the given rows (row number,
// text; short text is padded with empty cells). Pass 9 -> "" for a bottomless
// floor, or any text to replace the ground.
inline std::vector<std::string> arenaRows(const std::vector<std::pair<int, std::string>>& given,
                                          int cols = 20) {
    std::vector<std::string> rows(10, std::string(static_cast<std::size_t>(cols), '.'));
    rows[9] = std::string(static_cast<std::size_t>(cols), '#');
    for (const auto& g : given) {
        std::string text = g.second;
        text.resize(static_cast<std::size_t>(cols), '.');
        rows[static_cast<std::size_t>(g.first)] = text;
    }
    return rows;
}

}  // namespace tg::test
