// Lua rules inside the engine: touches, errors and the error screen.

#include <cstring>
#include <string>

#include "engine_helpers.h"
#include "tg_test.h"

using namespace tg;
using namespace tg::test;

namespace {

const char* GEM_RULES =
    "on.touch('player', 'gem', function(player, gem)\n"
    "  fx.sparkle(gem.x, gem.y)\n"
    "  gem:remove()\n"
    "  score.add(50)\n"
    "end)\n";

BundleSpec rulesSpec(const std::string& rules) {
    BundleSpec s = arenaSpec(arenaRows({{8, "...P.og.s"}}));
    s.has_rules = !rules.empty();
    s.rules = rules;
    return s;
}

int probeNumber(const Engine& e, const char* name) {
    char text[2048];
    e.probeText(text, sizeof(text));
    const std::string all = text;
    const std::string key = std::string(name) + " ";
    const std::size_t at = all.find(key);
    return at == std::string::npos ? -1 : std::atoi(all.c_str() + at + key.size());
}

int count(const Engine& e, const char* kind) {
    int n = 0;
    ActorProbe p;
    for (int i = 0; e.actor(i, p); ++i)
        if (std::strcmp(p.kind, kind) == 0) ++n;
    return n;
}

// Runs right until the score reaches `want` or the time is up.
void runUntilScore(Arena& a, int want, int max_ticks = 300) {
    for (int i = 0; i < max_ticks && a.engine.score() < want; ++i) a.engine.tick(tilt(1.0f));
}

}  // namespace

TG_TEST(touching_the_gem_gives_fifty_removes_it_and_sparkles) {
    Arena a(rulesSpec(GEM_RULES));
    CHECK(a.ok);
    CHECK_EQ(count(a.engine, "gem"), 1);
    CHECK_EQ(probeNumber(a.engine, "sparkles"), 0);
    // The coin is on the way: 10 for the coin, then 50 for the gem.
    runUntilScore(a, 60);
    CHECK_EQ(a.engine.score(), 60);
    CHECK_EQ(count(a.engine, "gem"), 0);
    CHECK_EQ(count(a.engine, "coin"), 0);
    CHECK_EQ(probeNumber(a.engine, "sparkles"), 1);
    CHECK_EQ(std::string(a.engine.error()), "");
}

TG_TEST(a_coin_still_gives_ten) {
    Arena a(rulesSpec(GEM_RULES));
    CHECK(a.ok);
    runUntilScore(a, 10);
    CHECK_EQ(a.engine.score(), 10);
    CHECK_EQ(count(a.engine, "gem"), 1);
}

TG_TEST(the_gem_is_worth_nothing_without_rules) {
    Arena a(rulesSpec(""));
    CHECK(a.ok);
    for (int i = 0; i < 200; ++i) a.engine.tick(tilt(1.0f));
    CHECK_EQ(a.engine.score(), 10);  // the coin only
    CHECK_EQ(count(a.engine, "gem"), 1);
    CHECK_EQ(std::string(a.engine.error()), "");
    GuardedScreen screen(320, 172);
    gfx::Canvas canvas(screen.surface());
    a.engine.draw(canvas);
    CHECK(screen.intact());
}

TG_TEST(an_endless_loop_stops_the_game_with_an_error) {
    Arena a(rulesSpec("on.tick(function()\n  while true do end\nend)\n"));
    CHECK(a.ok);
    CHECK_EQ(std::string(a.engine.error()), "");
    a.engine.tick(tilt(1.0f));
    const std::string error = a.engine.error();
    CHECK(!error.empty());
    CHECK(error.find("line 2") != std::string::npos);
    char before[2048], after[2048];
    a.engine.probeText(before, sizeof(before));
    for (int i = 0; i < 50; ++i) a.engine.tick(tilt(1.0f));
    a.engine.probeText(after, sizeof(after));
    CHECK_EQ(std::string(before), std::string(after));
    CHECK_EQ(std::string(a.engine.error()), error);
}

TG_TEST(the_error_screen_is_drawn_inside_the_screen) {
    for (int w : {320, 280}) {
        Arena a(rulesSpec("on.tick(function()\n  error('this is a very long made up message to see how it wraps')\nend)\n"),
                w, w == 320 ? 172 : 240);
        CHECK(a.ok);
        a.engine.tick({});
        CHECK(std::strlen(a.engine.error()) > 0);
        GuardedScreen screen(w, w == 320 ? 172 : 240);
        gfx::Canvas canvas(screen.surface());
        a.engine.draw(canvas);
        CHECK(screen.intact());
        const std::vector<gfx::Color> px = screen.pixels();
        int other = 0;
        for (gfx::Color c : px) other += c != px[0];
        CHECK(other > 100);  // text was drawn
    }
}

TG_TEST(a_syntax_error_fails_load_with_the_line_number) {
    Engine e;
    const Bytes b = buildBundle(rulesSpec("-- one\non.tick(function()\nlocal = \nend)\n"));
    CHECK(!e.load(b.data(), b.size(), 320, 172));
    const std::string error = e.error();
    CHECK(error.find("line 3") != std::string::npos);
}

TG_TEST(a_game_without_rules_still_runs) {
    Arena a(rulesSpec(""));
    CHECK(a.ok);
    a.run(60, tilt(1.0f));
    CHECK_EQ(a.engine.ticks(), 60u);
    CHECK_EQ(std::string(a.engine.error()), "");
}

TG_TEST(on_start_runs_once_when_the_level_starts) {
    Arena a(rulesSpec("on.start(function() score.add(7) end)\n"));
    CHECK(a.ok);
    CHECK_EQ(a.engine.score(), 7);
    a.run(30);
    CHECK_EQ(a.engine.score(), 7);
}

int main() { return tg::test::run(); }
