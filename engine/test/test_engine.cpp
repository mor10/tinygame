#include <algorithm>
#include <cmath>
#include <cstring>

#include "../core/hash.h"
#include "../core/random.h"
#include "engine_helpers.h"
#include "game_cases.h"
#include "golden_game.h"
#include "tg_test.h"

using namespace tg;
using namespace tg::test;

namespace {

// Presses at call `press_at`, runs `ticks` calls, and returns how far up (in
// blocks) the player got, measured from where it stood.
float climb(Arena& a, int press_at, int release_at, int ticks) {
    const float start = a.player().y;
    float top = start;
    for (int i = 0; i < ticks; ++i) {
        Input in;
        if (i == press_at) in = press();
        if (i == release_at) in.released = true;
        a.engine.tick(in);
        top = std::min(top, a.player().y);
    }
    return start - top;
}

std::vector<std::string> openRoom() { return arenaRows({{8, "..P"}}); }

}  // namespace

TG_TEST(engine_loads_the_cave_hop_bundle_and_reports_state) {
    Engine e;
    CHECK_EQ(std::string(e.error()), std::string(""));
    const Bytes b = caveHopBundle();
    CHECK(e.load(b.data(), b.size(), 320, 172));
    CHECK_EQ(std::string(e.error()), std::string(""));
    CHECK_EQ(e.ticks(), 0u);
    CHECK_EQ(e.score(), 0);
    CHECK_EQ(e.hurts(), 0);
    CHECK_EQ(e.actorCount(), 10);  // 7 coins, the gem, the slime and the player
    ActorProbe p;
    CHECK(e.player(p));
    CHECK(std::string(p.kind) == "player");
    CHECK_EQ(p.x, 2.0f);
    CHECK_EQ(p.y, 7.0f);  // a 2 x 2 hero stands with its feet on the ground
    CHECK(p.on_ground);
    e.tick(Input{});
    CHECK_EQ(e.ticks(), 1u);
}

TG_TEST(engine_load_failures_give_words_and_leave_a_quiet_engine) {
    Engine e;
    Bytes b = caveHopBundle();
    b[b.size() / 2] ^= 0xFF;
    CHECK(!e.load(b.data(), b.size(), 320, 172));
    CHECK(std::strlen(e.error()) > 0);
    e.tick(Input{});
    CHECK_EQ(e.ticks(), 0u);
    ActorProbe p;
    CHECK(!e.player(p));
    CHECK_EQ(e.actorCount(), 0);
    char text[64];
    CHECK(e.probeText(text, sizeof(text)) > 0);
    GuardedScreen s(320, 172);
    gfx::Canvas canvas(s.surface());
    e.draw(canvas);
    CHECK(s.intact());
    // A good bundle after a bad one works.
    const Bytes good = caveHopBundle();
    CHECK(e.load(good.data(), good.size(), 320, 172));
    CHECK_EQ(std::string(e.error()), std::string(""));
    CHECK(!e.load(good.data(), good.size(), 0, 0));
}

TG_TEST(gravity_and_landing) {
    Arena a(arenaRows({{2, "..P"}}));
    CHECK(a.ok);
    CHECK(!a.player().on_ground);
    a.run(1);
    CHECK(a.player().vy > 0);
    int landed = -1;
    for (int i = 1; i < 200 && landed < 0; ++i) {
        a.run(1);
        if (a.player().on_ground) landed = i;
    }
    CHECK(landed > 10);
    CHECK_EQ(a.player().y, 7.0f);
    CHECK_EQ(a.player().vy, 0.0f);
    // Free fall follows y = g t^2 / 2 (30 blocks/s^2): after 0.5 s about 3.75 blocks.
    Arena f(arenaRows({{3, "..P"}}));
    const float y0 = f.player().y;
    f.run(30);
    CHECK(std::fabs((f.player().y - y0) - 3.75f) < 0.05f);
}

