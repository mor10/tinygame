#include "script.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "script_internal.h"

// tools/tg.py passes -DLUA_32BITS=1, but the vendored luaconf.h (unmodified
// Lua 5.4.7) unconditionally runs `#define LUA_32BITS 0`, so the C files are
// built with 64-bit numbers (the compiler only warns). Drop the command-line
// value so this file sees exactly the configuration the C files get, instead
// of failing on the redefinition. Reported in the hand-off notes.
#ifdef LUA_32BITS
#undef LUA_32BITS
#endif

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

// How this file stays safe.
//
// Lua reports errors with longjmp (it is built as C). A longjmp must never
// cross a C++ object that has a destructor, so every function Lua can call
// (the `l_` and `run_` functions, the hook) holds only plain values while a
// Lua call that may raise is possible. The one place that needs std::string
// (on.touch) finishes all its Lua work first.
//
// Every entry from the engine goes through Impl::enter, which resets the
// instruction budget and runs one protected call. Nothing that can raise is
// called outside a protected call, so Lua's panic handler is never reached.

namespace tg {

namespace {

constexpr int HOOK_INTERVAL = 1000;   // VM instructions between budget checks
constexpr std::size_t MAX_HANDLERS = 256;
constexpr char CHUNK_NAME[] = "=rules.lua";
constexpr char SOURCE_NAME[] = "rules.lua";
constexpr char ACTOR_META[] = "tg.actor";
constexpr char OUT_OF_MEMORY[] = "not enough memory";   // Lua's own text

std::size_t g_live_bytes = 0;
std::size_t g_last_state_bytes = 0;

struct TouchHandler {
    std::string a;
    std::string b;
    int ref;
};

struct State {
    lua_State* L = nullptr;
    ScriptWorld* world = nullptr;
    std::size_t used = 0;
    int remaining = 0;
    bool budget_exceeded = false;
    bool closing = false;
    bool failed = false;
    char error[240] = "";

    std::vector<int> start_handlers;
    std::vector<int> tick_handlers;
    std::vector<TouchHandler> touch_handlers;

    // Arguments for the protected runners (set just before Impl::enter).
    const char* source = nullptr;
    std::size_t source_length = 0;
    const char* touch_a = nullptr;
    const char* touch_b = nullptr;
    ActorRef touch_ref_a;
    ActorRef touch_ref_b;

