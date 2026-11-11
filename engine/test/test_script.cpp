#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../lua/script.h"
#include "../lua/script_internal.h"
#include "tg_test.h"

using namespace tg;

namespace {

// A world that records every call, so the tests can see what Lua did.
struct FakeActor {
    std::string kind;
    float x = 0, y = 0, vx = 0, vy = 0;
    bool on_ground = false;
    bool alive = true;
    std::uint32_t serial = 1;
};

struct FakeWorld : ScriptWorld {
    std::vector<FakeActor> actors;
    std::vector<std::string> calls;   // human-readable record
    std::vector<std::string> logs;
    int score = 0;
    int remove_calls = 0;
    int sparkle_calls = 0;
    int add_calls = 0;
    int last_add = 0;
    float sparkle_x = 0, sparkle_y = 0;
    std::uint32_t next_random = 0;
    int random_calls = 0;

    ActorRef add(const char* kind, float x = 0, float y = 0) {
        FakeActor a;
        a.kind = kind;
        a.x = x;
        a.y = y;
        a.serial = 100 + static_cast<std::uint32_t>(actors.size());
        actors.push_back(a);
        return {static_cast<int>(actors.size()) - 1, a.serial};
    }
    FakeActor* get(ActorRef r) {
        if (r.index < 0 || r.index >= static_cast<int>(actors.size())) return nullptr;
        FakeActor& a = actors[r.index];
        return (a.alive && a.serial == r.serial) ? &a : nullptr;
    }
    bool actorAlive(ActorRef r) override { return get(r) != nullptr; }
    const char* actorKind(ActorRef r) override {
        FakeActor* a = get(r);
        return a ? a->kind.c_str() : "";
    }
    float actorGet(ActorRef r, ActorField f) override {
        FakeActor* a = get(r);
        if (!a) return 0;
        switch (f) {
            case ActorField::X: return a->x;
            case ActorField::Y: return a->y;
            case ActorField::Vx: return a->vx;
            case ActorField::Vy: return a->vy;
            case ActorField::OnGround: return a->on_ground ? 1.0f : 0.0f;
        }
        return 0;
    }
    void actorSet(ActorRef r, ActorField f, float v) override {
        FakeActor* a = get(r);
        if (!a) return;
        switch (f) {
            case ActorField::X: a->x = v; break;
            case ActorField::Y: a->y = v; break;
            case ActorField::Vx: a->vx = v; break;
            case ActorField::Vy: a->vy = v; break;
            case ActorField::OnGround: break;
        }
        calls.push_back("set");
    }
    void actorRemove(ActorRef r) override {
        FakeActor* a = get(r);
        if (a) a->alive = false;
        ++remove_calls;
        calls.push_back("actorRemove");
    }
    bool actorFind(const char* kind, ActorRef& out) override {
        for (std::size_t i = 0; i < actors.size(); ++i)
            if (actors[i].alive && actors[i].kind == kind) {
                out = {static_cast<int>(i), actors[i].serial};
                return true;
            }
        return false;
    }
    void scoreAdd(int n) override {
        score += n;
        last_add = n;
        ++add_calls;
        calls.push_back("scoreAdd(" + std::to_string(n) + ")");
    }
    int scoreGet() override { return score; }
    void fxSparkle(float x, float y) override {
        ++sparkle_calls;
        sparkle_x = x;
        sparkle_y = y;
        calls.push_back("fxSparkle");
    }
    void log(const char* text) override { logs.push_back(text); }
    std::uint32_t random() override {
        ++random_calls;
        return next_random;
    }
};

bool loadText(Script& s, const std::string& text, FakeWorld& w) {
    return s.load(text.data(), text.size(), w);
}

const char* CAVE_HOP =
    "-- Only what the stock parts cannot do: the gem is worth a lot and sparkles.\n"
    "on.touch(\"player\", \"gem\", function(player, gem)\n"
    "  fx.sparkle(gem.x, gem.y)\n"
    "  gem:remove()\n"
    "  score.add(50)\n"
    "end)\n";

bool contains(const char* text, const char* part) { return std::strstr(text, part) != nullptr; }

}  // namespace

