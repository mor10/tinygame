#include "lua_runtime.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tg_random.h"

extern "C" {
#include "lauxlib.h"
#include "lualib.h"
}

namespace tg::engine {
namespace {

constexpr int HOOK_INTERVAL = 100;
char module_cache_key;

struct CallbackInput {
    const char* name;
    float delta_seconds;
    const tg::Input* input;
    const WorldSize* world;
};

void removeGlobal(lua_State* state, const char* name) {
    lua_pushnil(state);
    lua_setglobal(state, name);
}

void removeField(lua_State* state, const char* table_name, const char* field_name) {
    lua_getglobal(state, table_name);
    lua_pushnil(state);
    lua_setfield(state, -2, field_name);
    lua_pop(state, 1);
}

void pushEncoder(lua_State* state, const tg::EncoderInput& input) {
    lua_createtable(state, 0, 6);
    lua_pushinteger(state, input.delta);
    lua_setfield(state, -2, "delta");
    lua_pushboolean(state, input.down);
    lua_setfield(state, -2, "down");
    lua_pushboolean(state, input.pressed);
    lua_setfield(state, -2, "pressed");
    lua_pushboolean(state, input.released);
    lua_setfield(state, -2, "released");
    lua_pushboolean(state, input.click);
    lua_setfield(state, -2, "click");
    lua_pushinteger(state, input.held_ms);
    lua_setfield(state, -2, "held_ms");
}

void pushAction(lua_State* state, const tg::Input& input) {
    lua_pushboolean(state, input.down);
    lua_setfield(state, -2, "down");
    lua_pushboolean(state, input.pressed);
    lua_setfield(state, -2, "pressed");
    lua_pushboolean(state, input.released);
    lua_setfield(state, -2, "released");
    lua_pushboolean(state, input.click);
    lua_setfield(state, -2, "click");
    lua_pushinteger(state, input.held_ms);
    lua_setfield(state, -2, "held_ms");
}

void pushVector(lua_State* state, const char* name, const tg::Vec3& vector) {
    lua_createtable(state, 0, 3);
    lua_pushnumber(state, vector.x);
    lua_setfield(state, -2, "x");
    lua_pushnumber(state, vector.y);
    lua_setfield(state, -2, "y");
    lua_pushnumber(state, vector.z);
    lua_setfield(state, -2, "z");
    lua_setfield(state, -2, name);
}

int callOriginal(lua_State* state, int results) {
    const int argument_count = lua_gettop(state);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_call(state, argument_count, results);
    return results == LUA_MULTRET ? lua_gettop(state) : results;
}

void wrapField(lua_State* state, const char* table_name, const char* name, lua_CFunction wrapper) {
    lua_getglobal(state, table_name);
    lua_getfield(state, -1, name);
    lua_pushcclosure(state, wrapper, 1);
    lua_setfield(state, -2, name);
    lua_pop(state, 1);
}

}  // namespace

LuaRuntime::LuaRuntime(std::uint32_t seed)
    : initial_seed_(seed == 0 ? 1 : seed), rng_state_(initial_seed_),
      cosmetic_rng_state_(initial_seed_ ^ 0x9e3779b9U) {}

LuaRuntime::~LuaRuntime() {
    reset();
}

void LuaRuntime::reset() {
    loaded_ = false;
    faulted_ = false;
    frame_.clear();
    rng_state_ = initial_seed_;
    cosmetic_rng_state_ = initial_seed_ ^ 0x9e3779b9U;
    error_[0] = '\0';
    if (state_ != nullptr) {
        lua_close(state_);
        state_ = nullptr;
    }
    heap_used_ = 0;
    module_resolver_ = nullptr;
    module_context_ = nullptr;
}

bool LuaRuntime::create() {
    state_ = lua_newstate(allocate, this);
    if (state_ == nullptr) {
        setError("Lua heap limit exceeded during startup");
        return false;
    }
    static_assert(LUA_EXTRASPACE >= sizeof(LuaRuntime*),
                  "Lua extra space must hold the runtime context");
    *static_cast<LuaRuntime**>(lua_getextraspace(state_)) = this;
    lua_pushcfunction(state_, openLibraries);
    if (!protectedCall(0)) {
        lua_close(state_);
        state_ = nullptr;
        heap_used_ = 0;
        return false;
    }
    return true;
}

bool LuaRuntime::load(const char* source, std::size_t size, const char* chunk_name,
                      LuaModuleResolver module_resolver, void* module_context) {
    reset();
    module_resolver_ = module_resolver;
    module_context_ = module_context;
    if (source == nullptr || chunk_name == nullptr || size > MAX_SOURCE_BYTES) {
        setError("Invalid or oversized Lua source");
        return false;
    }
    if (!create()) return false;

    if (luaL_loadbufferx(state_, source, size, chunk_name, "t") != LUA_OK) {
        const char* message = lua_tostring(state_, -1);
        setError(message == nullptr ? "Lua source could not be loaded" : message);
        lua_settop(state_, 0);
        faulted_ = true;
        return false;
    }
    if (!protectedCall(0)) {
        faulted_ = true;
        return false;
    }
    loaded_ = true;
    return true;
}

bool LuaRuntime::call(const char* function_name, float delta_seconds) {
    if (!ready()) {
        if (error_[0] == '\0') setError("Lua runtime is not ready");
        return false;
    }
    if (function_name == nullptr || !std::isfinite(delta_seconds) || delta_seconds < 0.0f) {
        setError("Lua runtime is not ready or the callback input is invalid");
        return false;
    }
    CallbackInput callback{function_name, delta_seconds, nullptr, nullptr};
    lua_pushcfunction(state_, invokeCallback);
    lua_pushlightuserdata(state_, &callback);
    return protectedCall(1);
}

bool LuaRuntime::callUpdate(float delta_seconds, const tg::Input& input) {
    return callUpdate(delta_seconds, input, WorldSize{});
}

bool LuaRuntime::callUpdate(float delta_seconds, const tg::Input& input,
                            const WorldSize& world) {
    if (!ready()) {
        if (error_[0] == '\0') setError("Lua runtime is not ready");
        return false;
    }
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f ||
        !std::isfinite(world.width) || !std::isfinite(world.height) ||
        world.width <= 0.0f || world.height <= 0.0f ||
        world.width > 1'000'000.0f || world.height > 1'000'000.0f) {
        setError("Lua update input or world size is invalid");
        frame_.clear();
        faulted_ = true;
        return false;
    }
    frame_.clear();
    CallbackInput callback{"update", delta_seconds, &input, &world};
    lua_pushcfunction(state_, invokeCallback);
    lua_pushlightuserdata(state_, &callback);
    if (protectedCall(1)) return true;
    frame_.clear();
    return false;
}

