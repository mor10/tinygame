// Reads a bundle (docs/formats/bundle.md) into a GameData. Every byte is
// treated as untrusted: all reads go through a bounds-checked Reader, and
// every index and limit is checked before the data is accepted.

#include <cmath>
#include <cstring>
#include <utility>

#include "game_data.h"

namespace tg {

namespace {

constexpr std::size_t HEADER_SIZE = 16;
constexpr std::size_t SECTION_ENTRY_SIZE = 12;
constexpr std::size_t MAX_BUNDLE_SIZE = 262144;
constexpr int MAX_SPRITES = 255;
constexpr int MAX_KINDS = 64;
constexpr int MAX_LEVELS = 32;
constexpr int MAX_LEVEL_CELLS = 16384;
constexpr int MAX_SPAWNS = 256;
constexpr int MAX_SPRITE_SIDE = 32;
constexpr std::size_t PALETTE_SIZE = 36;
constexpr std::uint16_t KNOWN_TILE_FLAGS = TILE_SOLID | TILE_ONE_WAY | TILE_HAZARD;

// CRC-32 with the zlib polynomial (reflected 0xEDB88320), one table lookup
// per byte. The table is worked out by the compiler.
struct CrcTable {
    std::uint32_t entry[256];
    constexpr CrcTable() : entry() {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int bit = 0; bit < 8; ++bit) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            entry[i] = c;
        }
    }
};
constexpr CrcTable CRC_TABLE;

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) crc = CRC_TABLE.entry[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

std::uint16_t readU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t readU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// Walks one section. A read past the end returns zero and sets `failed`; it
// never touches memory outside the section.
class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    bool failed() const { return failed_; }
    bool atEnd() const { return position_ == size_; }
    std::size_t left() const { return size_ - position_; }

    // The next `count` bytes, or nullptr (and failed) if there are not that many.
    const std::uint8_t* take(std::size_t count) {
        if (failed_ || count > size_ - position_) {
            failed_ = true;
            return nullptr;
        }
        const std::uint8_t* at = data_ + position_;
        position_ += count;
        return at;
    }
    std::uint8_t u8() {
        const std::uint8_t* p = take(1);
        return p ? p[0] : 0;
    }
    std::uint16_t u16() {
        const std::uint8_t* p = take(2);
        return p ? readU16(p) : 0;
    }
    std::uint32_t u32() {
        const std::uint8_t* p = take(4);
        return p ? readU32(p) : 0;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    float f32() {
        const std::uint32_t bits = u32();
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    // One length byte, then that many printable ASCII bytes.
    std::string string() {
        const std::size_t length = u8();
        const std::uint8_t* p = take(length);
        if (p == nullptr) return {};
        for (std::size_t i = 0; i < length; ++i) {
            if (p[i] < 0x20 || p[i] > 0x7E) {
                failed_ = true;
                return {};
            }
        }
        return std::string(reinterpret_cast<const char*>(p), length);
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t position_ = 0;
    bool failed_ = false;
};

struct Section {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    bool present = false;
};

// Each parser returns None, Malformed or TooLarge.
LoadError finish(const Reader& in) {
    return in.failed() || !in.atEnd() ? LoadError::Malformed : LoadError::None;
}

LoadError parseMeta(Reader in, GameData& game) {
    game.id = in.string();
    game.name = in.string();
    game.author = in.string();
    game.starter = in.string();
    const int view = in.u8();
    game.playfield_cols = in.u8();
    game.playfield_rows = in.u8();
    if (in.failed() || view > 1) return LoadError::Malformed;
    // A playfield has both sides or neither.
    if ((game.playfield_cols == 0) != (game.playfield_rows == 0)) return LoadError::Malformed;
    game.view = view == 1 ? View::Fill : View::Standard;
    return finish(in);
}

LoadError parsePalette(Reader in, GameData& game) {
    if (in.left() != PALETTE_SIZE) return LoadError::Malformed;
    for (int i = 0; i < 16; ++i) game.palette.color[i] = in.u16();
    game.palette.color[0] = 0;  // index 0 is transparent; its entry is unused
    game.palette.background = in.u16();
    game.palette.ink = in.u16();
    return finish(in);
}

LoadError parseSprites(Reader in, GameData& game) {
    const int count = in.u16();
    if (in.failed()) return LoadError::Malformed;
    if (count > MAX_SPRITES) return LoadError::TooLarge;
    game.sprites.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        SpriteData sprite;
        sprite.name = in.string();
        sprite.width = in.u8();
        sprite.height = in.u8();
        if (in.failed() || sprite.width == 0 || sprite.height == 0) return LoadError::Malformed;
        if (sprite.width > MAX_SPRITE_SIDE || sprite.height > MAX_SPRITE_SIDE)
            return LoadError::TooLarge;
        const std::size_t pixel_count = static_cast<std::size_t>(sprite.width) * sprite.height;
        const std::uint8_t* packed = in.take((pixel_count + 1) / 2);
        if (packed == nullptr) return LoadError::Malformed;
        sprite.pixels.resize(pixel_count);
        for (std::size_t p = 0; p < pixel_count; ++p) {
            const std::uint8_t byte = packed[p / 2];
            sprite.pixels[p] = (p % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
        }
        game.sprites.push_back(std::move(sprite));
    }
    return finish(in);
}

LoadError parseTiles(Reader in, GameData& game) {
    const int count = in.u8();
    if (in.failed()) return LoadError::Malformed;
    game.tiles.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        TileData tile;
        tile.sprite = in.u16();
        tile.flags = in.u16();
        if (in.failed()) return LoadError::Malformed;
        if (tile.sprite >= static_cast<int>(game.sprites.size())) return LoadError::Malformed;
        if ((tile.flags & ~KNOWN_TILE_FLAGS) != 0) return LoadError::Malformed;
        const SpriteData& sprite = game.sprites[static_cast<std::size_t>(tile.sprite)];
        if (sprite.width != ART_BLOCK || sprite.height != ART_BLOCK) return LoadError::Malformed;
        game.tiles.push_back(tile);
    }
    return finish(in);
}

LoadError parseKinds(Reader in, GameData& game) {
    const int count = in.u8();
    if (in.failed()) return LoadError::Malformed;
    if (count > MAX_KINDS) return LoadError::TooLarge;
    game.kinds.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        KindData kind;
        kind.name = in.string();
        kind.sprite = in.u16();
        const int movement = in.u8();
        const int behaviour = in.u8();
        const int flags = in.u8();
        const int touch = in.u8();
        kind.speed = in.f32();
        kind.points = in.i32();
        if (in.failed()) return LoadError::Malformed;
        if (kind.sprite >= static_cast<int>(game.sprites.size())) return LoadError::Malformed;
        if (movement > 1 || behaviour > 2 || flags > 1 || touch > 1) return LoadError::Malformed;
        if (!std::isfinite(kind.speed)) return LoadError::Malformed;
        kind.movement = static_cast<Movement>(movement);
        kind.behaviour = static_cast<Behaviour>(behaviour);
        kind.player = (flags & 1) != 0;
        kind.on_touch_player = static_cast<TouchPlayer>(touch);
        game.kinds.push_back(std::move(kind));
    }
    return finish(in);
}

LoadError parseTuning(Reader in, GameData& game) {
    const int count = in.u8();
    for (int i = 0; i < count; ++i) {
        const std::string key = in.string();
        const float value = in.f32();
        if (in.failed() || !std::isfinite(value)) return LoadError::Malformed;
        if (key == "gravity") game.tuning.gravity = value;
        else if (key == "jump_height") game.tuning.jump_height = value;
        else if (key == "run_speed") game.tuning.run_speed = value;
        // Unknown keys are ignored (bundle.md).
    }
    return finish(in);
}

LoadError parseLevels(Reader in, GameData& game) {
    const int count = in.u8();
    if (in.failed() || count == 0) return LoadError::Malformed;  // a game needs a first level
    if (count > MAX_LEVELS) return LoadError::TooLarge;
    game.levels.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        LevelData level;
        level.name = in.string();
        level.cols = in.u16();
        level.rows = in.u16();
        if (in.failed() || level.cols == 0 || level.rows == 0) return LoadError::Malformed;
        const std::size_t cell_count = static_cast<std::size_t>(level.cols) * level.rows;
        if (cell_count > static_cast<std::size_t>(MAX_LEVEL_CELLS)) return LoadError::TooLarge;
        const std::uint8_t* cells = in.take(cell_count);
        if (cells == nullptr) return LoadError::Malformed;
        for (std::size_t c = 0; c < cell_count; ++c) {
            if (cells[c] > game.tiles.size()) return LoadError::Malformed;
        }
        level.cells.assign(cells, cells + cell_count);
        const int spawn_count = in.u16();
        if (in.failed()) return LoadError::Malformed;
        if (spawn_count > MAX_SPAWNS) return LoadError::TooLarge;
        level.spawns.reserve(static_cast<std::size_t>(spawn_count));
        for (int s = 0; s < spawn_count; ++s) {
            SpawnData spawn;
            spawn.kind = in.u16();
            spawn.col = in.u16();
            spawn.row = in.u16();
            if (in.failed()) return LoadError::Malformed;
            if (spawn.kind >= static_cast<int>(game.kinds.size()) || spawn.col >= level.cols ||
                spawn.row >= level.rows)
                return LoadError::Malformed;
            level.spawns.push_back(spawn);
        }
        game.levels.push_back(std::move(level));
    }
    return finish(in);
}