TG_TEST(cave_hop_rules_gem_touch) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, CAVE_HOP, w));
    CHECK(!s.failed());
    ActorRef p = w.add("player");
    ActorRef g = w.add("gem", 3.5f, 4.5f);
    CHECK(s.wantsTouch("player", "gem"));
    s.touch("player", "gem", p, g);
    CHECK(!s.failed());
    CHECK_EQ(w.sparkle_calls, 1);
    CHECK_EQ(w.sparkle_x, 3.5f);
    CHECK_EQ(w.sparkle_y, 4.5f);
    CHECK_EQ(w.remove_calls, 1);
    CHECK_EQ(w.add_calls, 1);
    CHECK_EQ(w.last_add, 50);
    CHECK_EQ(w.score, 50);
    CHECK_EQ(w.calls.size(), static_cast<std::size_t>(3));
    CHECK(w.calls[0] == "fxSparkle");
    CHECK(w.calls[1] == "actorRemove");
    CHECK(w.calls[2] == "scoreAdd(50)");
}

TG_TEST(start_and_tick_handlers_run) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.start(function() score.add(1) end)\n"
                      "on.tick(function() score.add(10) end)\n"
                      "on.tick(function() score.add(100) end)\n", w));
    CHECK_EQ(w.score, 0);   // nothing runs at load
    s.start();
    CHECK_EQ(w.score, 1);
    s.tick();
    CHECK_EQ(w.score, 111);
    s.tick();
    CHECK_EQ(w.score, 221);
    s.start();
    CHECK_EQ(w.score, 222);
    CHECK(!s.failed());
}

TG_TEST(multiple_touch_handlers_for_one_pair_all_run) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.touch('a','b',function() score.add(1) end)\n"
                      "on.touch('a','b',function() score.add(20) end)\n"
                      "on.touch('a','c',function() score.add(300) end)\n", w));
    ActorRef a = w.add("a"), b = w.add("b");
    s.touch("a", "b", a, b);
    CHECK_EQ(w.score, 21);
    s.touch("a", "c", a, b);
    CHECK_EQ(w.score, 321);
    s.touch("b", "a", b, a);   // nobody listens to the reverse order
    CHECK_EQ(w.score, 321);
}

TG_TEST(touch_handler_gets_the_right_actors) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.touch('a','b',function(x, y) x.x = y.x + 1; y.vy = 7 end)", w));
    ActorRef a = w.add("a", 0, 0), b = w.add("b", 10, 0);
    s.touch("a", "b", a, b);
    CHECK(!s.failed());
    CHECK_EQ(w.actors[0].x, 11.0f);
    CHECK_EQ(w.actors[1].vy, 7.0f);
}

TG_TEST(wants_touch_true_and_false) {
    FakeWorld w;
    Script s;
    CHECK(!s.wantsTouch("player", "gem"));   // before load
    CHECK(loadText(s, CAVE_HOP, w));
    CHECK(s.wantsTouch("player", "gem"));
    CHECK(!s.wantsTouch("gem", "player"));
    CHECK(!s.wantsTouch("player", "coin"));
    CHECK(!s.wantsTouch("playe", "gem"));
    CHECK(!s.wantsTouch(nullptr, "gem"));
    // Answering must not enter Lua: no instruction is spent, no call happens.
    CHECK(w.calls.empty());
}

TG_TEST(actor_reads_reach_the_world) {
    FakeWorld w;
    Script s;
    ActorRef r = w.add("hero", 1.5f, 2.5f);
    w.actors[0].vx = -3;
    w.actors[0].vy = 4;
    w.actors[0].on_ground = true;
    CHECK(loadText(s,
        "on.tick(function()\n"
        "  local a = actor.find('hero')\n"
        "  log(a.kind .. ' ' .. a.x .. ' ' .. a.y .. ' ' .. a.vx .. ' ' .. a.vy)\n"
        "  log(tostring(a.on_ground) .. ' ' .. tostring(a.alive))\n"
        "end)", w));
    s.tick();
    CHECK(!s.failed());
    CHECK_EQ(w.logs.size(), static_cast<std::size_t>(2));
    CHECK(w.logs[0] == "hero 1.5 2.5 -3.0 4.0");
    CHECK(w.logs[1] == "true true");
    (void)r;
}