    bool create();
    void close();
    void fail(int status);
    void failWith(const char* message);
    bool enter(lua_CFunction function);
    std::size_t handlerCount() const {
        return start_handlers.size() + tick_handlers.size() + touch_handlers.size();
    }
};

}  // namespace

std::size_t lua_internal::liveBytes() { return g_live_bytes; }
std::size_t lua_internal::lastStateBytes() { return g_last_state_bytes; }

struct Script::Impl : State {};

namespace {

using Impl = State;

Impl* self(lua_State* L) { return *static_cast<Impl**>(lua_getextraspace(L)); }

// ---- memory ---------------------------------------------------------------

void* allocate(void* user, void* pointer, std::size_t old_size, std::size_t new_size) {
    Impl* impl = static_cast<Impl*>(user);
    if (pointer == nullptr) old_size = 0;   // Lua passes a type tag here instead
    if (new_size == 0) {
        std::free(pointer);
        impl->used -= old_size;
        g_live_bytes -= old_size;
        g_last_state_bytes = impl->used;
        return nullptr;
    }
    if (new_size > old_size && impl->used - old_size + new_size > Script::MEMORY_LIMIT)
        return nullptr;
    void* block = std::realloc(pointer, new_size);
    if (block == nullptr) return nullptr;
    impl->used = impl->used - old_size + new_size;
    g_live_bytes = g_live_bytes - old_size + new_size;
    g_last_state_bytes = impl->used;
    return block;
}

// ---- instruction budget ---------------------------------------------------

void budgetHook(lua_State* L, lua_Debug* ar) {
    Impl* impl = self(L);
    if (!impl->budget_exceeded) {
        impl->remaining -= HOOK_INTERVAL;
        if (impl->remaining > 0) return;
        impl->budget_exceeded = true;
        if (!impl->closing) {
            const char* text = "this took too long, so it was stopped. Is there a loop that never ends?";
            if (lua_getinfo(L, "Sl", ar) != 0 && ar->currentline > 0)
                std::snprintf(impl->error, sizeof impl->error, "%s line %d: %s",
                              SOURCE_NAME, ar->currentline, text);
            else
                std::snprintf(impl->error, sizeof impl->error, "%s: %s", SOURCE_NAME, text);
        }
        // From now on stop at every instruction, so a pcall in the script can
        // catch this error only to meet it again on its very next step.
        lua_sethook(L, budgetHook, LUA_MASKCOUNT, 1);
    }
    lua_pushliteral(L, "the script ran for too long");
    lua_error(L);
}

// ---- actors ---------------------------------------------------------------

void pushActor(lua_State* L, ActorRef ref) {
    ActorRef* slot = static_cast<ActorRef*>(lua_newuserdatauv(L, sizeof(ActorRef), 0));
    *slot = ref;
    luaL_setmetatable(L, ACTOR_META);
}

ActorRef checkActor(lua_State* L, int index) {
    return *static_cast<ActorRef*>(luaL_checkudata(L, index, ACTOR_META));
}

bool numberField(const char* key, ActorField& field) {
    if (std::strcmp(key, "x") == 0) { field = ActorField::X; return true; }
    if (std::strcmp(key, "y") == 0) { field = ActorField::Y; return true; }
    if (std::strcmp(key, "vx") == 0) { field = ActorField::Vx; return true; }
    if (std::strcmp(key, "vy") == 0) { field = ActorField::Vy; return true; }
    return false;
}

bool readOnlyField(const char* key) {
    return std::strcmp(key, "kind") == 0 || std::strcmp(key, "on_ground") == 0 ||
           std::strcmp(key, "alive") == 0 || std::strcmp(key, "remove") == 0;
}

int l_actor_remove(lua_State* L) {
    const ActorRef ref = checkActor(L, 1);
    ScriptWorld* world = self(L)->world;
    if (world->actorAlive(ref)) world->actorRemove(ref);
    return 0;
}

int l_actor_index(lua_State* L) {
    const ActorRef ref = checkActor(L, 1);
    if (lua_type(L, 2) != LUA_TSTRING) { lua_pushnil(L); return 1; }
    const char* key = lua_tostring(L, 2);
    ScriptWorld* world = self(L)->world;
    if (std::strcmp(key, "remove") == 0) { lua_pushcfunction(L, l_actor_remove); return 1; }
    const bool alive = world->actorAlive(ref);
    if (std::strcmp(key, "alive") == 0) { lua_pushboolean(L, alive); return 1; }
    ActorField field;
    if (!alive) {
        lua_pushnil(L);   // a removed actor: every other read gives nil
    } else if (numberField(key, field)) {
        lua_pushnumber(L, static_cast<lua_Number>(world->actorGet(ref, field)));
    } else if (std::strcmp(key, "on_ground") == 0) {
        lua_pushboolean(L, world->actorGet(ref, ActorField::OnGround) != 0.0f);
    } else if (std::strcmp(key, "kind") == 0) {
        const char* kind = world->actorKind(ref);
        lua_pushstring(L, kind != nullptr ? kind : "");
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int l_actor_newindex(lua_State* L) {
    const ActorRef ref = checkActor(L, 1);
    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "actors only have named fields (x, y, vx, vy can be changed)");
    const char* key = lua_tostring(L, 2);
    ActorField field;
    if (!numberField(key, field)) {
        if (readOnlyField(key))
            return luaL_error(L, "actor.%s cannot be changed (only x, y, vx, vy can)", key);
        return luaL_error(L, "actors have no field '%s' (x, y, vx, vy can be changed)", key);
    }
    if (lua_type(L, 3) != LUA_TNUMBER)
        return luaL_error(L, "actor.%s must be a number, not %s", key, luaL_typename(L, 3));
    const lua_Number value = lua_tonumber(L, 3);
    if (!std::isfinite(value))
        return luaL_error(L, "actor.%s must be an ordinary number (got infinity or not-a-number)", key);
    ScriptWorld* world = self(L)->world;
    if (world->actorAlive(ref)) world->actorSet(ref, field, static_cast<float>(value));
    return 0;
}

int l_actor_eq(lua_State* L) {
    const ActorRef* a = static_cast<ActorRef*>(luaL_testudata(L, 1, ACTOR_META));
    const ActorRef* b = static_cast<ActorRef*>(luaL_testudata(L, 2, ACTOR_META));
    lua_pushboolean(L, a != nullptr && b != nullptr && a->index == b->index && a->serial == b->serial);
    return 1;
}

int l_actor_find(lua_State* L) {
    const char* kind = luaL_checkstring(L, 1);
    ActorRef ref;
    if (self(L)->world->actorFind(kind, ref)) pushActor(L, ref);
    else lua_pushnil(L);
    return 1;
}

// ---- on.* -----------------------------------------------------------------

int checkRoom(lua_State* L) {
    if (self(L)->handlerCount() >= MAX_HANDLERS)
        return luaL_error(L, "too many handlers (the limit is %d)", static_cast<int>(MAX_HANDLERS));
    return 0;
}

int addSimpleHandler(lua_State* L, std::vector<int> Impl::*list) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    checkRoom(L);
    lua_settop(L, 1);
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    bool stored = true;
    try {
        (self(L)->*list).push_back(ref);
    } catch (...) {
        stored = false;
    }
    if (!stored) return luaL_error(L, "%s", OUT_OF_MEMORY);
    return 0;
}

int l_on_start(lua_State* L) { return addSimpleHandler(L, &Impl::start_handlers); }
int l_on_tick(lua_State* L) { return addSimpleHandler(L, &Impl::tick_handlers); }

int l_on_touch(lua_State* L) {
    const char* a = luaL_checkstring(L, 1);
    const char* b = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    checkRoom(L);
    lua_settop(L, 3);
    // The kind strings stay on the Lua stack (and so stay valid) below.
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    bool stored = true;
    try {
        self(L)->touch_handlers.push_back(TouchHandler{a, b, ref});
    } catch (...) {
        stored = false;
    }
    if (!stored) return luaL_error(L, "%s", OUT_OF_MEMORY);
    return 0;
}

// ---- score, fx, log -------------------------------------------------------

int l_score_add(lua_State* L) {
    lua_Number amount = luaL_checknumber(L, 1);
    if (!(amount == amount)) return luaL_error(L, "score.add needs an ordinary number");
    const lua_Number limit = 1000000000.0f;
    if (amount > limit) amount = limit;
    if (amount < -limit) amount = -limit;
    self(L)->world->scoreAdd(static_cast<int>(std::lround(amount)));
    return 0;
}

int l_score_get(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(self(L)->world->scoreGet()));
    return 1;
}

int l_fx_sparkle(lua_State* L) {
    const lua_Number x = luaL_checknumber(L, 1);
    const lua_Number y = luaL_checknumber(L, 2);
    self(L)->world->fxSparkle(static_cast<float>(x), static_cast<float>(y));
    return 0;
}

int l_log(lua_State* L) {
    luaL_checkany(L, 1);
    const char* text = luaL_tolstring(L, 1, nullptr);
    self(L)->world->log(text);
    return 0;
}

// ---- math.random ----------------------------------------------------------

int l_random(lua_State* L) {
    ScriptWorld* world = self(L)->world;
    lua_Integer low = 1;
    lua_Integer high = 0;
    switch (lua_gettop(L)) {
        case 0:
            // 24 random bits: every value is exact in a 32-bit float, and < 1.
            lua_pushnumber(L, static_cast<lua_Number>(world->random() >> 8) *
                                  (static_cast<lua_Number>(1) / 16777216));
            return 1;
        case 1:
            high = luaL_checkinteger(L, 1);
            if (high == 0) {   // as in Lua 5.4: a whole random integer
                lua_pushinteger(L, static_cast<lua_Integer>(world->random()));
                return 1;
            }
            break;
        case 2:
            low = luaL_checkinteger(L, 1);
            high = luaL_checkinteger(L, 2);
            break;
        default:
            return luaL_error(L, "wrong number of arguments");
    }
    luaL_argcheck(L, low <= high, lua_gettop(L), "interval is empty");
    const lua_Unsigned span = static_cast<lua_Unsigned>(high) - static_cast<lua_Unsigned>(low) + 1u;
    lua_Unsigned pick = static_cast<lua_Unsigned>(world->random());
    if (span != 0) pick %= span;   // span 0 means the whole integer range
    lua_pushinteger(L, static_cast<lua_Integer>(static_cast<lua_Unsigned>(low) + pick));
    return 1;
}

// Lua switches debug hooks off while a finalizer (__gc) runs, so the
// instruction budget could not stop a finalizer that never ends. Scripts
// therefore may not create one: setmetatable refuses a metatable with __gc.
// (Adding __gc to a metatable after setmetatable has no effect in Lua.)
int l_setmetatable(lua_State* L) {
    if (lua_type(L, 2) == LUA_TTABLE && lua_getfield(L, 2, "__gc") != LUA_TNIL)
        return luaL_error(L, "__gc is not available in game scripts");
    lua_settop(L, 2);
    lua_pushvalue(L, lua_upvalueindex(1));   // the original setmetatable
    lua_insert(L, 1);
    lua_call(L, 2, 1);
    return 1;
}

int l_randomseed(lua_State*) { return 0; }   // the engine owns the seed

// ---- setup ----------------------------------------------------------------

void setFunction(lua_State* L, const char* name, lua_CFunction function) {
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
}

int run_setup(lua_State* L) {
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_settop(L, 0);

    static const char* const REMOVED[] = {"load", "loadstring", "dofile", "loadfile",
                                          "require", "collectgarbage"};
    for (const char* name : REMOVED) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }

    lua_getglobal(L, "setmetatable");
    lua_pushcclosure(L, l_setmetatable, 1);
    lua_setglobal(L, "setmetatable");

    lua_getglobal(L, LUA_MATHLIBNAME);
    setFunction(L, "random", l_random);
    setFunction(L, "randomseed", l_randomseed);
    lua_pop(L, 1);

    luaL_newmetatable(L, ACTOR_META);
    setFunction(L, "__index", l_actor_index);
    setFunction(L, "__newindex", l_actor_newindex);
    setFunction(L, "__eq", l_actor_eq);
    lua_pushboolean(L, 0);   // getmetatable(actor) gives false, not the table
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);

    lua_newtable(L);
    setFunction(L, "start", l_on_start);
    setFunction(L, "tick", l_on_tick);
    setFunction(L, "touch", l_on_touch);
    lua_setglobal(L, "on");

    lua_newtable(L);
    setFunction(L, "find", l_actor_find);
    lua_setglobal(L, "actor");

    lua_newtable(L);
    setFunction(L, "add", l_score_add);
    setFunction(L, "get", l_score_get);
    lua_setglobal(L, "score");

    lua_newtable(L);
    setFunction(L, "sparkle", l_fx_sparkle);
    lua_setglobal(L, "fx");

    lua_pushcfunction(L, l_log);
    lua_setglobal(L, "log");

    lua_gc(L, LUA_GCINC, 0, 0, 0);   // incremental mode, default pacing
    return 0;
}