TG_TEST(jump_reaches_the_tuned_height) {
    Arena a(openRoom());
    CHECK(a.player().on_ground);
    const float height = climb(a, 0, -1, 80);
    CHECK(height > 3.0f - 0.1f);
    CHECK(height < 3.0f + 0.1f);
    CHECK(a.player().on_ground);
    CHECK_EQ(a.player().y, 7.0f);
}

TG_TEST(releasing_early_makes_a_lower_jump) {
    Arena full(openRoom());
    Arena early(openRoom());
    const float h_full = climb(full, 0, -1, 80);
    const float h_early = climb(early, 0, 4, 80);
    CHECK(h_early < h_full - 0.5f);
    CHECK(h_early > 0.3f);
    // Letting go on the ground, or after the top, changes nothing.
    Arena late(openRoom());
    const float h_late = climb(late, 0, 60, 80);
    CHECK(std::fabs(h_late - h_full) < 1e-4f);
    // Only once per jump: a second release while rising does not halve again.
    Arena twice(openRoom());
    Input in;
    float top = 0;
    const float start = twice.player().y;
    for (int i = 0; i < 80; ++i) {
        in = Input{};
        if (i == 0) in = press();
        if (i == 3 || i == 4) in.released = true;
        twice.engine.tick(in);
        top = std::max(top, start - twice.player().y);
    }
    CHECK(std::fabs(top - h_early) < 0.35f);
}

TG_TEST(a_press_is_remembered_for_6_ticks_before_landing) {
    // The player drops from the air; find which call it lands on.
    Arena probe(arenaRows({{3, "..P"}}));
    int land = -1;
    for (int i = 0; i < 100 && land < 0; ++i) {
        probe.run(1);
        if (probe.player().on_ground) land = i;
    }
    CHECK(land > 10);
    // A press on call land - 5 is the 6th tick before the first call that
    // starts on the ground: it still jumps. One tick earlier it is forgotten.
    for (int back = 0; back <= 9; ++back) {
        Arena a(arenaRows({{3, "..P"}}));
        float top = 99;
        for (int i = 0; i < land + 40; ++i) {
            a.engine.tick(i == land - back ? press() : Input{});
            if (i > land) top = std::min(top, a.player().y);  // only after landing
        }
        const bool jumped = top < 6.5f;  // standing is y = 7
        if (back <= 5) CHECK(jumped);
        else CHECK(!jumped);
    }
}

TG_TEST(a_jump_is_still_possible_for_6_ticks_after_leaving_the_ground) {
    // Ground for columns 0..9 only; run right off the edge.
    auto rows = arenaRows({{8, ".P"}, {9, "##########"}});
    Arena probe(rows);
    int left = -1;
    for (int i = 0; i < 200 && left < 0; ++i) {
        probe.run(1, tilt(1));
        if (!probe.player().on_ground) left = i;
    }
    CHECK(left > 0);
    for (int later = 1; later <= 9; ++later) {
        Arena a(rows);
        a.run(left + 1, tilt(1));
        CHECK(!a.player().on_ground);
        Input in = press();
        in.tilt_x = 1;
        // `later` calls after the one that left the ground
        for (int k = 1; k < later; ++k) a.run(1, tilt(1));
        a.engine.tick(in);
        const bool jumped = a.player().vy < 0;
        if (later <= 6) CHECK(jumped);
        else CHECK(!jumped);
    }
}

TG_TEST(there_is_no_second_jump_in_the_air) {
    Arena a(openRoom());
    a.engine.tick(press());
    a.run(20);
    CHECK(a.player().vy < 0 || a.player().y < 7.0f);
    const float vy = a.player().vy;
    a.engine.tick(press());
    CHECK(a.player().vy > vy);  // only gravity acted
}

TG_TEST(walls_stop_the_player) {
    Arena a(arenaRows({{7, "..........B"}, {8, "..P.......B"}}));
    a.run(200, tilt(1));
    CHECK_EQ(a.player().x, 8.0f);
    CHECK(a.player().on_ground);
}