TG_TEST(actor_writes_reach_the_world) {
    FakeWorld w;
    Script s;
    w.add("other");
    w.add("hero");
    CHECK(loadText(s, "on.tick(function()\n"
                      "  local a = actor.find('hero')\n"
                      "  a.x = 5; a.y = 6.5; a.vx = -1; a.vy = 2\n"
                      "end)", w));
    s.tick();
    CHECK(!s.failed());
    CHECK_EQ(w.actors[0].x, 0.0f);   // the other actor is untouched
    CHECK_EQ(w.actors[1].x, 5.0f);
    CHECK_EQ(w.actors[1].y, 6.5f);
    CHECK_EQ(w.actors[1].vx, -1.0f);
    CHECK_EQ(w.actors[1].vy, 2.0f);
}

TG_TEST(actor_find_returns_actor_or_nil) {
    FakeWorld w;
    Script s;
    w.add("gem");
    CHECK(loadText(s, "on.tick(function()\n"
                      "  if actor.find('gem') then score.add(1) end\n"
                      "  if actor.find('ghost') == nil then score.add(10) end\n"
                      "  if actor.find('gem') == actor.find('gem') then score.add(100) end\n"
                      "end)", w));
    s.tick();
    CHECK(!s.failed());
    CHECK_EQ(w.score, 111);
}

TG_TEST(stale_actor_is_harmless) {
    FakeWorld w;
    Script s;
    w.add("gem", 2, 3);
    CHECK(loadText(s,
        "local saved\n"
        "on.start(function() saved = actor.find('gem') end)\n"
        "on.tick(function()\n"
        "  saved:remove()\n"
        "  saved:remove()\n"                      // twice: harmless
        "  saved.x = 99\n"                        // write: does nothing
        "  if saved.x == nil and saved.kind == nil and saved.vx == nil\n"
        "     and saved.vy == nil and saved.on_ground == nil\n"
        "     and saved.alive == false then score.add(7) end\n"
        "end)", w));
    s.start();
    s.tick();
    CHECK(!s.failed());
    CHECK_EQ(w.score, 7);
    CHECK_EQ(w.remove_calls, 1);   // the second remove did not reach the world
    CHECK_EQ(w.actors[0].x, 2.0f);
}

TG_TEST(stale_when_slot_is_reused) {
    FakeWorld w;
    Script s;
    ActorRef r = w.add("gem");
    CHECK(loadText(s, "local saved\n"
                      "on.start(function() saved = actor.find('gem') end)\n"
                      "on.tick(function() if saved.alive then score.add(1) end end)", w));
    s.start();
    s.tick();
    CHECK_EQ(w.score, 1);
    w.actors[r.index].serial += 1;   // slot reused by a new actor
    s.tick();
    CHECK_EQ(w.score, 1);
    CHECK(!s.failed());
}

TG_TEST(unknown_and_read_only_fields_are_errors) {
    const char* bodies[] = {"a.color = 1", "a.kind = 'x'", "a.on_ground = true",
                            "a.alive = false", "a.remove = 1", "a.x = 'far'",
                            "a.x = 1/0"};
    for (const char* body : bodies) {
        FakeWorld w;
        Script s;
        w.add("hero");
        std::string src = std::string("on.tick(function()\n local a = actor.find('hero')\n ") + body + "\nend)";
        CHECK(loadText(s, src, w));
        s.tick();
        CHECK(s.failed());
        CHECK(contains(s.error(), "line 3"));
    }
    FakeWorld w;
    Script s;
    w.add("hero");
    CHECK(loadText(s, "on.tick(function() actor.find('hero').color = 1 end)", w));
    s.tick();
    CHECK(s.failed());
    CHECK(contains(s.error(), "color"));
}

TG_TEST(reading_unknown_field_is_nil) {
    FakeWorld w;
    Script s;
    w.add("hero");
    CHECK(loadText(s, "on.tick(function() if actor.find('hero').nothing == nil then score.add(1) end end)", w));
    s.tick();
    CHECK_EQ(w.score, 1);
    CHECK(!s.failed());
}

