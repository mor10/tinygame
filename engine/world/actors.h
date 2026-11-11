#pragma once

// The actor pool: a fixed array of 64 slots, no allocation while playing.
// A slot that is freed and used again gets a new serial number, so a handle
// made of (index, serial) can tell that "its" actor is gone.

#include <cstdint>

namespace tg {

inline constexpr int MAX_ACTORS = 64;

enum ActorFlag : std::uint32_t {
    ACTOR_ON_GROUND = 1,  // landed during the last move
    ACTOR_HIT_WALL = 2,   // blocked sideways during the last move
    ACTOR_PLAYER = 4,     // the player-controlled actor
};

struct Actor {
    bool alive = false;
    std::uint32_t serial = 0;  // 0 never belongs to an actor
    int kind = 0;              // index into GameData::kinds
    float x = 0, y = 0;        // top-left corner, in blocks
    float vx = 0, vy = 0;      // blocks per second
    float w = 1, h = 1;        // size in blocks
    int facing = 1;            // -1 left, +1 right
    std::uint32_t flags = 0;

    bool onGround() const { return (flags & ACTOR_ON_GROUND) != 0; }
    bool hitWall() const { return (flags & ACTOR_HIT_WALL) != 0; }
    bool isPlayer() const { return (flags & ACTOR_PLAYER) != 0; }
};

class ActorPool {
public:
    // Uses the first free slot. Returns its index, or -1 when the pool is full.
    int spawn(int kind, float x, float y, float w, float h);
    void remove(int index);  // ignores a bad or empty slot
    void clear();            // removes everything; serials keep counting

    bool valid(int index) const { return index >= 0 && index < MAX_ACTORS && actors_[index].alive; }
    // True when slot `index` holds the actor with this serial.
    bool alive(int index, std::uint32_t serial) const {
        return valid(index) && actors_[index].serial == serial;
    }
    Actor& at(int index) { return actors_[index]; }
    const Actor& at(int index) const { return actors_[index]; }
    int count() const { return count_; }

private:
    Actor actors_[MAX_ACTORS];
    int count_ = 0;
    std::uint32_t next_serial_ = 1;
};

}  // namespace tg