TG_TEST(a_one_way_tile_can_be_jumped_through_and_stood_on) {
    // A 4-block jump (the feet rise 4 blocks, above the ledge's top at row 6).
    BundleSpec spec = arenaSpec(arenaRows({{6, "-----"}, {8, "..P"}}));
    spec.tuning = {{"gravity", 30}, {"jump_height", 4}, {"run_speed", 6}};
    Arena a(spec);
    float top = 99;
    for (int i = 0; i < 100; ++i) {
        a.engine.tick(i == 0 ? press() : Input{});
        top = std::min(top, a.player().y);
    }
    CHECK(top < 4.0f);  // the body went right through the ledge
    CHECK(a.player().on_ground);
    CHECK_EQ(a.player().y, 4.0f);  // and the feet came down on top of it, y = 6 - 2
    // From beside it, the ledge is not a wall either.
    Arena side(spec);
    side.run(120, tilt(1));
    CHECK(side.player().x > 6.0f);
}

TG_TEST(knob_run_direction_is_held_for_12_ticks_and_beats_tilt) {
    Arena a(arenaRows({{8, "..........P"}}));
    Input turn = tilt(-1);
    turn.main.delta = 3;
    a.engine.tick(turn);
    CHECK_EQ(a.player().vx, 6.0f);
    for (int i = 1; i <= 12; ++i) {  // the next 12 ticks keep the knob's direction
        a.engine.tick(tilt(-1));
        CHECK_EQ(a.player().vx, 6.0f);
    }
    a.engine.tick(tilt(-1));  // then tilt takes over
    CHECK_EQ(a.player().vx, -6.0f);
    // A turn the other way replaces it at once and restarts the hold.
    Input back = tilt(1);
    back.main.delta = -1;
    a.engine.tick(back);
    CHECK_EQ(a.player().vx, -6.0f);
    for (int i = 0; i < 12; ++i) a.engine.tick(tilt(1));
    CHECK_EQ(a.player().vx, -6.0f);
    a.engine.tick(tilt(1));
    CHECK_EQ(a.player().vx, 6.0f);
}

TG_TEST(tilt_has_a_dead_zone_and_reaches_full_speed_at_0_6) {
    Arena a(arenaRows({{8, "..........P"}}));
    a.engine.tick(tilt(0.14f));
    CHECK_EQ(a.player().vx, 0.0f);
    a.engine.tick(tilt(-0.14f));
    CHECK_EQ(a.player().vx, 0.0f);
    a.engine.tick(tilt(0.3f));
    CHECK(a.player().vx > 0.0f && a.player().vx < 6.0f);
    a.engine.tick(tilt(0.6f));
    CHECK_EQ(a.player().vx, 6.0f);
    a.engine.tick(tilt(1.0f));
    CHECK_EQ(a.player().vx, 6.0f);
    a.engine.tick(tilt(-0.9f));
    CHECK_EQ(a.player().vx, -6.0f);
    a.engine.tick(tilt(0.0f));
    CHECK_EQ(a.player().vx, 0.0f);
}

TG_TEST(the_gem_does_nothing_until_lua_rules_exist) {
    // Lua is not connected to the engine yet: touching the gem changes nothing.
    Arena a(arenaRows({{8, "..P.g"}}));
    a.run(100, tilt(1));
    CHECK_EQ(a.engine.score(), 0);
    CHECK_EQ(a.engine.actorCount(), 2);
}

