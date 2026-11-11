#pragma once

// A loaded game: what a bundle (docs/formats/bundle.md) becomes in memory.
// Loading may allocate; nothing here allocates per frame.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../gfx/palette.h"
#include "../world/stage.h"

namespace tg {

inline constexpr int BUNDLE_API = 1;
inline constexpr int ART_BLOCK = 8;  // art pixels per block

struct SpriteData {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // one palette index per pixel, row by row
};

enum TileFlag : std::uint16_t { TILE_SOLID = 1, TILE_ONE_WAY = 2, TILE_HAZARD = 4 };

struct TileData {
    int sprite = 0;
    std::uint16_t flags = 0;
};

enum class Movement : std::uint8_t { None = 0, Platformer = 1 };
enum class Behaviour : std::uint8_t { None = 0, Walker = 1, Pickup = 2 };
enum class TouchPlayer : std::uint8_t { Nothing = 0, Hurt = 1 };

struct KindData {
    std::string name;
    int sprite = 0;
    Movement movement = Movement::None;
    Behaviour behaviour = Behaviour::None;
    bool player = false;
    TouchPlayer on_touch_player = TouchPlayer::Nothing;
    float speed = 0.0f;  // blocks per second
    int points = 0;
};

struct SpawnData {
    int kind = 0;
    int col = 0;
    int row = 0;
};

struct LevelData {
    std::string name;
    int cols = 0;
    int rows = 0;
    std::vector<std::uint8_t> cells;  // cols * rows; 0 = empty, n = tile n-1
    std::vector<SpawnData> spawns;
};

struct Tuning {
    float gravity = 30.0f;     // blocks per second squared
    float jump_height = 3.0f;  // blocks
    float run_speed = 6.0f;    // blocks per second
};

struct GameData {
    std::string id;
    std::string name;
    std::string author;
    std::string starter;
    View view = View::Standard;
    int playfield_cols = 0;
    int playfield_rows = 0;
    gfx::Palette palette{"GAME", {}, 0, 0};
    std::vector<SpriteData> sprites;
    std::vector<TileData> tiles;
    std::vector<KindData> kinds;
    std::vector<LevelData> levels;
    Tuning tuning;
    std::string rules;  // Lua source; empty when the game has none
};

enum class LoadError {
    None,
    TooSmall,
    BadMagic,
    WrongApi,
    BadSize,
    BadChecksum,
    MissingSection,
    Malformed,   // a section runs past its end or holds an out-of-range value
    TooLarge,    // a documented limit is exceeded
};

// A short message a child can read, for example "This game file is damaged."
const char* loadErrorText(LoadError error);

// Parses and validates a bundle. On any error `out` is left empty.
LoadError loadBundle(const std::uint8_t* data, std::size_t size, GameData& out);

}  // namespace tg
