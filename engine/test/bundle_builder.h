#pragma once

// Builds bundles (docs/formats/bundle.md) in memory for tests, so the engine
// tests do not depend on the packer. Also holds the cave_hop game as data.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../game/game_data.h"
#include "../gfx/color.h"
#include "../testcard/testcard.h"

namespace tg::test {

using Bytes = std::vector<std::uint8_t>;

inline std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}

inline void put8(Bytes& b, unsigned v) { b.push_back(static_cast<std::uint8_t>(v)); }
inline void put16(Bytes& b, unsigned v) { put8(b, v & 0xFF); put8(b, (v >> 8) & 0xFF); }
inline void put32(Bytes& b, std::uint32_t v) { put16(b, v & 0xFFFF); put16(b, v >> 16); }
inline void putF(Bytes& b, float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, 4);
    put32(b, bits);
}
inline void putStr(Bytes& b, const std::string& s) {
    put8(b, static_cast<unsigned>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}

struct SpriteSpec {
    std::string name;
    int w = 8, h = 8;
    std::vector<std::uint8_t> pixels;  // one index per pixel
};
struct TileSpec { int sprite = 0; int flags = 0; };
struct KindSpec {
    std::string name;
    int sprite = 0;
    int movement = 0, behaviour = 0, flags = 0, touch = 0;
    float speed = 0;
    int points = 0;
};
struct SpawnSpec { int kind = 0, col = 0, row = 0; };
struct LevelSpec {
    std::string name = "level";
    int cols = 20, rows = 10;
    std::vector<std::uint8_t> cells;
    std::vector<SpawnSpec> spawns;
};

struct BundleSpec {
    std::string id = "test", name = "TEST", author = "Me", starter = "platformer";
    int view = 0, playfield_cols = 0, playfield_rows = 0;
    std::uint16_t palette[18] = {};  // 16 colours, background, ink
    std::vector<SpriteSpec> sprites;
    std::vector<TileSpec> tiles;
    std::vector<KindSpec> kinds;
    std::vector<std::pair<std::string, float>> tuning = {
        {"gravity", 30}, {"jump_height", 3}, {"run_speed", 6}};
    std::vector<LevelSpec> levels;
    bool has_rules = false;
    std::string rules;
};

// Section tags, so a test can drop or reorder one.
struct BuildOptions {
    std::vector<std::string> omit;     // tags left out
    bool reverse_sections = false;     // table (and data) in reverse order
    bool extra_unknown_section = false;
};

inline Bytes buildBundle(const BundleSpec& s, const BuildOptions& opt = {}) {
    std::vector<std::pair<std::string, Bytes>> sections;
    {
        Bytes b;
        putStr(b, s.id); putStr(b, s.name); putStr(b, s.author); putStr(b, s.starter);
        put8(b, s.view); put8(b, s.playfield_cols); put8(b, s.playfield_rows);
        sections.push_back({"META", b});
    }
    {
        Bytes b;
        for (int i = 0; i < 18; ++i) put16(b, s.palette[i]);
        sections.push_back({"PALT", b});
    }
    {
        Bytes b;
        put16(b, static_cast<unsigned>(s.sprites.size()));
        for (const SpriteSpec& sp : s.sprites) {
            putStr(b, sp.name); put8(b, sp.w); put8(b, sp.h);
            const std::size_t n = static_cast<std::size_t>(sp.w) * sp.h;
            Bytes packed((n + 1) / 2, 0);
            for (std::size_t i = 0; i < n && i < sp.pixels.size(); ++i)
                packed[i / 2] |= static_cast<std::uint8_t>((i % 2 == 0) ? (sp.pixels[i] & 15) : (sp.pixels[i] & 15) << 4);
            b.insert(b.end(), packed.begin(), packed.end());
        }
        sections.push_back({"SPRT", b});
    }
    {
        Bytes b;
        put8(b, static_cast<unsigned>(s.tiles.size()));
        for (const TileSpec& t : s.tiles) { put16(b, t.sprite); put16(b, t.flags); }
        sections.push_back({"TILE", b});
    }
    {
        Bytes b;
        put8(b, static_cast<unsigned>(s.kinds.size()));
        for (const KindSpec& k : s.kinds) {
            putStr(b, k.name); put16(b, k.sprite); put8(b, k.movement); put8(b, k.behaviour);
            put8(b, k.flags); put8(b, k.touch); putF(b, k.speed); put32(b, static_cast<std::uint32_t>(k.points));
        }
        sections.push_back({"KIND", b});
    }
    {
        Bytes b;
        put8(b, static_cast<unsigned>(s.tuning.size()));
        for (const auto& t : s.tuning) { putStr(b, t.first); putF(b, t.second); }
        sections.push_back({"TUNE", b});
    }
    {
        Bytes b;
        put8(b, static_cast<unsigned>(s.levels.size()));
        for (const LevelSpec& l : s.levels) {
            putStr(b, l.name); put16(b, l.cols); put16(b, l.rows);
            Bytes cells = l.cells;
            cells.resize(static_cast<std::size_t>(l.cols) * l.rows, 0);
            b.insert(b.end(), cells.begin(), cells.end());
            put16(b, static_cast<unsigned>(l.spawns.size()));
            for (const SpawnSpec& sp : l.spawns) { put16(b, sp.kind); put16(b, sp.col); put16(b, sp.row); }
        }
        sections.push_back({"LEVL", b});
    }
    if (s.has_rules) sections.push_back({"RULE", Bytes(s.rules.begin(), s.rules.end())});
    if (opt.extra_unknown_section) sections.push_back({"XTRA", Bytes{1, 2, 3, 4, 5}});
    for (const std::string& tag : opt.omit)
        for (std::size_t i = 0; i < sections.size(); ++i)
            if (sections[i].first == tag) { sections.erase(sections.begin() + static_cast<long>(i)); break; }
    if (opt.reverse_sections) std::vector<std::pair<std::string, Bytes>>(sections.rbegin(), sections.rend()).swap(sections);

    Bytes body, table;
    const std::size_t data_start = 16 + sections.size() * 12;
    for (const auto& sec : sections) {
        table.insert(table.end(), sec.first.begin(), sec.first.end());
        put32(table, static_cast<std::uint32_t>(data_start + body.size()));
        put32(table, static_cast<std::uint32_t>(sec.second.size()));
        body.insert(body.end(), sec.second.begin(), sec.second.end());
    }
    Bytes out = {'T', 'G', 'B', '1'};
    put16(out, 1);
    put16(out, static_cast<unsigned>(sections.size()));
    put32(out, static_cast<std::uint32_t>(data_start + body.size()));
    put32(out, 0);
    out.insert(out.end(), table.begin(), table.end());
    out.insert(out.end(), body.begin(), body.end());
    const std::uint32_t crc = crc32(out.data() + 16, out.size() - 16);
    for (int i = 0; i < 4; ++i) out[12 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
    return out;
}

// After editing bytes of a bundle, make its size and checksum right again so
// the edit reaches the section checks.
inline void fixHeader(Bytes& b) {
    if (b.size() < 16) return;
    for (int i = 0; i < 4; ++i) b[8 + i] = static_cast<std::uint8_t>(b.size() >> (8 * i));
    const std::uint32_t crc = crc32(b.data() + 16, b.size() - 16);
    for (int i = 0; i < 4; ++i) b[12 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
}

// Offset of a section's data (and its size) in a bundle built by buildBundle.
inline bool findSection(const Bytes& b, const char* tag, std::size_t& offset, std::size_t& size) {
    const unsigned count = b[6] | (b[7] << 8);
    for (unsigned i = 0; i < count; ++i) {
        const std::uint8_t* e = b.data() + 16 + i * 12;
        if (std::memcmp(e, tag, 4) != 0) continue;
        offset = e[4] | (e[5] << 8) | (e[6] << 16) | (static_cast<std::size_t>(e[7]) << 24);
        size = e[8] | (e[9] << 8) | (e[10] << 16) | (static_cast<std::size_t>(e[11]) << 24);
        return true;
    }
    return false;
}

// ---- Cave Hop as data --------------------------------------------------------
// The sprites and level text are copied from games/cave_hop so the tests need
// no files.

inline const char* caveHopSpritesText() {
    return R"(== ground 8x8
77777777
77877787
dd7dd7dd
dddddddd
ddeddddd
dddddded
dedddddd
dddddddd
== brick 8x8
55535555
55535555
33333333
55555355
55555355
33333333
55535555
55535555
== ledge 8x8
dddddddd
deddedde
dddddddd
.f....f.
........
........
........
........
== spikes 8x8
........
........
.3...3..
.3...3..
323.323.
333.333.
33333333
ffffffff
== coin 8x8
..6666..
.626665.
62666665
62666665
66666665
66666655
.666655.
..5555..
== gem 8x8
........
..bbbb..
.bc2bbb.
bccbbbbb
.bbbbbb.
..bbbb..
...bb...
........
== slime 8x8
........
........
..7777..
.777777.
72177217
77777777
77777777
.8.88.8.
== hero 16x16
................
....44444444....
...4444444444...
..444444444444..
..444444444444..
..442214422144..
..442214422144..
..444444444444..
..444444444444..
..444411114444..
..444444444444..
..444444444444..
...4444444444...
...44......44...
..fff......fff..
..fff......fff..
)";
}

inline const char* const CAVE_ROWS[10] = {
    "........................................",
    "........................................",
    "........................................",
    "........................................",
    "........................................",
    "........................................",
    "........oooo................g...........",
    "........----...............BB...........",
    "..P..........B..s..B..^^..BBB....ooo....",
    "########################################",
};

inline std::vector<SpriteSpec> parseSprites(const std::string& text) {
    std::vector<SpriteSpec> out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (line.rfind("== ", 0) == 0) {
            SpriteSpec s;
            char name[32];
            int w = 0, h = 0;
            std::sscanf(line.c_str(), "== %31s %dx%d", name, &w, &h);
            s.name = name; s.w = w; s.h = h;
            out.push_back(s);
        } else if (!out.empty() && !line.empty()) {
            for (char c : line)
                out.back().pixels.push_back(c == '.' ? 0 : (c <= '9' ? c - '0' : c - 'a' + 10));
        }
    }
    return out;
}

inline void setPalette(BundleSpec& s) {
    const gfx::Palette& p = testcard::palette(1);  // BRIGHT
    for (int i = 0; i < 16; ++i) s.palette[i] = p.color[i];
    s.palette[16] = p.background;
    s.palette[17] = p.ink;
}

// Sprites, tiles and kinds of cave_hop (tile order: ground, brick, ledge,
// spikes; kind order: player, slime, coin, gem), with no levels yet.
inline BundleSpec caveHopBase() {
    BundleSpec s;
    s.id = "cave_hop"; s.name = "CAVE HOP"; s.author = "TinyGame";
    setPalette(s);
    s.sprites = parseSprites(caveHopSpritesText());  // ground brick ledge spikes coin gem slime hero
    s.tiles = {{0, TILE_SOLID}, {1, TILE_SOLID}, {2, TILE_ONE_WAY}, {3, TILE_HAZARD}};
    KindSpec player; player.name = "player"; player.sprite = 7; player.movement = 1; player.flags = 1;
    KindSpec slime; slime.name = "slime"; slime.sprite = 6; slime.movement = 1; slime.behaviour = 1;
    slime.speed = 2; slime.touch = 1;
    KindSpec coin; coin.name = "coin"; coin.sprite = 4; coin.behaviour = 2; coin.points = 10;
    KindSpec gem; gem.name = "gem"; gem.sprite = 5;
    s.kinds = {player, slime, coin, gem};
    return s;
}

// Builds a level from text rows. `legend` maps a character to a 1-based tile
// number (tiles) or, for kinds, to a spawn of kind index.
inline LevelSpec levelFromRows(const std::vector<std::string>& rows,
                               const std::map<char, int>& tiles, const std::map<char, int>& kinds) {
    LevelSpec l;
    l.rows = static_cast<int>(rows.size());
    l.cols = static_cast<int>(rows[0].size());
    for (int r = 0; r < l.rows; ++r)
        for (int c = 0; c < l.cols; ++c) {
            const char ch = rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
            std::uint8_t cell = 0;
            if (tiles.count(ch)) cell = static_cast<std::uint8_t>(tiles.at(ch));
            else if (kinds.count(ch)) l.spawns.push_back({kinds.at(ch), c, r});
            l.cells.push_back(cell);
        }
    return l;
}

inline BundleSpec caveHopSpec() {
    BundleSpec s = caveHopBase();
    const std::vector<std::string> rows(CAVE_ROWS, CAVE_ROWS + 10);
    LevelSpec l = levelFromRows(rows, {{'#', 1}, {'B', 2}, {'-', 3}, {'^', 4}},
                                {{'P', 0}, {'s', 1}, {'o', 2}, {'g', 3}});
    l.name = "cave 1";
    s.levels.push_back(l);
    return s;
}

inline Bytes caveHopBundle() { return buildBundle(caveHopSpec()); }

// A small game for physics tests: the cave_hop sprites, tiles and kinds, and
// one level made from `rows` ('#' ground, 'B' brick, '-' ledge, '^' spikes,
// 'P' player, 's' slime, 'o' coin, 'g' gem).
inline BundleSpec arenaSpec(const std::vector<std::string>& rows) {
    BundleSpec s = caveHopBase();
    s.levels.push_back(levelFromRows(rows, {{'#', 1}, {'B', 2}, {'-', 3}, {'^', 4}},
                                     {{'P', 0}, {'s', 1}, {'o', 2}, {'g', 3}}));
    return s;
}

inline Bytes arenaBundle(const std::vector<std::string>& rows) { return buildBundle(arenaSpec(rows)); }

}  // namespace tg::test