int LuaRuntime::invokeCallback(lua_State* state) {
    const auto* callback = static_cast<const CallbackInput*>(lua_touserdata(state, 1));
    lua_getglobal(state, callback->name);
    if (!lua_isfunction(state, -1)) return luaL_error(state, "Lua callback is missing");
    lua_pushnumber(state, callback->delta_seconds);
    if (callback->input != nullptr) {
        fromState(state)->pushInput(*callback->input, *callback->world);
        lua_call(state, 2, 0);
    } else {
        lua_call(state, 1, 0);
    }
    return 0;
}

void LuaRuntime::pushInput(const tg::Input& input, const WorldSize& world) {
    lua_createtable(state_, 0, 14);
    pushEncoder(state_, input.main);
    lua_setfield(state_, -2, "main");
    pushEncoder(state_, input.left);
    lua_setfield(state_, -2, "left");
    pushEncoder(state_, input.right);
    lua_setfield(state_, -2, "right");
    pushAction(state_, input);
    lua_pushnumber(state_, input.tilt_x);
    lua_setfield(state_, -2, "tilt_x");
    lua_pushnumber(state_, input.tilt_y);
    lua_setfield(state_, -2, "tilt_y");
    pushVector(state_, "accel_g", input.accel_g);
    pushVector(state_, "gyro_dps", input.gyro_dps);
    lua_createtable(state_, 0, 2);
    lua_pushnumber(state_, world.width);
    lua_setfield(state_, -2, "width");
    lua_pushnumber(state_, world.height);
    lua_setfield(state_, -2, "height");
    lua_setfield(state_, -2, "screen");
}

