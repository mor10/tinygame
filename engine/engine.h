#pragma once

// The one doorway into the engine. Every host (console firmware, browser
// simulator, headless runner, tests) loads a bundle, feeds input once per
// 60 Hz tick, and asks for a frame. Nothing else in engine/ is a host API.

#include <cstddef>
#include <cstdint>

#include "core/input.h"
#include "gfx/canvas.h"

namespace tg {

struct ActorProbe {
    int index = 0;
    const char* kind = "";
    float x = 0, y = 0;    // top-left corner, in blocks
    float vx = 0, vy = 0;  // blocks per second
    bool on_ground = false;
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Loads a game for a screen of the given size and starts its first level.
    // False on failure; error() then says why in words a child can read.
    bool load(const std::uint8_t* bundle, std::size_t size, int screen_w, int screen_h);
    // "" while everything is fine. Set by a failed load or a game error (for
    // example a Lua error); after a game error tick() does nothing.
    const char* error() const;

    void tick(const Input& input);          // one 60 Hz step
    void draw(gfx::Canvas& canvas) const;   // the whole screen, status area included

    // ---- Probes: cheap text-friendly state for tests and agents ----------
    std::uint32_t ticks() const;
    int score() const;
    int hurts() const;                      // times the player has been hurt
    int actorCount() const;                 // living actors
    bool actor(int n, ActorProbe& out) const;      // n-th living actor
    bool player(ActorProbe& out) const;
    // Writes lines such as "tick 120", "score 10", "hurts 0", "sparkles 0",
    // "actor 0 player x=2.00 y=6.00 vx=0.00 vy=0.00 ground=1". "sparkles N" counts
    // fx.sparkle calls.
    // Returns the number of characters written (always NUL-terminated).
    std::size_t probeText(char* buffer, std::size_t capacity) const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace tg