LoadError parseBundle(const std::uint8_t* data, std::size_t size, GameData& game) {
    if (data == nullptr || size < HEADER_SIZE) return LoadError::TooSmall;
    if (std::memcmp(data, "TGB1", 4) != 0) return LoadError::BadMagic;
    if (readU16(data + 4) != BUNDLE_API) return LoadError::WrongApi;
    const std::size_t section_count = readU16(data + 6);
    if (readU32(data + 8) != size) return LoadError::BadSize;
    if (size > MAX_BUNDLE_SIZE) return LoadError::TooLarge;
    if (crc32(data + HEADER_SIZE, size - HEADER_SIZE) != readU32(data + 12))
        return LoadError::BadChecksum;

    const std::size_t table_end = HEADER_SIZE + section_count * SECTION_ENTRY_SIZE;
    if (table_end > size) return LoadError::Malformed;

    static const char* const TAGS[] = {"META", "PALT", "SPRT", "TILE", "KIND", "TUNE", "LEVL", "RULE"};
    enum { META, PALT, SPRT, TILE, KIND, TUNE, LEVL, RULE, TAG_COUNT };
    Section sections[TAG_COUNT];
    for (std::size_t i = 0; i < section_count; ++i) {
        const std::uint8_t* entry = data + HEADER_SIZE + i * SECTION_ENTRY_SIZE;
        const std::size_t offset = readU32(entry + 4);
        const std::size_t length = readU32(entry + 8);
        // Every section, known or not, must lie inside the bundle after the table.
        if (offset < table_end || offset > size || length > size - offset)
            return LoadError::Malformed;
        for (int tag = 0; tag < TAG_COUNT; ++tag) {
            if (std::memcmp(entry, TAGS[tag], 4) != 0) continue;
            if (sections[tag].present) return LoadError::Malformed;  // the same section twice
            sections[tag] = {data + offset, length, true};
        }
    }
    for (int tag = 0; tag < TAG_COUNT; ++tag) {
        if (tag != RULE && !sections[tag].present) return LoadError::MissingSection;
    }

    using Parser = LoadError (*)(Reader, GameData&);
    // Sprites before tiles and kinds, and those before levels: later sections
    // hold indexes into earlier ones.
    static const struct {
        int tag;
        Parser parse;
    } ORDER[] = {{META, parseMeta}, {PALT, parsePalette}, {SPRT, parseSprites}, {TILE, parseTiles},
                 {KIND, parseKinds}, {TUNE, parseTuning}, {LEVL, parseLevels}};
    for (const auto& step : ORDER) {
        const Section& section = sections[step.tag];
        const LoadError error = step.parse(Reader(section.data, section.size), game);
        if (error != LoadError::None) return error;
    }
    if (sections[RULE].present) {
        game.rules.assign(reinterpret_cast<const char*>(sections[RULE].data), sections[RULE].size);
    }
    return LoadError::None;
}

}  // namespace

const char* loadErrorText(LoadError error) {
    switch (error) {
        case LoadError::None: return "";
        case LoadError::TooSmall: return "This game file is too small to be a game.";
        case LoadError::BadMagic: return "This file is not a TinyGame game.";
        case LoadError::WrongApi: return "This game was made for a different version of TinyGame.";
        case LoadError::BadSize: return "This game file is not complete.";
        case LoadError::BadChecksum: return "This game file is damaged.";
        case LoadError::MissingSection: return "This game file has a part missing.";
        case LoadError::Malformed: return "This game file is mixed up inside.";
        case LoadError::TooLarge: return "This game is too big for the console.";
    }
    return "This game file could not be read.";
}

LoadError loadBundle(const std::uint8_t* data, std::size_t size, GameData& out) {
    GameData game;
    const LoadError error = parseBundle(data, size, game);
    out = error == LoadError::None ? std::move(game) : GameData{};
    return error;
}

}  // namespace tg