bool LuaRuntime::protectedCall(int argument_count) {
    instructions_remaining_ = MAX_INSTRUCTIONS_PER_CALL;
    lua_sethook(state_, instructionHook, LUA_MASKCOUNT, HOOK_INTERVAL);
    const int status = lua_pcall(state_, argument_count, 0, 0);
    lua_sethook(state_, nullptr, 0, 0);
    if (status == LUA_OK) return true;

    const char* message = lua_tostring(state_, -1);
    setError(message == nullptr ? "Lua callback failed" : message);
    lua_settop(state_, 0);
    faulted_ = true;
    return false;
}

void* LuaRuntime::allocate(void* context, void* pointer, std::size_t old_size,
                           std::size_t new_size) {
    auto* runtime = static_cast<LuaRuntime*>(context);
    const std::size_t previous = pointer == nullptr ? 0 : old_size;
    if (new_size == 0) {
        std::free(pointer);
        runtime->heap_used_ -= std::min(runtime->heap_used_, previous);
        return nullptr;
    }
    if (previous > runtime->heap_used_ ||
        new_size > MAX_HEAP_BYTES - (runtime->heap_used_ - previous)) {
        return nullptr;
    }
    void* resized = std::realloc(pointer, new_size);
    if (resized != nullptr) runtime->heap_used_ = runtime->heap_used_ - previous + new_size;
    return resized;
}

void LuaRuntime::instructionHook(lua_State* state, lua_Debug*) {
    auto* runtime = *static_cast<LuaRuntime**>(lua_getextraspace(state));
    if (runtime->instructions_remaining_ <= HOOK_INTERVAL) {
        runtime->instructions_remaining_ = 0;
        luaL_error(state, "instruction limit exceeded");
        return;
    }
    runtime->instructions_remaining_ -= HOOK_INTERVAL;
}

int LuaRuntime::deterministicRandom(lua_State* state) {
    return randomFromStream(state, fromState(state)->rng_state_);
}

int LuaRuntime::cosmeticRandom(lua_State* state) {
    return randomFromStream(state, fromState(state)->cosmetic_rng_state_);
}

int LuaRuntime::randomFromStream(lua_State* state, std::uint32_t& rng_state) {
    const int arguments = lua_gettop(state);
    if (arguments > 2) return luaL_error(state, "random accepts at most two bounds");
    const std::uint32_t value = tg::randomNext(rng_state);

    if (arguments == 0) {
        lua_pushnumber(state, static_cast<lua_Number>(value >> 8) / 16777216.0);
        return 1;
    }

    lua_Integer lower = 1;
    lua_Integer upper = luaL_checkinteger(state, 1);
    if (arguments > 1) {
        lower = upper;
        upper = luaL_checkinteger(state, 2);
    }
    if (lower > upper) return luaL_error(state, "invalid random interval");

    const std::uint64_t distance = static_cast<std::uint64_t>(upper) -
                                   static_cast<std::uint64_t>(lower);
    if (distance >= 0x1'0000'0000ULL) {
        return luaL_error(state, "random interval exceeds the generator range");
    }
    const std::uint64_t range = distance + 1;
    const std::uint64_t offset = static_cast<std::uint64_t>(value) % range;
    lua_pushinteger(state, static_cast<lua_Integer>(lower + static_cast<lua_Integer>(offset)));
    return 1;
}