TG_TEST(probe_text_has_the_documented_lines) {
    Engine e;
    const Bytes b = caveHopBundle();
    CHECK(e.load(b.data(), b.size(), 320, 172));
    char text[2048];
    const std::size_t n = e.probeText(text, sizeof(text));
    CHECK_EQ(n, std::strlen(text));
    const std::string s = text;
    CHECK(s.rfind("tick 0\nscore 0\nhurts 0\n", 0) == 0);
    CHECK(s.find("actor 5 player x=2.00 y=7.00 vx=0.00 vy=0.00 ground=1\n") != std::string::npos);
    CHECK(s.find("actor 6 slime x=16.00 y=8.00 vx=0.00 vy=0.00 ground=1\n") != std::string::npos);
    CHECK(s.find("actor 0 coin x=8.00 y=6.00 vx=0.00 vy=0.00 ground=0\n") != std::string::npos);
    CHECK_EQ(std::count(s.begin(), s.end(), '\n'), 3 + 10);
    CHECK_EQ(s.back(), '\n');
    e.tick(Input{});
    e.probeText(text, sizeof(text));
    CHECK(std::string(text).rfind("tick 1\n", 0) == 0);
    CHECK(std::string(text).find("actor 6 slime x=15.97 y=8.00 vx=-2.00 vy=0.00 ground=1\n") != std::string::npos);
    // Small buffers: always terminated, only whole lines, never past the end.
    for (std::size_t cap = 0; cap < 140; ++cap) {
        char small[160];
        std::memset(small, 'Z', sizeof(small));
        const std::size_t w = e.probeText(small, cap);
        if (cap == 0) { CHECK_EQ(w, 0u); CHECK_EQ(small[0], 'Z'); continue; }
        CHECK(w < cap);
        CHECK_EQ(std::strlen(small), w);
        CHECK(w == 0 || small[w - 1] == '\n');
        CHECK_EQ(small[cap < sizeof(small) ? cap : 0 + 0], cap < sizeof(small) ? 'Z' : small[0]);
    }
    CHECK_EQ(e.probeText(nullptr, 10), 0u);
    // Negative zero is never printed.
    Arena a(arenaRows({{8, "..P"}}));
    char t2[1024];
    a.run(5, tilt(-0.001f));
    a.engine.probeText(t2, sizeof(t2));
    CHECK(std::string(t2).find("-0.00") == std::string::npos);
}

TG_TEST(actor_probes_list_living_actors_in_pool_order) {
    Arena a(arenaRows({{8, "..P..o.o"}}));
    CHECK_EQ(a.engine.actorCount(), 3);
    ActorProbe p;
    CHECK(a.engine.actor(0, p));
    CHECK_EQ(p.index, 0);
    CHECK(std::string(p.kind) == "player");
    CHECK(a.engine.actor(2, p));
    CHECK_EQ(p.index, 2);
    CHECK(!a.engine.actor(3, p));
    CHECK(!a.engine.actor(-1, p));
    a.run(25, tilt(1));  // takes the first coin
    CHECK_EQ(a.engine.actorCount(), 2);
    CHECK(a.engine.actor(1, p));
    CHECK_EQ(p.index, 2);  // slot 1 is free, so the n-th living actor is in slot 2
}

namespace {

Input scripted(Random& rng) {
    Input in;
    in.tilt_x = static_cast<float>(rng.below(2001)) / 1000.0f - 1.0f;
    if (rng.below(40) == 0) in.main.delta = static_cast<int>(rng.below(7)) - 3;
    if (rng.below(25) == 0) { in.pressed = true; in.down = true; }
    if (rng.below(25) == 0) in.released = true;
    return in;
}

}  // namespace

TG_TEST(two_engines_given_the_same_inputs_stay_identical) {
    const Bytes b = caveHopBundle();
    Engine one, two;
    CHECK(one.load(b.data(), b.size(), 320, 172));
    CHECK(two.load(b.data(), b.size(), 320, 172));
    Random r1(7), r2(7);
    GuardedScreen s1(320, 172), s2(320, 172);
    for (int t = 0; t < 900; ++t) {
        one.tick(scripted(r1));
        two.tick(scripted(r2));
        if (t % 50 == 0) {
            char a[4096], c[4096];
            one.probeText(a, sizeof(a));
            two.probeText(c, sizeof(c));
            CHECK(std::strcmp(a, c) == 0);
            gfx::Canvas c1(s1.surface()), c2(s2.surface());
            one.draw(c1);
            two.draw(c2);
            CHECK(s1.pixels() == s2.pixels());
        }
    }
    CHECK_EQ(one.score(), two.score());
    CHECK_EQ(one.hurts(), two.hurts());
}

