#include <algorithm>

#include "../world/world.h"
#include "engine_helpers.h"
#include "tg_test.h"

using namespace tg;
using namespace tg::test;

namespace {

// Physics on a hand-made grid: tile 1 solid, 2 one-way, 3 hazard.
struct Grid {
    int cols, rows;
    std::vector<std::uint8_t> cells;
    std::uint16_t flags[3] = {TILE_SOLID, TILE_ONE_WAY, TILE_HAZARD};
    Grid(int c, int r) : cols(c), rows(r), cells(static_cast<std::size_t>(c * r), 0) {}
    void set(int col, int row, int tile) { cells[static_cast<std::size_t>(row * cols + col)] = static_cast<std::uint8_t>(tile); }
    TileGrid view() const { return {cols, rows, cells.data(), flags, 3}; }
};

Actor box(float x, float y, float w = 1, float h = 1) {
    Actor a;
    a.alive = true;
    a.x = x; a.y = y; a.w = w; a.h = h;
    return a;
}

}  // namespace

TG_TEST(pool_holds_64_and_gives_new_serials_to_reused_slots) {
    ActorPool pool;
    int first = -1;
    std::uint32_t first_serial = 0;
    for (int i = 0; i < MAX_ACTORS; ++i) {
        const int index = pool.spawn(0, 0, 0, 1, 1);
        CHECK_EQ(index, i);
        if (i == 0) { first = index; first_serial = pool.at(index).serial; }
        CHECK(pool.at(index).serial != 0);
    }
    CHECK_EQ(pool.count(), 64);
    CHECK_EQ(pool.spawn(0, 0, 0, 1, 1), -1);
    CHECK(pool.alive(first, first_serial));
    pool.remove(first);
    pool.remove(first);   // twice is harmless
    pool.remove(-1);
    pool.remove(64);
    CHECK_EQ(pool.count(), 63);
    CHECK(!pool.alive(first, first_serial));
    const int again = pool.spawn(1, 0, 0, 1, 1);
    CHECK_EQ(again, first);
    CHECK(pool.at(again).serial != first_serial);
    CHECK(!pool.alive(first, first_serial));
    CHECK(pool.alive(again, pool.at(again).serial));
    CHECK(!pool.alive(70, 1));
}

TG_TEST(gravity_pulls_down_and_landing_stops_and_flags_ground) {
    Grid g(10, 10);
    for (int c = 0; c < 10; ++c) g.set(c, 9, 1);
    Actor a = box(3, 2);
    stepPlatformer(a, g.view(), 30);
    CHECK(a.vy > 0);
    CHECK(!a.onGround());
    float last_y = a.y;
    for (int i = 0; i < 200 && !a.onGround(); ++i) {
        stepPlatformer(a, g.view(), 30);
        CHECK(a.y >= last_y);
        last_y = a.y;
    }
    CHECK(a.onGround());
    CHECK_EQ(a.y, 8.0f);
    CHECK_EQ(a.vy, 0.0f);
    for (int i = 0; i < 10; ++i) {  // stays put, and stays "on ground" every tick
        stepPlatformer(a, g.view(), 30);
        CHECK(a.onGround());
        CHECK_EQ(a.y, 8.0f);
    }
}

TG_TEST(fall_speed_is_capped_at_30_blocks_per_second) {
    Grid g(4, 100);
    Actor a = box(1, 0);
    for (int i = 0; i < 300; ++i) stepPlatformer(a, g.view(), 30);
    CHECK_EQ(a.vy, 30.0f);
    float before = a.y;
    stepPlatformer(a, g.view(), 30);
    CHECK(a.y - before <= 0.5f + 1e-5f);
    // Never more than half a block per tick, whatever the speed asked for.
    a.vx = 1000;
    before = a.x;
    stepPlatformer(a, g.view(), 30);
    CHECK(a.x - before <= 0.5f + 1e-5f);
}

