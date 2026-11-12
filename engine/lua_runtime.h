#pragma once

#include <cstddef>
#include <cstdint>

#include "draw_frame.h"
#include "lua_limits.h"
#include "lua_module_name.h"
#include "tg_types.h"

struct lua_State;
struct lua_Debug;

namespace tg::engine {

struct LuaModuleSource {
    const char* source = nullptr;
    std::size_t size = 0;
};

using LuaModuleResolver = bool (*)(void* context, const char* name,
                                   std::size_t name_size, LuaModuleSource& module);

class LuaRuntime {
public:
    static constexpr std::size_t MAX_HEAP_BYTES = 1024 * 1024;
    static constexpr std::size_t MAX_SOURCE_BYTES = MAX_LUA_SOURCE_BYTES;
    static constexpr std::size_t MAX_MODULE_NAME_BYTES = MAX_LUA_MODULE_NAME_BYTES;
    static constexpr std::uint32_t MAX_INSTRUCTIONS_PER_CALL = 100'000;
    static constexpr std::uint32_t MAX_NATIVE_LOOP_ITEMS = 4096;
    static constexpr std::uint32_t MAX_NATIVE_COPY_BYTES = 65'536;

    explicit LuaRuntime(std::uint32_t seed = 0x6d2b79f5);
    ~LuaRuntime();

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;

    bool load(const char* source, std::size_t size, const char* chunk_name = "game",
              LuaModuleResolver module_resolver = nullptr, void* module_context = nullptr);
    bool call(const char* function_name, float delta_seconds = 0.0f);
    bool callUpdate(float delta_seconds, const tg::Input& input);
    bool callUpdate(float delta_seconds, const tg::Input& input, const WorldSize& world);
    void reset();

    bool ready() const { return state_ != nullptr && loaded_ && !faulted_; }
    std::size_t heapUsed() const { return heap_used_; }
    const char* error() const { return error_; }
    const DrawFrame& frame() const { return frame_; }

private:
    static void* allocate(void* context, void* pointer, std::size_t old_size,
                          std::size_t new_size);
    static void instructionHook(lua_State* state, ::lua_Debug* debug);
    static int deterministicRandom(lua_State* state);
    static int cosmeticRandom(lua_State* state);
    static int randomFromStream(lua_State* state, std::uint32_t& rng_state);
    static int requireModule(lua_State* state);
    static int boundedStringRepeat(lua_State* state);
    static int boundedStringBytes(lua_State* state);
    static int boundedToNumber(lua_State* state);
    static int boundedArguments(lua_State* state);
    static int boundedTableUnpack(lua_State* state);
    static int boundedTableMove(lua_State* state);
    static int boundedTableInsert(lua_State* state);
    static int boundedTableRemove(lua_State* state);
    static int boundedTableConcat(lua_State* state);
    static int drawRectangle(lua_State* state);
    static int drawText(lua_State* state);
    static int openLibraries(lua_State* state);
    static int invokeCallback(lua_State* state);

    static LuaRuntime* fromState(lua_State* state);
    static bool numberArgument(lua_State* state, int index, float& value);
    void pushInput(const tg::Input& input, const WorldSize& world);
    bool create();
    bool protectedCall(int argument_count);
    void setError(const char* message);

    lua_State* state_ = nullptr;
    std::size_t heap_used_ = 0;
    const std::uint32_t initial_seed_;
    std::uint32_t rng_state_;
    std::uint32_t cosmetic_rng_state_;
    std::uint32_t instructions_remaining_ = 0;
    LuaModuleResolver module_resolver_ = nullptr;
    void* module_context_ = nullptr;
    bool loaded_ = false;
    bool faulted_ = false;
    DrawFrame frame_;
    char error_[192] = {};
};

}  // namespace tg::engine
