#include "registry.h"

#include <algorithm>
#include <cstring>

namespace {

// Plain arrays, so they are zero-initialized before any game registers.
tg::GameInfo games[registry::MAX_GAMES];
std::uint8_t game_count;
registry::Error errors[registry::MAX_GAMES + 1];
std::uint8_t error_count;

bool validId(const char* id) {
    if (!id || id[0] < 'a' || id[0] > 'z' || std::strlen(id) > 15) return false;
    for (const char* p = id; *p; ++p) {
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') && *p != '_') return false;
    }
    return true;
}

bool validName(const char* name) {
    if (!name || !*name || std::strlen(name) > 13) return false;
    bool visible = false;
    for (const char* p = name; *p; ++p) {
        if (*p < 32 || *p > 126) return false;
        visible |= *p != ' ';
    }
    return visible;
}

}  // namespace

namespace registry {

bool add(const tg::GameInfo& info) {
    const char* reason = !validId(info.id) ? "invalid id"
                       : !validName(info.name) ? "invalid name"
                       : !info.create ? "missing factory"
                       : find(info.id) >= 0 ? "duplicate id"
                       : game_count == MAX_GAMES ? "32-game capacity reached" : nullptr;
    if (reason) {
        if (error_count < MAX_GAMES + 1) errors[error_count++] = {info.id, reason};
        return false;
    }
    games[game_count++] = info;
    return true;
}

std::uint8_t errorCount() { return error_count; }
const Error& error(std::uint8_t index) { return errors[index]; }

void sortByName() {
    std::sort(games, games + game_count, [](const tg::GameInfo& a, const tg::GameInfo& b) {
        return std::strcmp(a.name, b.name) < 0;
    });
}

std::uint8_t count() {
    return game_count;
}

const tg::GameInfo& get(std::uint8_t index) {
    return games[index];
}

int find(const char* id) {
    for (std::uint8_t i = 0; i < game_count; ++i) {
        if (std::strcmp(games[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

}  // namespace registry
