#include <vector>

#include "../gfx/sprite.h"
#include "../world/tilemap.h"
#include "bundle_builder.h"
#include "tg_test.h"

using namespace tg;
using namespace tg::test;

namespace {

LoadError load(const Bytes& b, GameData& out) { return loadBundle(b.data(), b.size(), out); }

// Overwrites a byte inside a section's data and repairs size and checksum.
LoadError loadWithEdit(const char* tag, std::size_t at, std::uint8_t value) {
    Bytes b = caveHopBundle();
    std::size_t offset = 0, size = 0;
    CHECK(findSection(b, tag, offset, size));
    b[offset + at] = value;
    fixHeader(b);
    GameData g;
    return load(b, g);
}

}  // namespace

TG_TEST(crc32_matches_the_zlib_check_value) {
    const char* text = "123456789";
    CHECK_EQ(crc32(reinterpret_cast<const std::uint8_t*>(text), 9), 0xCBF43926u);
}

TG_TEST(valid_bundle_loads_with_every_field_right) {
    const Bytes b = caveHopBundle();
    GameData g;
    CHECK(load(b, g) == LoadError::None);
    CHECK_EQ(g.id, std::string("cave_hop"));
    CHECK_EQ(g.name, std::string("CAVE HOP"));
    CHECK_EQ(g.author, std::string("TinyGame"));
    CHECK_EQ(g.starter, std::string("platformer"));
    CHECK(g.view == View::Standard);
    CHECK_EQ(g.playfield_cols, 0);
    const gfx::Palette& bright = testcard::palette(1);
    for (int i = 1; i < 16; ++i) CHECK_EQ(g.palette.color[i], bright.color[i]);
    CHECK_EQ(g.palette.background, bright.background);
    CHECK_EQ(g.palette.ink, bright.ink);
    CHECK_EQ(g.sprites.size(), 8u);
    CHECK_EQ(g.sprites[0].name, std::string("ground"));
    CHECK_EQ(g.sprites[0].width, 8);
    CHECK_EQ(g.sprites[0].pixels.size(), 64u);
    CHECK_EQ(int(g.sprites[0].pixels[0]), 7);   // '7'
    CHECK_EQ(int(g.sprites[0].pixels[10]), 8);   // second row, '8'
    CHECK_EQ(int(g.sprites[0].pixels[16]), 13); // 'd'
    CHECK_EQ(int(g.sprites[2].pixels[3 * 8 + 0]), 0);   // '.' is transparent
    CHECK_EQ(int(g.sprites[2].pixels[3 * 8 + 1]), 15);  // 'f'
    CHECK_EQ(g.sprites[7].name, std::string("hero"));
    CHECK_EQ(g.sprites[7].width, 16);
    CHECK_EQ(g.sprites[7].height, 16);
    CHECK_EQ(g.sprites[7].pixels.size(), 256u);
    CHECK_EQ(g.tiles.size(), 4u);
    CHECK_EQ(g.tiles[2].sprite, 2);
    CHECK_EQ(g.tiles[0].flags, TILE_SOLID);
    CHECK_EQ(g.tiles[2].flags, TILE_ONE_WAY);
    CHECK_EQ(g.tiles[3].flags, TILE_HAZARD);
    CHECK_EQ(g.kinds.size(), 4u);
    CHECK_EQ(g.kinds[0].name, std::string("player"));
    CHECK(g.kinds[0].player);
    CHECK(g.kinds[0].movement == Movement::Platformer);
    CHECK(g.kinds[1].behaviour == Behaviour::Walker);
    CHECK(g.kinds[1].on_touch_player == TouchPlayer::Hurt);
    CHECK_EQ(g.kinds[1].speed, 2.0f);
    CHECK(g.kinds[2].behaviour == Behaviour::Pickup);
    CHECK_EQ(g.kinds[2].points, 10);
    CHECK_EQ(g.kinds[2].sprite, 4);
    CHECK_EQ(g.tuning.gravity, 30.0f);
    CHECK_EQ(g.tuning.jump_height, 3.0f);
    CHECK_EQ(g.tuning.run_speed, 6.0f);
    CHECK_EQ(g.levels.size(), 1u);
    const LevelData& l = g.levels[0];
    CHECK_EQ(l.name, std::string("cave 1"));
    CHECK_EQ(l.cols, 40);
    CHECK_EQ(l.rows, 10);
    CHECK_EQ(l.cells.size(), 400u);
    CHECK_EQ(int(l.cells[9 * 40]), 1);      // ground
    CHECK_EQ(int(l.cells[8 * 40 + 13]), 2); // brick
    CHECK_EQ(int(l.cells[7 * 40 + 8]), 3);  // ledge
    CHECK_EQ(int(l.cells[8 * 40 + 22]), 4); // spikes
    CHECK_EQ(int(l.cells[8 * 40 + 2]), 0);  // the player's cell is empty
    CHECK_EQ(l.spawns.size(), 1u + 1u + 4u + 3u + 1u);
    CHECK_EQ(l.spawns[0].kind, 2);  // row-major: the first coin
    CHECK_EQ(l.spawns[0].col, 8);
    CHECK_EQ(l.spawns[0].row, 6);
    CHECK(g.rules.empty());
}