TG_TEST(walls_stop_sideways_movement_and_set_hit_wall) {
    Grid g(10, 10);
    for (int c = 0; c < 10; ++c) g.set(c, 9, 1);
    g.set(6, 8, 1);
    Actor a = box(2, 8);
    for (int i = 0; i < 100; ++i) {
        a.vx = 6;
        stepPlatformer(a, g.view(), 30);
    }
    CHECK_EQ(a.x, 5.0f);
    CHECK_EQ(a.vx, 0.0f);
    CHECK(a.hitWall());
    CHECK(a.onGround());
    a.vx = -6;
    stepPlatformer(a, g.view(), 30);
    CHECK(!a.hitWall());
    // The level's left, right and top edges are walls; the bottom is open.
    for (int i = 0; i < 200; ++i) { a.vx = -6; stepPlatformer(a, g.view(), 30); }
    CHECK_EQ(a.x, 0.0f);
    Actor b = box(8, 8);
    for (int i = 0; i < 100; ++i) { b.vx = 6; stepPlatformer(b, g.view(), 30); }
    CHECK_EQ(b.x, 9.0f);
    Actor c = box(4, 3);
    for (int i = 0; i < 100; ++i) { c.vy = -30; stepPlatformer(c, g.view(), 0); }
    CHECK_EQ(c.y, 0.0f);
    CHECK_EQ(c.vy, 0.0f);
}

TG_TEST(ceilings_stop_upward_movement) {
    Grid g(10, 10);
    for (int c = 0; c < 10; ++c) g.set(c, 9, 1);
    g.set(4, 5, 1);
    Actor a = box(4, 8);
    a.vy = -20;
    for (int i = 0; i < 30; ++i) stepPlatformer(a, g.view(), 30);
    CHECK(a.y >= 6.0f);
    Actor b = box(4, 7);
    b.vy = -20;
    stepPlatformer(b, g.view(), 30);
    for (int i = 0; i < 3; ++i) { b.vy = -20; stepPlatformer(b, g.view(), 0); }
    CHECK_EQ(b.y, 6.0f);
    CHECK_EQ(b.vy, 0.0f);
}

TG_TEST(one_way_tiles_block_only_from_above) {
    Grid g(10, 10);
    for (int c = 0; c < 10; ++c) g.set(c, 9, 1);
    g.set(4, 5, 2);
    // From below: rises straight through.
    Actor up = box(4, 8);
    up.vy = -17;
    float top = up.y;
    for (int i = 0; i < 90; ++i) { stepPlatformer(up, g.view(), 30); top = std::min(top, up.y); }
    CHECK(top < 5.0f);
    // ...and lands on it coming down.
    CHECK_EQ(up.y, 4.0f);
    CHECK(up.onGround());
    // From the side it is not a wall.
    Actor side = box(2, 5);
    side.vy = 0;
    for (int i = 0; i < 40; ++i) { side.vx = 6; stepPlatformer(side, g.view(), 0); }
    CHECK(side.x > 5.0f);
    // An actor already below its top is not caught while falling past it.
    Actor late = box(4, 4.5f);
    stepPlatformer(late, g.view(), 30);
    CHECK(!late.onGround());
}

TG_TEST(hazards_do_not_block_but_are_found_by_overlap) {
    Grid g(10, 10);
    for (int c = 0; c < 10; ++c) g.set(c, 9, 1);
    g.set(5, 8, 3);
    Actor a = box(3, 8);
    for (int i = 0; i < 100; ++i) { a.vx = 6; stepPlatformer(a, g.view(), 30); }
    CHECK(a.x > 6.0f);  // walked straight through
    CHECK(overlapsTileFlag(g.view(), box(4.5f, 8), TILE_HAZARD));
    CHECK(!overlapsTileFlag(g.view(), box(4.0f, 8), TILE_HAZARD));   // touching is not overlapping
    CHECK(!overlapsTileFlag(g.view(), box(6.0f, 8), TILE_HAZARD));
    CHECK(!overlapsTileFlag(g.view(), box(5.0f, 7), TILE_HAZARD));
    CHECK(overlapsTileFlag(g.view(), box(5.0f, 7, 1, 1.5f), TILE_HAZARD));
}