TG_TEST(score_and_log) {
    FakeWorld w;
    Script s;
    w.score = 5;
    CHECK(loadText(s, "on.tick(function() score.add(score.get() + 1); log('hi'); log(42) end)", w));
    s.tick();
    CHECK_EQ(w.score, 11);
    CHECK_EQ(w.logs.size(), static_cast<std::size_t>(2));
    CHECK(w.logs[0] == "hi");
    CHECK(w.logs[1] == "42");
}

TG_TEST(endless_loop_in_handler_is_stopped) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.tick(function() while true do end end)", w));
    CHECK(!s.failed());
    s.tick();
    CHECK(s.failed());
    CHECK(contains(s.error(), "line 1"));
    CHECK(contains(s.error(), "too long"));
}

TG_TEST(endless_loop_in_start_and_touch_is_stopped) {
    {
        FakeWorld w;
        Script s;
        CHECK(loadText(s, "on.start(function() repeat until false end)", w));
        s.start();
        CHECK(s.failed());
    }
    {
        FakeWorld w;
        Script s;
        CHECK(loadText(s, "on.touch('a','b',function() while true do end end)", w));
        ActorRef a = w.add("a"), b = w.add("b");
        s.touch("a", "b", a, b);
        CHECK(s.failed());
    }
}

TG_TEST(endless_loop_at_load_is_stopped) {
    FakeWorld w;
    Script s;
    CHECK(!loadText(s, "while true do end", w));
    CHECK(s.failed());
    CHECK(contains(s.error(), "line 1"));
}

TG_TEST(endless_loop_under_pcall_still_fails) {
    const char* sources[] = {
        "on.tick(function() pcall(function() while true do end end) end)",
        "on.tick(function() while true do pcall(function() while true do end end) end end)",
        "on.tick(function() repeat local ok = pcall(function() while true do end end) until false end)",
        "on.tick(function() while true do pcall(error, 'x') end end)",
        "on.tick(function() xpcall(function() while true do end end, function() end) end)",
    };
    for (const char* src : sources) {
        FakeWorld w;
        Script s;
        CHECK(loadText(s, src, w));
        s.tick();
        CHECK(s.failed());
        s.tick();   // later calls are no-ops
        CHECK(s.failed());
    }
    // At load time too.
    FakeWorld w;
    Script s;
    CHECK(!loadText(s, "while true do pcall(function() while true do end end) end", w));
    CHECK(s.failed());
}

TG_TEST(pcall_cannot_swallow_a_budget_error_even_once) {
    // A short loop that does finish inside pcall is fine; only running over
    // the budget fails the script, and then it stays failed.
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.tick(function() local n = 0; for i = 1, 100 do n = n + i end; score.add(n) end)", w));
    s.tick();
    CHECK(!s.failed());
    CHECK_EQ(w.score, 5050);
    FakeWorld w2;
    Script s2;
    CHECK(loadText(s2, "on.tick(function() pcall(function() while true do end end); score.add(1) end)", w2));
    s2.tick();
    CHECK(s2.failed());
    CHECK_EQ(w2.score, 0);   // the line after pcall never ran
}

TG_TEST(budget_is_per_call) {
    // About 60,000 VM instructions per tick: fine every time.
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.tick(function() local n = 0; for i = 1, 15000 do n = n + 1 end end)", w));
    for (int i = 0; i < 20; ++i) s.tick();
    CHECK(!s.failed());
    // A loop that needs far more than the budget fails.
    FakeWorld w2;
    Script s2;
    CHECK(loadText(s2, "on.tick(function() local n = 0; for i = 1, 1000000 do n = n + 1 end end)", w2));
    s2.tick();
    CHECK(s2.failed());
}

TG_TEST(memory_hog_table_is_stopped) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "local t = {}\n"
                      "on.tick(function() for i = 1, 100000 do t[#t + 1] = i end end)", w));
    for (int i = 0; i < 100 && !s.failed(); ++i) s.tick();
    CHECK(s.failed());
    CHECK(contains(s.error(), "memory"));
}