TG_TEST(rules_text_and_unknown_things_are_handled) {
    BundleSpec s = caveHopSpec();
    s.has_rules = true;
    s.rules = "on.tick(function() end)\n";
    s.tuning.push_back({"mystery", 5.0f});
    BuildOptions opt;
    opt.extra_unknown_section = true;
    opt.reverse_sections = true;
    GameData g;
    CHECK(load(buildBundle(s, opt), g) == LoadError::None);
    CHECK_EQ(g.rules, s.rules);
    CHECK_EQ(g.tuning.gravity, 30.0f);
}

TG_TEST(each_load_error_has_a_bundle_that_causes_it) {
    GameData g;
    const Bytes good = caveHopBundle();

    CHECK(loadBundle(nullptr, 0, g) == LoadError::TooSmall);
    CHECK(loadBundle(good.data(), 15, g) == LoadError::TooSmall);

    Bytes b = good;
    b[0] = 'X';
    CHECK(load(b, g) == LoadError::BadMagic);

    b = good;
    b[4] = 2;
    CHECK(load(b, g) == LoadError::WrongApi);

    b = good;
    b[8] ^= 1;  // total size no longer matches
    CHECK(load(b, g) == LoadError::BadSize);
    b = good;
    b.push_back(0);
    CHECK(load(b, g) == LoadError::BadSize);

    b = good;
    b[b.size() - 1] ^= 0x40;  // body changed, checksum not
    CHECK(load(b, g) == LoadError::BadChecksum);
    b = good;
    b[12] ^= 1;
    CHECK(load(b, g) == LoadError::BadChecksum);

    for (const char* tag : {"META", "PALT", "SPRT", "TILE", "KIND", "TUNE", "LEVL"}) {
        BuildOptions opt;
        opt.omit = {tag};
        CHECK(load(buildBundle(caveHopSpec(), opt), g) == LoadError::MissingSection);
    }
    {
        BundleSpec s = caveHopSpec();  // RULE is optional
        BuildOptions opt;
        opt.omit = {"RULE"};
        CHECK(load(buildBundle(s, opt), g) == LoadError::None);
    }

    // Malformed: each of these has a good checksum but a bad value.
    CHECK(loadWithEdit("META", 8 + 1 + 0 /*id len*/ + 0, 0xFF) == LoadError::Malformed);  // a string runs on
    {
        BundleSpec s = caveHopSpec();
        s.name = std::string("A\x01B");  // non-printable
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.view = 2;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.playfield_cols = 10;  // one side only
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.tiles[0].sprite = 99;  // sprite out of range
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.tiles[0].sprite = 7;  // the 16x16 hero is not an 8x8 tile sprite
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.tiles[0].flags = 8;  // unknown flag
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.kinds[0].sprite = 50;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.kinds[1].behaviour = 3;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.kinds[1].movement = 2;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.kinds[1].touch = 2;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.kinds[1].speed = 1.0f / 0.0f;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.levels[0].cells[0] = 5;  // only 4 tiles
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.levels[0].spawns[0].kind = 9;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.levels[0].spawns[0].col = 40;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.levels[0].spawns[0].row = 10;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.levels.clear();
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.sprites[0].w = 0;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
        s = caveHopSpec();
        s.tuning[0].second = 0.0f / 0.0f;
        CHECK(load(buildBundle(s), g) == LoadError::Malformed);
    }
    {
        // A section that claims to be bigger than the bundle.
        b = good;
        b[16 + 8] = 0xFF;
        b[16 + 9] = 0xFF;
        fixHeader(b);
        CHECK(load(b, g) == LoadError::Malformed);
        // A section that starts inside the section table.
        b = good;
        b[16 + 4] = 4;
        b[16 + 5] = 0;
        fixHeader(b);
        CHECK(load(b, g) == LoadError::Malformed);
        // More sections than the bundle can hold.
        b = good;
        b[6] = 0xFF;
        b[7] = 0xFF;
        fixHeader(b);
        CHECK(load(b, g) == LoadError::Malformed);
        // The same section twice.
        b = good;
        std::copy(b.begin() + 16, b.begin() + 20, b.begin() + 16 + 12);
        fixHeader(b);
        CHECK(load(b, g) == LoadError::Malformed);
        // Extra bytes after a section's records.
        BundleSpec s = caveHopSpec();
        Bytes t = buildBundle(s);
        std::size_t off = 0, size = 0;
        CHECK(findSection(t, "TUNE", off, size));
        t[off] = 2;  // the count says two entries but the section holds three
        fixHeader(t);
        CHECK(load(t, g) == LoadError::Malformed);
    }
    CHECK(g.levels.empty() && g.sprites.empty());  // a failed load leaves nothing behind
}