int LuaRuntime::requireModule(lua_State* state) {
    auto* runtime = fromState(state);
    if (runtime == nullptr || runtime->module_resolver_ == nullptr) {
        return luaL_error(state, "bundle-local modules are unavailable");
    }
    if (lua_gettop(state) != 1 || lua_type(state, 1) != LUA_TSTRING) {
        return luaL_error(state, "require expects one module name");
    }

    std::size_t name_size = 0;
    const char* name = lua_tolstring(state, 1, &name_size);
    if (!validLuaModuleName(std::string_view(name, name_size))) {
        return luaL_error(state, "invalid bundle module name");
    }

    lua_rawgetp(state, LUA_REGISTRYINDEX, &module_cache_key);
    const int cache = lua_gettop(state);
    lua_pushlstring(state, name, name_size);
    lua_rawget(state, cache);
    if (!lua_isnil(state, -1)) {
        lua_remove(state, cache);
        return 1;
    }
    lua_pop(state, 1);

    lua_pushlstring(state, name, name_size);
    lua_pushboolean(state, 1);
    lua_rawset(state, cache);

    LuaModuleSource module;
    if (!runtime->module_resolver_(runtime->module_context_, name, name_size, module) ||
        module.source == nullptr || module.size > MAX_SOURCE_BYTES) {
        lua_pushlstring(state, name, name_size);
        lua_pushnil(state);
        lua_rawset(state, cache);
        lua_pop(state, 1);
        return luaL_error(state, "bundle module not found or too large: %s", name);
    }
    if (luaL_loadbufferx(state, module.source, module.size, name, "t") != LUA_OK) {
        lua_pushlstring(state, name, name_size);
        lua_pushnil(state);
        lua_rawset(state, cache);
        lua_remove(state, cache);
        return lua_error(state);
    }
    if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
        lua_pushlstring(state, name, name_size);
        lua_pushnil(state);
        lua_rawset(state, cache);
        lua_remove(state, cache);
        return lua_error(state);
    }
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_pushboolean(state, 1);
    }
    lua_pushlstring(state, name, name_size);
    lua_pushvalue(state, -2);
    lua_rawset(state, cache);
    lua_remove(state, cache);
    return 1;
}

int LuaRuntime::boundedStringRepeat(lua_State* state) {
    std::size_t source_size = 0;
    std::size_t separator_size = 0;
    luaL_checklstring(state, 1, &source_size);
    const lua_Integer count = luaL_checkinteger(state, 2);
    luaL_optlstring(state, 3, "", &separator_size);
    const auto work = count > 0 ? static_cast<std::uint64_t>(count) : 0;
    auto* runtime = fromState(state);
    if (work > MAX_NATIVE_LOOP_ITEMS || work > runtime->instructions_remaining_) {
        return luaL_error(state, "string.rep native work limit exceeded");
    }
    runtime->instructions_remaining_ -= static_cast<std::uint32_t>(work);
    const std::uint64_t limit =
        std::min(MAX_NATIVE_COPY_BYTES, runtime->instructions_remaining_);
    if (work > 0) {
        if (source_size > limit / work) {
            return luaL_error(state, "string.rep native byte limit exceeded");
        }
        std::uint64_t bytes = source_size * work;
        if (work > 1) {
            if (separator_size > (limit - bytes) / (work - 1)) {
                return luaL_error(state, "string.rep native byte limit exceeded");
            }
            bytes += separator_size * (work - 1);
        }
        runtime->instructions_remaining_ -= static_cast<std::uint32_t>(bytes);
    }
    return callOriginal(state, 1);
}

int LuaRuntime::boundedStringBytes(lua_State* state) {
    std::size_t size = 0;
    luaL_checklstring(state, 1, &size);
    auto* runtime = fromState(state);
    if (size > MAX_NATIVE_COPY_BYTES || size > runtime->instructions_remaining_) {
        return luaL_error(state, "string native byte limit exceeded");
    }
    runtime->instructions_remaining_ -= static_cast<std::uint32_t>(size);
    return callOriginal(state, LUA_MULTRET);
}

int LuaRuntime::boundedToNumber(lua_State* state) {
    auto* runtime = fromState(state);
    for (int index = 1; index <= 2; ++index) {
        if (lua_type(state, index) != LUA_TSTRING) continue;
        std::size_t size = 0;
        lua_tolstring(state, index, &size);
        if (size > MAX_NATIVE_COPY_BYTES || size > runtime->instructions_remaining_) {
            return luaL_error(state, "tonumber native byte limit exceeded");
        }
        runtime->instructions_remaining_ -= static_cast<std::uint32_t>(size);
    }
    return callOriginal(state, 1);
}