TG_TEST(memory_hog_string_is_stopped) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.tick(function() local s = 'x'; while true do s = s .. s end end)", w));
    s.tick();
    CHECK(s.failed());
    CHECK(contains(s.error(), "memory"));
    FakeWorld w2;
    Script s2;
    CHECK(!loadText(s2, "local s = string.rep('abcdefgh', 1000000)", w2));
    CHECK(s2.failed());
    CHECK(contains(s2.error(), "memory"));
    FakeWorld w3;
    Script s3;
    CHECK(!loadText(s3, "local t = {}\nfor i = 1, 1000000 do t[i] = {i} end", w3));
    CHECK(s3.failed());
}

TG_TEST(memory_hog_pcall_still_fails_or_survives_without_crash) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "local t = {}\n"
                      "on.tick(function() while true do pcall(function() t[#t + 1] = string.rep('x', 1000) end) end end)", w));
    s.tick();
    CHECK(s.failed());
}

TG_TEST(memory_stays_within_limit) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "local t = {}\n"
                      "on.tick(function() for i = 1, 100000 do t[#t + 1] = i end end)", w));
    std::size_t peak = 0;
    for (int i = 0; i < 100 && !s.failed(); ++i) {
        s.tick();
        if (lua_internal::liveBytes() > peak) peak = lua_internal::liveBytes();
    }
    CHECK(peak <= Script::MEMORY_LIMIT);
    CHECK_EQ(lua_internal::liveBytes(), static_cast<std::size_t>(0));   // failed state was freed
}

TG_TEST(removed_globals_and_libraries_are_absent) {
    const char* names[] = {"io", "os", "debug", "package", "require", "load", "dofile",
                           "loadfile", "loadstring", "collectgarbage", "coroutine", "utf8"};
    for (const char* name : names) {
        FakeWorld w;
        Script s;
        std::string src = std::string("if ") + name + " == nil then score.add(1) end\n"
                          "if rawget(_G, '" + name + "') == nil then score.add(10) end";
        CHECK(loadText(s, src, w));
        CHECK_EQ(w.score, 11);
        if (w.score != 11) std::printf("    (global %s is present)\n", name);
    }
    // Calling one is a normal Lua error that fails the script.
    FakeWorld w;
    Script s;
    CHECK(!loadText(s, "load('x = 1')", w));
    CHECK(s.failed());
    FakeWorld w2;
    Script s2;
    CHECK(!loadText(s2, "collectgarbage()", w2));
    CHECK(s2.failed());
}

TG_TEST(allowed_libraries_are_present) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s,
        "assert(type(math.floor) == 'function' and type(string.format) == 'function'\n"
        "  and type(table.insert) == 'function' and type(pcall) == 'function'\n"
        "  and type(ipairs) == 'function' and type(setmetatable) == 'function')\n"
        "assert(string.format('%d-%s', 3, 'a') == '3-a')\n"
        "assert(math.floor(2.7) == 2)\n"
        "local t = {3, 1, 2}; table.sort(t); assert(t[1] == 1 and #t == 3)\n"
        "score.add(1)", w));
    CHECK_EQ(w.score, 1);
    // The string metatable is the way into string functions; make sure no
    // path leads to a loader.
    FakeWorld w2;
    Script s2;
    CHECK(loadText(s2, "local n = 0; for k in pairs(_G) do n = n + 1 end; score.add(n)", w2));
    CHECK(w2.score > 10);
}

TG_TEST(bytecode_is_rejected) {
    {
        FakeWorld w;
        Script s;
        const std::string chunk = std::string("\x1b" "Lua") + "\x54\x00\x19\x93\r\n\x1a\n";
        CHECK(!s.load(chunk.data(), chunk.size(), w));
        CHECK(s.failed());
        CHECK(std::strlen(s.error()) > 0);
    }
    {
        // Sneaking the signature after a comment line is just a syntax error.
        FakeWorld w;
        Script s;
        const std::string chunk = "\x1b" "Lua";
        CHECK(!s.load(chunk.data(), 4, w));
        CHECK(s.failed());
    }
    {
        // A real compiled chunk (made by this very Lua) is refused too.
        FakeWorld w;
        Script maker;
        CHECK(loadText(maker, "dumped = string.dump(function() score.add(1000) end)\n"
                              "on.start(function() log(dumped) end)", w));
        maker.start();
        CHECK_EQ(w.logs.size(), static_cast<std::size_t>(1));
        const std::string bytes = w.logs[0];   // may be cut at a NUL by log(const char*)
        FakeWorld w2;
        Script s;
        CHECK(!s.load(bytes.data(), bytes.size(), w2));
        CHECK(s.failed());
        CHECK_EQ(w2.score, 0);
    }
}