TG_TEST(draw_never_writes_outside_the_canvas) {
    const Bytes b = caveHopBundle();
    const int sizes[2][2] = {{320, 172}, {280, 240}};
    for (const auto& size : sizes) {
        Engine e;
        CHECK(e.load(b.data(), b.size(), size[0], size[1]));
        Random rng(3);
        GuardedScreen screen(size[0], size[1]);
        for (int t = 0; t < 700; ++t) {
            e.tick(scripted(rng));
            if (t % 20 == 0) {
                gfx::Canvas canvas(screen.surface());
                e.draw(canvas);
                CHECK(screen.intact());
            }
        }
    }
}

TG_TEST(draw_shows_score_and_covers_the_whole_screen) {
    const Bytes b = caveHopBundle();
    Engine e;
    CHECK(e.load(b.data(), b.size(), 320, 172));
    GuardedScreen s(320, 172);
    gfx::Canvas canvas(s.surface());
    e.draw(canvas);
    const std::vector<gfx::Color> first = s.pixels();
    // The status strip (top 12 rows) has text pixels in the ink colour.
    GameData g;
    CHECK(loadBundle(b.data(), b.size(), g) == LoadError::None);
    int ink = 0;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 320; ++x) ink += first[static_cast<std::size_t>(y * 320 + x)] == g.palette.ink;
    CHECK(ink > 20);
    // Collecting a coin changes the status text.
    Arena a(arenaRows({{8, "..P.o"}}));
    GuardedScreen s2(320, 172);
    gfx::Canvas c2(s2.surface());
    a.engine.draw(c2);
    const auto before = s2.pixels();
    a.run(60, tilt(1));
    CHECK_EQ(a.engine.score(), 10);
    a.engine.draw(c2);
    int differs = 0;
    const auto after = s2.pixels();
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 320; ++x)
            differs += before[static_cast<std::size_t>(y * 320 + x)] != after[static_cast<std::size_t>(y * 320 + x)];
    CHECK(differs > 5);
}

TG_TEST(the_camera_follows_the_player_through_a_wide_level) {
    const Bytes b = caveHopBundle();
    Engine e;
    CHECK(e.load(b.data(), b.size(), 320, 172));
    GuardedScreen s(320, 172);
    gfx::Canvas canvas(s.surface());
    e.draw(canvas);
    const auto start = s.pixels();
    for (int i = 0; i < 300; ++i) e.tick(tilt(1));
    e.draw(canvas);
    CHECK(s.intact());
    CHECK(s.pixels() != start);
    ActorProbe p;
    CHECK(e.player(p));
    CHECK(p.x > 10.0f);
}

TG_TEST(engine_frames_match_golden_fingerprints) {
    const int cases = sizeof(GAME_CASES) / sizeof(GAME_CASES[0]);
    const int goldens = sizeof(golden::GAME) / sizeof(golden::GAME[0]);
    CHECK_EQ(cases, goldens);
    const Bytes bundle = caveHopBundle();
    for (int i = 0; i < cases && i < goldens; ++i) {
        const GameCase& c = GAME_CASES[i];
        CHECK(std::strcmp(c.name, golden::GAME[i].name) == 0);
        Engine e;
        CHECK(e.load(bundle.data(), bundle.size(), c.width, c.height));
        for (int t = 0; t < c.tick; ++t) e.tick(goldenInput(t));
        std::vector<gfx::Color> pixels(static_cast<std::size_t>(c.width) * c.height);
        gfx::Canvas canvas({pixels.data(), c.width, c.height, c.width});
        e.draw(canvas);
        if (hashPixels(pixels.data(), pixels.size()) != golden::GAME[i].hash)
            tg::test::fail(__FILE__, __LINE__, std::string("frame differs from golden: ") + c.name);
    }
}

int main() { return tg::test::run(); }