// ---- protected runners ----------------------------------------------------

int run_load(lua_State* L) {
    Impl* impl = self(L);
    // Mode "t": text only. A precompiled chunk is refused.
    if (luaL_loadbufferx(L, impl->source, impl->source_length, CHUNK_NAME, "t") != LUA_OK)
        return lua_error(L);
    lua_call(L, 0, 0);
    return 0;
}

void runHandlers(lua_State* L, const std::vector<int>& handlers) {
    // Handlers added while these run wait until the next call.
    const std::size_t count = handlers.size();
    for (std::size_t i = 0; i < count; ++i) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, handlers[i]);
        lua_call(L, 0, 0);
    }
}

int run_start(lua_State* L) {
    runHandlers(L, self(L)->start_handlers);
    return 0;
}

int run_tick(lua_State* L) {
    runHandlers(L, self(L)->tick_handlers);
    lua_gc(L, LUA_GCSTEP, 0);   // one bounded step of the incremental collector
    return 0;
}

int run_touch(lua_State* L) {
    Impl* impl = self(L);
    pushActor(L, impl->touch_ref_a);
    pushActor(L, impl->touch_ref_b);
    const std::size_t count = impl->touch_handlers.size();
    for (std::size_t i = 0; i < count; ++i) {
        const TouchHandler& handler = impl->touch_handlers[i];
        if (handler.a != impl->touch_a || handler.b != impl->touch_b) continue;
        lua_rawgeti(L, LUA_REGISTRYINDEX, handler.ref);
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 0);
    }
    return 0;
}

// "rules.lua:12: oops" becomes "rules.lua line 12: oops".
void describe(const char* message, char* out, std::size_t size) {
    const std::size_t name_length = sizeof SOURCE_NAME - 1;
    if (std::strncmp(message, SOURCE_NAME, name_length) == 0 && message[name_length] == ':') {
        const char* digits = message + name_length + 1;
        const char* end = digits;
        while (*end >= '0' && *end <= '9') ++end;
        if (end != digits && end[0] == ':' && end[1] == ' ') {
            std::snprintf(out, size, "%s line %.*s: %s", SOURCE_NAME,
                          static_cast<int>(end - digits), digits, end + 2);
            return;
        }
    }
    std::snprintf(out, size, "%s: %s", SOURCE_NAME, message);
}