TG_TEST(support_overlap_and_fall_out_helpers) {
    Grid g(10, 10);
    g.set(3, 5, 1);
    g.set(5, 5, 2);
    CHECK(isSupported(g.view(), box(3, 4)));
    CHECK(isSupported(g.view(), box(5, 4)));
    CHECK(isSupported(g.view(), box(2.5f, 4)));
    CHECK(!isSupported(g.view(), box(3, 3.5f)));
    CHECK(!isSupported(g.view(), box(1, 4)));
    CHECK(actorsOverlap(box(0, 0), box(0.5f, 0.5f)));
    CHECK(!actorsOverlap(box(0, 0), box(1, 0)));
    CHECK(!hasFallenOut(g.view(), box(2, 12.0f)));
    CHECK(hasFallenOut(g.view(), box(2, 12.01f)));
    // Out of the level: solid on the left, right and top; open below.
    CHECK(g.view().flagsAt(-1, 3) & TILE_SOLID);
    CHECK(g.view().flagsAt(10, 3) & TILE_SOLID);
    CHECK(g.view().flagsAt(4, -1) & TILE_SOLID);
    CHECK_EQ(int(g.view().flagsAt(4, 10)), 0);
}

TG_TEST(walker_turns_at_walls) {
    Arena arena(arenaRows({{8, "...B.....s...B"}}));
    CHECK(arena.ok);
    float lo = 99, hi = -99;
    for (int i = 0; i < 600; ++i) {
        arena.run(1);
        ActorProbe a;
        CHECK(arena.engine.actor(0, a));
        CHECK(std::string(a.kind) == "slime");
        lo = std::min(lo, a.x);
        hi = std::max(hi, a.x);
        CHECK(a.on_ground);
    }
    CHECK(lo >= 4.0f && lo < 4.05f);    // turned at the brick in column 3
    CHECK(hi <= 12.0f && hi > 11.95f);  // and at the brick in column 13
}

TG_TEST(walker_turns_at_ledges_and_does_not_fall) {
    // Ground only in columns 3..12; the slime starts in column 5.
    Arena arena(arenaRows({{8, ".....s"}, {9, "...##########"}}));
    CHECK(arena.ok);
    float lo = 99, hi = -99;
    for (int i = 0; i < 900; ++i) {
        arena.run(1);
        ActorProbe a;
        CHECK(arena.engine.actor(0, a));
        lo = std::min(lo, a.x);
        hi = std::max(hi, a.x);
        CHECK_EQ(a.y, 8.0f);
    }
    CHECK(lo > 2.9f && lo <= 3.0f + 1e-3f);
    CHECK(hi >= 12.0f - 1e-3f && hi < 12.1f);
}

TG_TEST(walker_starts_facing_left) {
    Arena arena(arenaRows({{8, ".....s"}}));
    arena.run(1);
    ActorProbe a;
    CHECK(arena.engine.actor(0, a));
    CHECK(a.vx < 0);
    CHECK_EQ(a.vx, -2.0f);
}

TG_TEST(pickup_adds_points_and_is_removed) {
    Arena arena(arenaRows({{8, "..P...o"}}));
    CHECK_EQ(arena.engine.actorCount(), 2);
    arena.run(100, tilt(1));
    CHECK_EQ(arena.engine.score(), 10);
    CHECK_EQ(arena.engine.actorCount(), 1);
    arena.run(100, tilt(-1));
    CHECK_EQ(arena.engine.score(), 10);  // gone for good
}

TG_TEST(slime_touch_hurts_and_respawns_the_player) {
    Arena arena(arenaRows({{8, "..P.....s"}}));
    CHECK_EQ(arena.engine.hurts(), 0);
    bool hurt = false;
    for (int i = 0; i < 200 && !hurt; ++i) {
        arena.run(1, tilt(1));
        hurt = arena.engine.hurts() > 0;
    }
    CHECK(hurt);
    CHECK_EQ(arena.engine.hurts(), 1);
    const ActorProbe p = arena.player();
    CHECK_EQ(p.x, 2.0f);
    CHECK_EQ(p.y, 7.0f);
    CHECK_EQ(p.vx, 0.0f);
    CHECK_EQ(p.vy, 0.0f);
    CHECK(p.on_ground);
}

