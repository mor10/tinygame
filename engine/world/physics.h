#pragma once

// Tile collision and platformer movement, as written in docs/formats/motion.md.
// Pure functions over a TileGrid and an Actor; no state, no allocation.

#include <cstdint>

#include "actors.h"

namespace tg {

inline constexpr float MAX_SPEED = 30.0f;  // blocks per second (0.5 block per tick)
inline constexpr float MAX_STEP = 0.5f;    // blocks per tick on one axis
inline constexpr float FALL_OUT_ROWS = 2.0f;

// A level seen by the physics: cells plus the flags of each tile.
struct TileGrid {
    int cols = 0;
    int rows = 0;
    const std::uint8_t* cells = nullptr;        // cols * rows; 0 = empty, n = tile n-1
    const std::uint16_t* tile_flags = nullptr;  // TileFlag bits, one per tile
    int tile_count = 0;

    // Left, right and top outside the level are solid; below it is open.
    std::uint16_t flagsAt(int col, int row) const;
};

// One tick of platformer movement: gravity, then X, then Y, each resolved
// against the tiles. Sets or clears ACTOR_ON_GROUND and ACTOR_HIT_WALL.
void stepPlatformer(Actor& actor, const TileGrid& grid, float gravity);

// True when the actor's box overlaps a cell whose flags include `flag`.
bool overlapsTileFlag(const TileGrid& grid, const Actor& actor, std::uint16_t flag);
// True when the actor's feet rest exactly on a solid or one-way tile.
bool isSupported(const TileGrid& grid, const Actor& actor);
bool actorsOverlap(const Actor& a, const Actor& b);
// The actor's top has passed rows + 2.
bool hasFallenOut(const TileGrid& grid, const Actor& actor);

}  // namespace tg