int LuaRuntime::boundedArguments(lua_State* state) {
    const auto count = static_cast<std::uint32_t>(lua_gettop(state));
    auto* runtime = fromState(state);
    if (count > MAX_NATIVE_LOOP_ITEMS || count > runtime->instructions_remaining_) {
        return luaL_error(state, "native argument work limit exceeded");
    }
    runtime->instructions_remaining_ -= count;
    return callOriginal(state, LUA_MULTRET);
}

int LuaRuntime::boundedTableUnpack(lua_State* state) {
    if (lua_type(state, 1) != LUA_TTABLE) return callOriginal(state, LUA_MULTRET);
    const auto raw_length = lua_rawlen(state, 1);
    if (static_cast<std::uint64_t>(raw_length) >= static_cast<std::uint64_t>(LUA_MAXINTEGER)) {
        return luaL_error(state, "table length exceeds native work limit");
    }
    const lua_Integer first = luaL_optinteger(state, 2, 1);
    const lua_Integer last = luaL_optinteger(state, 3, static_cast<lua_Integer>(raw_length));
    const std::uint64_t distance = last >= first
        ? static_cast<std::uint64_t>(last) - static_cast<std::uint64_t>(first) : 0;
    auto* runtime = fromState(state);
    if (last >= first &&
        (distance >= MAX_NATIVE_LOOP_ITEMS || distance >= runtime->instructions_remaining_)) {
        return luaL_error(state, "table.unpack native work limit exceeded");
    }
    if (last >= first) runtime->instructions_remaining_ -= static_cast<std::uint32_t>(distance + 1);
    return callOriginal(state, LUA_MULTRET);
}

int LuaRuntime::boundedTableMove(lua_State* state) {
    const lua_Integer first = luaL_checkinteger(state, 2);
    const lua_Integer last = luaL_checkinteger(state, 3);
    const std::uint64_t distance = last >= first
        ? static_cast<std::uint64_t>(last) - static_cast<std::uint64_t>(first)
        : 0;
    auto* runtime = fromState(state);
    if (last >= first &&
        (distance >= MAX_NATIVE_LOOP_ITEMS || distance >= runtime->instructions_remaining_)) {
        return luaL_error(state, "table.move native work limit exceeded");
    }
    if (last >= first) runtime->instructions_remaining_ -= static_cast<std::uint32_t>(distance + 1);
    return callOriginal(state, 1);
}

int LuaRuntime::boundedTableInsert(lua_State* state) {
    if (lua_type(state, 1) != LUA_TTABLE ||
        (lua_gettop(state) != 2 && lua_gettop(state) != 3)) {
        return callOriginal(state, 0);
    }
    const auto raw_length = lua_rawlen(state, 1);
    if (static_cast<std::uint64_t>(raw_length) >= static_cast<std::uint64_t>(LUA_MAXINTEGER)) {
        return luaL_error(state, "table length exceeds native work limit");
    }
    const auto length = static_cast<lua_Integer>(raw_length);
    const lua_Integer position = lua_gettop(state) == 2 ? length + 1
                                                        : luaL_checkinteger(state, 2);
    if (position < 1 || position > length + 1) return callOriginal(state, 0);
    const auto work = static_cast<std::uint64_t>(length - position + 2);
    auto* runtime = fromState(state);
    if (work > MAX_NATIVE_LOOP_ITEMS || work > runtime->instructions_remaining_) {
        return luaL_error(state, "table.insert native work limit exceeded");
    }
    runtime->instructions_remaining_ -= static_cast<std::uint32_t>(work);
    return callOriginal(state, 0);
}