TG_TEST(hazard_tile_hurts_and_respawns_the_player) {
    Arena arena(arenaRows({{8, "..P.....^"}}));
    arena.run(200, tilt(1));
    CHECK(arena.engine.hurts() >= 1);
}

TG_TEST(hazard_tile_counts_exactly_once_per_touch) {
    Arena arena(arenaRows({{8, "..P....^"}}));
    int last = 0;
    for (int i = 0; i < 100; ++i) {
        arena.run(1, tilt(1));
        const int h = arena.engine.hurts();
        CHECK(h - last <= 1);
        last = h;
        if (h == 1) break;
    }
    CHECK_EQ(arena.engine.hurts(), 1);
    CHECK_EQ(arena.player().x, 2.0f);
}

TG_TEST(player_falling_out_of_the_level_respawns) {
    // No ground from column 6 on.
    Arena arena(arenaRows({{8, "..P"}, {9, "######"}}));
    int hurts_at = -1;
    for (int i = 0; i < 400; ++i) {
        arena.run(1, tilt(1));
        if (arena.engine.hurts() > 0) { hurts_at = i; break; }
    }
    CHECK(hurts_at > 0);
    CHECK_EQ(arena.engine.hurts(), 1);
    const ActorProbe p = arena.player();
    CHECK_EQ(p.x, 2.0f);
    CHECK_EQ(p.y, 7.0f);
}

TG_TEST(other_actors_that_fall_out_are_removed) {
    Arena arena(arenaRows({{2, "..s"}, {8, "............P"}, {9, "..........##########"}}));
    CHECK_EQ(arena.engine.actorCount(), 2);
    arena.run(300);
    CHECK_EQ(arena.engine.actorCount(), 1);
    CHECK_EQ(arena.engine.hurts(), 0);
}

TG_TEST(player_faces_the_last_non_zero_run_direction) {
    Bytes b = arenaBundle(arenaRows({{8, "..P"}}));
    GameData g;
    CHECK(loadBundle(b.data(), b.size(), g) == LoadError::None);
    World w;
    CHECK(w.start(g, 0));
    const int p = w.playerIndex();
    CHECK(p >= 0);
    CHECK_EQ(w.pool().at(p).facing, 1);
    w.tick(tilt(-1));
    CHECK_EQ(w.pool().at(p).facing, -1);
    w.tick(tilt(0));
    CHECK_EQ(w.pool().at(p).facing, -1);  // standing still keeps it
    w.tick(tilt(1));
    CHECK_EQ(w.pool().at(p).facing, 1);
}

TG_TEST(world_handles_see_removed_and_reused_slots) {
    Bytes b = arenaBundle(arenaRows({{8, "..P...o"}}));
    GameData g;
    CHECK(loadBundle(b.data(), b.size(), g) == LoadError::None);
    World w;
    CHECK(w.start(g, 0));
    int index = -1;
    std::uint32_t serial = 0;
    CHECK(w.find("coin", index, serial));
    CHECK(w.alive(index, serial));
    CHECK(std::string(w.kindName(w.pool().at(index))) == "coin");
    CHECK(!w.find("gem", index, serial));
    CHECK(w.find("coin", index, serial));
    Actor* coin = w.actor(index, serial);
    CHECK(coin != nullptr);
    World::set(*coin, &Actor::x, 9.0f);
    World::set(*coin, &Actor::y, 0.0f / 0.0f);  // ignored: not a number
    CHECK_EQ(coin->x, 9.0f);
    CHECK_EQ(coin->y, 8.0f);
    w.remove(index, serial);
    CHECK(!w.alive(index, serial));
    CHECK(w.actor(index, serial) == nullptr);
    w.addScore(5);
    CHECK_EQ(w.score(), 5);
}

int main() { return tg::test::run(); }