TG_TEST(documented_limits_are_enforced) {
    GameData g;
    BundleSpec s = caveHopSpec();
    s.sprites[0].w = 33;
    s.sprites[0].h = 8;
    s.sprites[0].pixels.assign(33 * 8, 1);
    s.tiles[0].sprite = 1;
    CHECK(load(buildBundle(s), g) == LoadError::TooLarge);

    s = caveHopSpec();
    s.sprites.resize(256, s.sprites[0]);
    CHECK(load(buildBundle(s), g) == LoadError::TooLarge);
    s.sprites.resize(255);
    CHECK(load(buildBundle(s), g) == LoadError::None);

    s = caveHopSpec();
    s.kinds.resize(65, s.kinds[2]);
    CHECK(load(buildBundle(s), g) == LoadError::TooLarge);
    s.kinds.resize(64);
    CHECK(load(buildBundle(s), g) == LoadError::None);

    s = caveHopSpec();
    s.levels.resize(33, s.levels[0]);
    CHECK(load(buildBundle(s), g) == LoadError::TooLarge);
    s.levels.resize(32);
    CHECK(load(buildBundle(s), g) == LoadError::None);

    s = caveHopSpec();
    s.levels[0].cols = 128;
    s.levels[0].rows = 129;  // 16,512 cells
    CHECK(load(buildBundle(s), g) == LoadError::TooLarge);
    s.levels[0].rows = 128;  // exactly 16,384 cells
    s.levels[0].spawns.clear();
    CHECK(load(buildBundle(s), g) == LoadError::None);

    s = caveHopSpec();
    s.levels[0].spawns.assign(257, SpawnSpec{});
    CHECK(load(buildBundle(s), g) == LoadError::TooLarge);
    s.levels[0].spawns.resize(256);
    CHECK(load(buildBundle(s), g) == LoadError::None);

    s = caveHopSpec();  // whole bundle over 262,144 bytes
    s.has_rules = true;
    s.rules.assign(262144, 'x');
    CHECK(load(buildBundle(s), g) == LoadError::TooLarge);
}

TG_TEST(every_truncation_is_rejected_safely) {
    const Bytes good = caveHopBundle();
    GameData g;
    CHECK(load(good, g) == LoadError::None);
    for (std::size_t length = 0; length < good.size(); ++length) {
        // An exact-size copy, so a read past the end is seen by the sanitizers
        // and by the guard bytes the allocator puts there.
        Bytes cut(good.begin(), good.begin() + static_cast<long>(length));
        cut.shrink_to_fit();
        GameData out;
        CHECK(loadBundle(cut.data(), cut.size(), out) != LoadError::None);
        CHECK(out.levels.empty());
        // The same cut with a repaired header must still be safe.
        if (length >= 16) {
            fixHeader(cut);
            GameData out2;
            CHECK(loadBundle(cut.data(), cut.size(), out2) != LoadError::None);
        }
    }
}