TG_TEST(syntax_error_message_has_line_number) {
    FakeWorld w;
    Script s;
    CHECK(!loadText(s, "-- one\n-- two\nlocal x = = 3\n", w));
    CHECK(s.failed());
    CHECK(contains(s.error(), "rules.lua line 3"));
}

TG_TEST(runtime_error_message_has_line_number) {
    FakeWorld w;
    Script s;
    CHECK(!loadText(s, "local a = 1\nlocal b = nil\nlocal c = b.x\n", w));
    CHECK(contains(s.error(), "rules.lua line 3"));
    FakeWorld w2;
    Script s2;
    CHECK(loadText(s2, "on.tick(function()\n\n  error('boom')\nend)", w2));
    s2.tick();
    CHECK(s2.failed());
    CHECK(contains(s2.error(), "rules.lua line 3"));
    CHECK(contains(s2.error(), "boom"));
    FakeWorld w3;
    Script s3;
    CHECK(loadText(s3, "on.tick(function()\n  local x = nil + 1\nend)", w3));
    s3.tick();
    CHECK(contains(s3.error(), "line 2"));
    FakeWorld w4;
    Script s4;
    CHECK(!loadText(s4, "\n\non.touch(1)", w4));
    CHECK(contains(s4.error(), "line 3"));
    // Errors with no text still give some message.
    FakeWorld w5;
    Script s5;
    CHECK(loadText(s5, "on.tick(function() error({}) end)", w5));
    s5.tick();
    CHECK(s5.failed());
    CHECK(std::strlen(s5.error()) > 0);
}

TG_TEST(after_failure_every_call_is_a_noop) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s,
        "on.start(function() score.add(1) end)\n"
        "on.tick(function() score.add(10); error('bad') end)\n"
        "on.touch('a','b',function() score.add(100) end)", w));
    ActorRef a = w.add("a"), b = w.add("b");
    s.tick();
    CHECK(s.failed());
    CHECK_EQ(w.score, 10);
    const std::string message = s.error();
    s.start();
    s.tick();
    s.touch("a", "b", a, b);
    CHECK(!s.wantsTouch("a", "b"));
    CHECK_EQ(w.score, 10);
    CHECK(message == s.error());
    CHECK(!loadText(s, "on.start(function() score.add(1000) end)", w));
    CHECK_EQ(w.score, 10);
}

TG_TEST(calls_before_load_do_nothing) {
    Script s;
    FakeWorld w;
    ActorRef a = w.add("a");
    s.start();
    s.tick();
    s.touch("a", "a", a, a);
    CHECK(!s.wantsTouch("a", "a"));
    CHECK(!s.failed());
}

TG_TEST(odd_input_does_not_crash) {
    const char* sources[] = {
        "", "\n", "return", "return 1", "x = ", "(", "))))", "local",
        "on.start()", "on.tick(1)", "on.touch()", "on.touch('a')", "on.touch('a','b')",
        "actor.find()", "actor.find({})", "score.add()", "score.add('x')", "score.add(0/0)",
        "score.add(1e30)", "fx.sparkle()", "fx.sparkle(1)", "log()", "log(nil)", "log({})",
        "setmetatable(_G, {__index = function() error('x') end}); local y = zzz",
        "local a = actor; actor = nil; on.start(function() a.find('x') end)",
        "string.rep('x', -1)", "string.rep('x', 1e9)", "('x'):rep(1e9)",
        "local t = setmetatable({}, {__gc = function() while true do end end}); t = nil",
        "setmetatable({}, {__gc = function() error('x') end})",
        "local function f() return f() end return f()",
        "local function f() return 1 + f() end return f()",
        "local t = {} t[1] = t for i = 1, 100 do t = {t} end",
        "return string.format('%99999d', 1)",
        "error()", "error(nil)", "error(setmetatable({}, {__tostring = function() return 'x' end}))",
        "math.random(0, -1)", "math.random(1, 2, 3)", "math.random('a')",
        "on.tick(function() return actor.find('x').y end)",
        "local s = string.rep('(', 300) .. string.rep(')', 300) load = nil",
        "return #arg",
    };
    for (const char* src : sources) {
        FakeWorld w;
        Script s;
        w.add("x");
        const bool ok = loadText(s, src, w);
        CHECK(ok == !s.failed());
        s.start();
        s.tick();
        ActorRef a = w.add("a");
        s.touch("a", "b", a, a);
        s.touch("x", "x", a, a);
    }
    // Deep nesting in the parser.
    FakeWorld w;
    Script s;
    std::string deep(5000, '(');
    CHECK(!loadText(s, "return " + deep, w));
    CHECK(s.failed());
    Script n;
    CHECK(!n.load(nullptr, 0, w));
    CHECK(n.failed());
    // Embedded NUL bytes and non-UTF-8.
    FakeWorld w2;
    Script z;
    const char odd[] = "score.add(1)\0score.add(2)";
    CHECK(!z.load(odd, sizeof odd - 1, w2));
    Script hi;
    const char bin[] = "\xff\xfe\x80";
    CHECK(!hi.load(bin, 3, w2));
}