int LuaRuntime::boundedTableRemove(lua_State* state) {
    if (lua_type(state, 1) != LUA_TTABLE) return callOriginal(state, 1);
    const auto raw_length = lua_rawlen(state, 1);
    if (static_cast<std::uint64_t>(raw_length) >= static_cast<std::uint64_t>(LUA_MAXINTEGER)) {
        return luaL_error(state, "table length exceeds native work limit");
    }
    const auto length = static_cast<lua_Integer>(raw_length);
    const lua_Integer position = luaL_optinteger(state, 2, length);
    if (position < 0 || (position == 0 && length != 0) || position > length + 1) {
        return callOriginal(state, 1);
    }
    const auto work = position < length
        ? static_cast<std::uint64_t>(length - position + 1) : 1;
    auto* runtime = fromState(state);
    if (work > MAX_NATIVE_LOOP_ITEMS || work > runtime->instructions_remaining_) {
        return luaL_error(state, "table.remove native work limit exceeded");
    }
    runtime->instructions_remaining_ -= static_cast<std::uint32_t>(work);
    return callOriginal(state, 1);
}

int LuaRuntime::boundedTableConcat(lua_State* state) {
    if (lua_type(state, 1) != LUA_TTABLE) return callOriginal(state, 1);
    const auto raw_length = lua_rawlen(state, 1);
    if (static_cast<std::uint64_t>(raw_length) >= static_cast<std::uint64_t>(LUA_MAXINTEGER)) {
        return luaL_error(state, "table length exceeds native work limit");
    }
    const auto length = static_cast<lua_Integer>(raw_length);
    std::size_t separator_size = 0;
    luaL_optlstring(state, 2, "", &separator_size);
    const lua_Integer first = luaL_optinteger(state, 3, 1);
    const lua_Integer last = luaL_optinteger(state, 4, length);
    const std::uint64_t distance = last >= first
        ? static_cast<std::uint64_t>(last) - static_cast<std::uint64_t>(first) : 0;
    auto* runtime = fromState(state);
    if (last >= first && distance >= MAX_NATIVE_LOOP_ITEMS) {
        return luaL_error(state, "table.concat native work limit exceeded");
    }
    const std::uint64_t items = last >= first ? distance + 1 : 0;
    if (items * 2 > runtime->instructions_remaining_) {
        return luaL_error(state, "table.concat native work limit exceeded");
    }
    runtime->instructions_remaining_ -= static_cast<std::uint32_t>(items * 2);
    const std::uint64_t limit =
        std::min(MAX_NATIVE_COPY_BYTES, runtime->instructions_remaining_);
    if (items > 0) {
        if (distance > 0 && separator_size > limit / distance) {
            return luaL_error(state, "table.concat native byte limit exceeded");
        }
        std::uint64_t bytes = separator_size * distance;
        for (std::uint64_t offset = 0; offset < items; ++offset) {
            lua_rawgeti(state, 1, first + static_cast<lua_Integer>(offset));
            if (!lua_isstring(state, -1)) {
                lua_pop(state, 1);
                return callOriginal(state, 1);
            }
            std::size_t size = 0;
            lua_tolstring(state, -1, &size);
            lua_pop(state, 1);
            if (size > limit - bytes) {
                return luaL_error(state, "table.concat native byte limit exceeded");
            }
            bytes += size;
        }
        runtime->instructions_remaining_ -= static_cast<std::uint32_t>(bytes);
    }
    return callOriginal(state, 1);
}

int LuaRuntime::drawRectangle(lua_State* state) {
    auto* runtime = fromState(state);
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    if (!numberArgument(state, 1, x) || !numberArgument(state, 2, y) ||
        !numberArgument(state, 3, width) || !numberArgument(state, 4, height) ||
        !lua_isinteger(state, 5)) {
        return luaL_error(state, "tg.rect expects finite coordinates and a palette index");
    }
    const lua_Integer palette_index = lua_tointeger(state, 5);
    if (palette_index < 0 || palette_index >= 16 ||
        !runtime->frame_.rectangle(x, y, width, height,
                                   static_cast<std::uint8_t>(palette_index))) {
        return luaL_error(state, "tg.rect command is invalid or the draw limit was reached");
    }
    return 0;
}

