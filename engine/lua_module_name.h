#pragma once

#include <string_view>

namespace tg::engine {

inline constexpr std::size_t MAX_LUA_MODULE_NAME_BYTES = 116;

inline bool validLuaModuleName(std::string_view name) {
    if (name.empty() || name.size() > MAX_LUA_MODULE_NAME_BYTES) return false;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const unsigned char value = static_cast<unsigned char>(name[i]);
        const bool letter = (value >= 'a' && value <= 'z') ||
                            (value >= 'A' && value <= 'Z');
        const bool digit = value >= '0' && value <= '9';
        if (!(letter || value == '_' || (i > 0 && digit))) return false;
    }
    return true;
}

}  // namespace tg::engine