TG_TEST(math_random_uses_world_random) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s,
        "on.tick(function()\n"
        "  log(tostring(math.random()))\n"
        "  log(tostring(math.random(10)))\n"
        "  log(tostring(math.random(5, 8)))\n"
        "  math.randomseed(12345)\n"
        "  math.randomseed()\n"
        "end)", w));
    w.next_random = 0;
    s.tick();
    CHECK(!s.failed());
    CHECK_EQ(w.random_calls, 3);   // randomseed drew nothing
    CHECK(w.logs[0] == "0.0");
    CHECK(w.logs[1] == "1");
    CHECK(w.logs[2] == "5");
    w.logs.clear();
    w.next_random = 0xFFFFFFFFu;
    s.tick();
    CHECK(w.logs[0] != "1.0");
    CHECK(std::stof(w.logs[0]) < 1.0f);
    CHECK(std::stof(w.logs[0]) > 0.99f);
    CHECK(w.logs[1] == "6");   // 0xFFFFFFFF % 10 + 1
    CHECK(w.logs[2] == "8");   // 0xFFFFFFFF % 4 + 5
}

TG_TEST(math_random_respects_ranges) {
    FakeWorld w;
    Script s;
    CHECK(loadText(s,
        "local seen = {}\n"
        "on.tick(function()\n"
        "  for i = 1, 200 do\n"
        "    local a = math.random(6)\n"
        "    local b = math.random(-3, 3)\n"
        "    local c = math.random(7, 7)\n"
        "    local d = math.random()\n"
        "    if a < 1 or a > 6 or a ~= math.floor(a) then error('a') end\n"
        "    if b < -3 or b > 3 then error('b') end\n"
        "    if c ~= 7 then error('c') end\n"
        "    if d < 0 or d >= 1 then error('d') end\n"
        "  end\n"
        "end)", w));
    for (std::uint32_t v : {0u, 1u, 5u, 6u, 7u, 0x80000000u, 0xFFFFFFFFu, 12345u}) {
        w.next_random = v;
        s.tick();
        CHECK(!s.failed());
        if (s.failed()) std::printf("    %s\n", s.error());
    }
    // Empty intervals are errors, like in Lua.
    FakeWorld w2;
    Script s2;
    CHECK(loadText(s2, "on.tick(function() math.random(5, 1) end)", w2));
    s2.tick();
    CHECK(s2.failed());
    // A float result is a float and an integer result is an integer.
    FakeWorld w3;
    Script s3;
    CHECK(loadText(s3, "on.tick(function() log(math.type(math.random())); log(math.type(math.random(3))) end)", w3));
    s3.tick();
    CHECK(w3.logs[0] == "float");
    CHECK(w3.logs[1] == "integer");
}