bool State::create() {
    L = lua_newstate(allocate, this);
    if (L == nullptr) return false;
    *static_cast<Impl**>(lua_getextraspace(L)) = this;
    lua_pushcfunction(L, run_setup);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) return false;
    return true;
}

void State::close() {
    if (L != nullptr) {
        // Closing runs the script's finalizers (__gc); keep them on a budget.
        closing = true;
        budget_exceeded = false;
        remaining = Script::INSTRUCTION_BUDGET;
        lua_sethook(L, budgetHook, LUA_MASKCOUNT, HOOK_INTERVAL);
        lua_close(L);
        L = nullptr;
        closing = false;
    }
    start_handlers.clear();
    tick_handlers.clear();
    touch_handlers.clear();
}

void State::failWith(const char* message) {
    failed = true;
    std::snprintf(error, sizeof error, "%s", message);
    close();
}

void State::fail(int status) {
    char text[sizeof error];
    if (budget_exceeded) {
        std::snprintf(text, sizeof text, "%s", error);   // written by the hook
    } else {
        const char* message =
            (L != nullptr && lua_gettop(L) > 0 && lua_type(L, -1) == LUA_TSTRING)
                ? lua_tostring(L, -1) : nullptr;
        if (status == LUA_ERRMEM || (message != nullptr && std::strstr(message, OUT_OF_MEMORY)))
            std::snprintf(text, sizeof text,
                          "%s: the script used too much memory (the limit is 1 MB)", SOURCE_NAME);
        else if (message != nullptr)
            describe(message, text, sizeof text);
        else
            std::snprintf(text, sizeof text, "%s: the script stopped with an error", SOURCE_NAME);
    }
    failWith(text);
}

bool State::enter(lua_CFunction function) {
    if (L == nullptr || failed) return false;
    remaining = Script::INSTRUCTION_BUDGET;
    budget_exceeded = false;
    lua_sethook(L, budgetHook, LUA_MASKCOUNT, HOOK_INTERVAL);
    lua_settop(L, 0);
    lua_pushcfunction(L, function);
    const int status = lua_pcall(L, 0, 0, 0);
    if (status != LUA_OK || budget_exceeded) {
        fail(status);
        return false;
    }
    lua_settop(L, 0);
    return true;
}

}  // namespace

Script::Script() : impl_(new (std::nothrow) Impl) {}

Script::~Script() {
    if (impl_ != nullptr) {
        impl_->close();
        delete impl_;
    }
}

bool Script::load(const char* source, std::size_t length, ScriptWorld& world) {
    if (impl_ == nullptr || impl_->failed) return false;
    impl_->close();   // loading again starts from a fresh state
    impl_->world = &world;
    if (source == nullptr) {
        impl_->failWith("rules.lua: there is no script to load");
        return false;
    }
    if (!impl_->create()) {
        impl_->failWith("rules.lua: not enough memory to start the script");
        return false;
    }
    impl_->source = source;
    impl_->source_length = length;
    const bool ok = impl_->enter(run_load);
    impl_->source = nullptr;
    impl_->source_length = 0;
    return ok;
}

void Script::start() {
    if (impl_ != nullptr) impl_->enter(run_start);
}

void Script::tick() {
    if (impl_ != nullptr) impl_->enter(run_tick);
}

void Script::touch(const char* kind_a, const char* kind_b, ActorRef a, ActorRef b) {
    if (impl_ == nullptr || kind_a == nullptr || kind_b == nullptr) return;
    if (!wantsTouch(kind_a, kind_b)) return;
    impl_->touch_a = kind_a;
    impl_->touch_b = kind_b;
    impl_->touch_ref_a = a;
    impl_->touch_ref_b = b;
    impl_->enter(run_touch);
    impl_->touch_a = nullptr;
    impl_->touch_b = nullptr;
}

bool Script::wantsTouch(const char* kind_a, const char* kind_b) const {
    if (impl_ == nullptr || impl_->failed || kind_a == nullptr || kind_b == nullptr) return false;
    for (const TouchHandler& handler : impl_->touch_handlers)
        if (handler.a == kind_a && handler.b == kind_b) return true;
    return false;
}

bool Script::failed() const { return impl_ == nullptr || impl_->failed; }

const char* Script::error() const {
    if (impl_ == nullptr) return "rules.lua: not enough memory to start the script";
    return impl_->error;
}

}  // namespace tg
