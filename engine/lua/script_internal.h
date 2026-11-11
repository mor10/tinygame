#pragma once

// Private to the Lua host and its tests. Not part of the engine's interface.

#include <cstddef>

namespace tg::lua_internal {

// Bytes currently handed out to Lua states by every Script in the process.
// Returns to zero when every Script has been destroyed (or has failed, which
// frees its Lua state). Not thread-safe: scripts run on the game thread.
std::size_t liveBytes();

// Bytes currently held by the Lua state of the script that most recently
// allocated or freed. Handy for seeing how much one small script costs.
std::size_t lastStateBytes();

}  // namespace tg::lua_internal