TG_TEST(two_scripts_are_independent) {
    FakeWorld w1, w2;
    Script a, b;
    CHECK(loadText(a, "counter = 0\non.tick(function() counter = counter + 1; score.add(counter) end)", w1));
    CHECK(loadText(b, "counter = 100\non.tick(function() counter = counter + 100; score.add(counter) end)", w2));
    a.tick();
    b.tick();
    a.tick();
    CHECK_EQ(w1.score, 3);     // 1 + 2
    CHECK_EQ(w2.score, 200);   // 200
    // One failing does not affect the other.
    Script bad;
    FakeWorld w3;
    CHECK(!loadText(bad, "error('x')", w3));
    a.tick();
    CHECK(!a.failed());
    CHECK_EQ(w1.score, 6);
    // Globals do not leak between scripts.
    FakeWorld w4;
    Script c;
    CHECK(loadText(c, "if counter == nil then score.add(1) end", w4));
    CHECK_EQ(w4.score, 1);
}

TG_TEST(create_and_destroy_200_scripts_does_not_leak) {
    CHECK_EQ(lua_internal::liveBytes(), static_cast<std::size_t>(0));
    for (int i = 0; i < 200; ++i) {
        FakeWorld w;
        Script s;
        w.add("a");
        w.add("b");
        switch (i % 4) {
            case 0: loadText(s, CAVE_HOP, w); break;
            case 1:
                loadText(s, "local t = {}\nfor i = 1, 500 do t[i] = tostring(i) end\n"
                            "on.tick(function() t[#t + 1] = 'x' end)\n"
                            "on.start(function() end)\n"
                            "on.touch('a','b',function(x, y) x.x = y.x end)", w);
                s.start();
                s.tick();
                s.touch("a", "b", {0, w.actors[0].serial}, {1, w.actors[1].serial});
                break;
            case 2: loadText(s, "error('bad')", w); break;
            case 3: loadText(s, "on.tick(function() while true do end end)", w); s.tick(); break;
        }
        CHECK(lua_internal::liveBytes() > 0 || s.failed());
    }
    CHECK_EQ(lua_internal::liveBytes(), static_cast<std::size_t>(0));
    // Heap-allocated, explicitly deleted.
    for (int i = 0; i < 20; ++i) {
        FakeWorld w;
        std::unique_ptr<Script> s(new Script);
        loadText(*s, CAVE_HOP, w);
        CHECK(lua_internal::liveBytes() > 0);
        s.reset();
        CHECK_EQ(lua_internal::liveBytes(), static_cast<std::size_t>(0));
    }
}

TG_TEST(garbage_is_collected_across_ticks) {
    // Each tick makes garbage; the per-tick GC step must keep memory bounded.
    FakeWorld w;
    Script s;
    CHECK(loadText(s, "on.tick(function() local t = {}; for i = 1, 200 do t[i] = {i, tostring(i)} end end)", w));
    for (int i = 0; i < 2000; ++i) s.tick();
    CHECK(!s.failed());
    CHECK(lua_internal::lastStateBytes() < Script::MEMORY_LIMIT);
}

TG_TEST(a_garbage_finalizer_cannot_hang_shutdown) {
    // Lua turns hooks off inside __gc, so scripts may not create finalizers.
    FakeWorld w;
    {
        Script s;
        CHECK(!loadText(s, "setmetatable({}, {__gc = function() while true do end end})", w));
        CHECK(s.failed());
        CHECK(contains(s.error(), "__gc"));
        CHECK(contains(s.error(), "line 1"));
    }
    {
        Script s;
        // Adding __gc afterwards does not mark the object in Lua: harmless.
        CHECK(loadText(s, "local mt = {}\nlocal t = setmetatable({}, mt)\n"
                          "mt.__gc = function() while true do end end\n"
                          "t = nil; mt = nil", w));
        s.tick();
    }   // destructor must return
    {
        Script s;
        CHECK(loadText(s, "local t = setmetatable({}, {__index = function() return 5 end})\n"
                          "assert(t.x == 5 and getmetatable(t) ~= nil)\n"
                          "assert(getmetatable(setmetatable({}, {__metatable = 'locked'})) == 'locked')\n"
                          "assert(not pcall(setmetatable, setmetatable({}, {__metatable = 1}), {}))", w));
    }
    CHECK_EQ(lua_internal::liveBytes(), static_cast<std::size_t>(0));
}

int main() { return tg::test::run(); }