TG_TEST(every_corrupted_byte_is_safe) {
    const Bytes good = caveHopBundle();
    for (std::size_t i = 0; i < good.size(); ++i) {
        for (std::uint8_t value : {std::uint8_t(0), std::uint8_t(0xFF), std::uint8_t(good[i] ^ 0x5A)}) {
            Bytes b = good;
            b[i] = value;
            fixHeader(b);
            GameData g;
            const LoadError e = load(b, g);
            if (e != LoadError::None) CHECK(g.levels.empty() && g.kinds.empty());
        }
    }
}

TG_TEST(error_texts_exist_for_every_error) {
    for (LoadError e : {LoadError::TooSmall, LoadError::BadMagic, LoadError::WrongApi, LoadError::BadSize,
                        LoadError::BadChecksum, LoadError::MissingSection, LoadError::Malformed,
                        LoadError::TooLarge}) {
        CHECK(loadErrorText(e)[0] != '\0');
    }
    CHECK_EQ(std::string(loadErrorText(LoadError::None)), std::string(""));
}

TG_TEST(sprite_drawing_scales_flips_and_clips) {
    SpriteData s;
    s.width = 2;
    s.height = 1;
    s.pixels = {1, 0};
    CHECK(gfx::spriteValid(s));
    gfx::Palette p{"T", {}, 0, 0};
    p.color[1] = 0xF800;
    std::vector<gfx::Color> mem(8 * 4, 0x1234);
    gfx::Canvas canvas({mem.data(), 8, 4, 8});
    gfx::DrawOptions o;
    o.scale = 3;
    gfx::drawSprite(canvas, s, p, 1, 0, o);
    CHECK_EQ(canvas.at(1, 0), gfx::Color{0xF800});
    CHECK_EQ(canvas.at(3, 2), gfx::Color{0xF800});
    CHECK_EQ(canvas.at(4, 0), gfx::Color{0x1234});
    o.flip_x = true;
    canvas.fill(0x1234);
    gfx::drawSprite(canvas, s, p, 1, 0, o);
    CHECK_EQ(canvas.at(1, 0), gfx::Color{0x1234});
    CHECK_EQ(canvas.at(4, 0), gfx::Color{0xF800});
    gfx::drawSprite(canvas, s, p, -50, -50, o);
    gfx::drawSprite(canvas, s, p, 7, 3, o);  // hangs off the corner
    s.pixels = {1};
    CHECK(!gfx::spriteValid(s));
    gfx::drawSprite(canvas, s, p, 0, 0, o);  // invalid: draws nothing, no crash
}

TG_TEST(tile_cache_from_loaded_tiles_matches_the_art_form) {
    GameData g;
    CHECK(load(caveHopBundle(), g) == LoadError::None);
    TileCache cache;
    CHECK(cache.build(g.sprites, g.tiles, g.palette, 2, true));
    CHECK_EQ(cache.count(), 4);
    CHECK_EQ(cache.block(), 16);
    CHECK(cache.mask(0) == nullptr);  // the ground has no see-through pixels
    CHECK(cache.mask(2) != nullptr);  // the ledge does
    // Same pixels as drawing the sprite directly.
    std::vector<gfx::Color> mem(16 * 16, 0);
    gfx::Canvas canvas({mem.data(), 16, 16, 16});
    gfx::DrawOptions o;
    o.bevel = true;
    gfx::drawSprite(canvas, g.sprites[0], g.palette, 0, 0, o);
    for (int i = 0; i < 256; ++i) CHECK_EQ(cache.pixels(0)[i], mem[static_cast<std::size_t>(i)]);

    TileCache empty;
    CHECK(empty.build(g.sprites, {}, g.palette, 2, true));
    CHECK_EQ(empty.count(), 0);
    std::vector<TileData> bad = {{99, 0}};
    CHECK(!empty.build(g.sprites, bad, g.palette, 2, true));
    bad = {{0, 0}, {7, 0}};  // 8x8 and 16x16 mixed
    CHECK(!empty.build(g.sprites, bad, g.palette, 2, true));
}

int main() { return tg::test::run(); }
