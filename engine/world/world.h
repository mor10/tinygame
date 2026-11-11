#pragma once

// The simulated world: the actor pool, the level's tiles, the score and the
// player's controls. One tick follows "Order within a tick" in
// docs/formats/motion.md, steps 1 and 2 (input, movement, behaviours, stock
// touches). Lua rules (step 3) and the camera (step 4) are the engine's job.
//
// Connecting Lua: a ScriptWorld adapter (engine/lua/script.h) sits on top of
// this class. An ActorRef {index, serial} maps to World::alive / actor /
// find / remove / addScore / score / random; X, Y, Vx, Vy are fields of
// Actor, and OnGround is Actor::onGround(). Use setField-style writes only
// through World::set so values stay finite.

#include <cstdint>
#include <vector>

#include "../core/input.h"
#include "../core/random.h"
#include "../game/game_data.h"
#include "actors.h"
#include "physics.h"

namespace tg {

class World {
public:
    // Starts level `level` of `game`, which must outlive the world. False if
    // there is no such level. Allocates; call at load time only.
    bool start(const GameData& game, int level);
    void tick(const Input& input);  // steps 1 and 2 of a tick

    const GameData& game() const { return *game_; }
    const TileGrid& grid() const { return grid_; }
    ActorPool& pool() { return pool_; }
    const ActorPool& pool() const { return pool_; }

    int score() const { return score_; }
    void addScore(int amount) { score_ += amount; }
    int hurts() const { return hurts_; }
    std::uint32_t random() { return rng_.next(); }

    // Pool index of the player, or -1 when the level has none (or it was removed).
    int playerIndex() const { return pool_.valid(player_) ? player_ : -1; }
    const char* kindName(const Actor& actor) const;
    const KindData& kindOf(const Actor& actor) const { return game_->kinds[actor.kind]; }

    // Handle-based access for scripts. A handle is (index, serial).
    bool alive(int index, std::uint32_t serial) const { return pool_.alive(index, serial); }
    Actor* actor(int index, std::uint32_t serial) {
        return pool_.alive(index, serial) ? &pool_.at(index) : nullptr;
    }
    void remove(int index, std::uint32_t serial) {
        if (pool_.alive(index, serial)) pool_.remove(index);
    }
    // First living actor of the kind with this name.
    bool find(const char* kind, int& index, std::uint32_t& serial) const;
    // Sets a position or velocity; ignores values that are not finite.
    static void set(Actor& actor, float Actor::*field, float value);

    // Puts the player back at its spawn with no speed and counts a hurt.
    void hurtPlayer();

private:
    struct PlayerState {
        int knob_dir = 0;
        int knob_hold = 0;
        int press_age = 255;  // ticks since the last press
        int air_ticks = 0;    // ticks since the feet last touched ground
        bool jumping = false;
    };

    void respawnPlayer();
    void playerInput(Actor& player, const Input& input);
    void walkerThink(Actor& walker, const KindData& kind) const;
    void touches();

    const GameData* game_ = nullptr;
    const LevelData* level_ = nullptr;
    std::vector<std::uint16_t> tile_flags_;
    TileGrid grid_;
    ActorPool pool_;
    int player_ = -1;
    float spawn_x_ = 0, spawn_y_ = 0;
    PlayerState state_;
    float gravity_ = 30, jump_speed_ = 0, run_speed_ = 6;
    int score_ = 0;
    int hurts_ = 0;
    Random rng_{1};
};

}  // namespace tg