int LuaRuntime::drawText(lua_State* state) {
    auto* runtime = fromState(state);
    float x = 0.0f;
    float y = 0.0f;
    if (!numberArgument(state, 1, x) || !numberArgument(state, 2, y) ||
        lua_type(state, 3) != LUA_TSTRING || !lua_isinteger(state, 4)) {
        return luaL_error(state, "tg.text expects coordinates, ASCII text, and a palette index");
    }
    std::size_t size = 0;
    const char* value = lua_tolstring(state, 3, &size);
    const lua_Integer palette_index = lua_tointeger(state, 4);
    if (palette_index < 0 || palette_index >= 16 ||
        !runtime->frame_.text(x, y, value, size,
                              static_cast<std::uint8_t>(palette_index))) {
        return luaL_error(state, "tg.text command is invalid or the draw limit was reached");
    }
    return 0;
}

int LuaRuntime::openLibraries(lua_State* state) {
    luaL_requiref(state, LUA_GNAME, luaopen_base, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state, 1);

    constexpr const char* forbidden_globals[] = {
        "collectgarbage", "dofile", "load", "loadfile", "next", "pairs",
        "pcall", "print", "rawset", "setmetatable", "getmetatable", "warn",
        "xpcall",
    };
    for (const char* name : forbidden_globals) removeGlobal(state, name);
    lua_getglobal(state, "tonumber");
    lua_pushcclosure(state, boundedToNumber, 1);
    lua_setglobal(state, "tonumber");

    removeField(state, LUA_MATHLIBNAME, "randomseed");
    lua_getglobal(state, LUA_MATHLIBNAME);
    lua_pushcfunction(state, deterministicRandom);
    lua_setfield(state, -2, "random");
    lua_pop(state, 1);

    constexpr const char* restricted_string_functions[] = {
        "dump", "find", "format", "gmatch", "gsub", "match",
        "pack", "packsize", "unpack",
    };
    for (const char* name : restricted_string_functions) removeField(state, LUA_STRLIBNAME, name);
    removeField(state, LUA_TABLIBNAME, "sort");

    wrapField(state, LUA_STRLIBNAME, "rep", boundedStringRepeat);
    for (const char* name : {"byte", "lower", "reverse", "sub", "upper"}) {
        wrapField(state, LUA_STRLIBNAME, name, boundedStringBytes);
    }
    wrapField(state, LUA_STRLIBNAME, "char", boundedArguments);
    wrapField(state, LUA_TABLIBNAME, "pack", boundedArguments);
    wrapField(state, LUA_TABLIBNAME, "unpack", boundedTableUnpack);
    wrapField(state, LUA_TABLIBNAME, "move", boundedTableMove);
    wrapField(state, LUA_TABLIBNAME, "insert", boundedTableInsert);
    wrapField(state, LUA_TABLIBNAME, "remove", boundedTableRemove);
    wrapField(state, LUA_TABLIBNAME, "concat", boundedTableConcat);

    lua_createtable(state, 0, 3);
    lua_pushcfunction(state, drawRectangle);
    lua_setfield(state, -2, "rect");
    lua_pushcfunction(state, drawText);
    lua_setfield(state, -2, "text");
    lua_pushcfunction(state, cosmeticRandom);
    lua_setfield(state, -2, "cosmetic_random");
    lua_setglobal(state, "tg");

    if (fromState(state)->module_resolver_ != nullptr) {
        lua_newtable(state);
        lua_rawsetp(state, LUA_REGISTRYINDEX, &module_cache_key);
        lua_pushcfunction(state, requireModule);
        lua_setglobal(state, "require");
    }
    return 0;
}

LuaRuntime* LuaRuntime::fromState(lua_State* state) {
    return *static_cast<LuaRuntime**>(lua_getextraspace(state));
}

bool LuaRuntime::numberArgument(lua_State* state, int index, float& value) {
    if (lua_type(state, index) != LUA_TNUMBER) return false;
    const lua_Number number = lua_tonumber(state, index);
    if (!std::isfinite(number) || number < -1'000'000.0 || number > 1'000'000.0) {
        return false;
    }
    value = static_cast<float>(number);
    return std::isfinite(value);
}

void LuaRuntime::setError(const char* message) {
    if (message == nullptr) message = "Lua runtime error";
    std::snprintf(error_, sizeof(error_), "%s", message);
}

}  // namespace tg::engine
