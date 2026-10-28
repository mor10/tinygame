#pragma once

#include <cstdint>

#include "tg_types.h"

// All games compiled into the firmware. Each game adds itself with the
// TINYGAME macro before setup() runs, so there is no list to maintain.
namespace registry {

inline constexpr std::uint8_t MAX_GAMES = 32;

struct Error {
    const char* id;
    const char* reason;
};

bool add(const tg::GameInfo& info);
std::uint8_t errorCount();
const Error& error(std::uint8_t index);

// Sorts games by name for the launcher. Call once at boot.
void sortByName();
std::uint8_t count();
const tg::GameInfo& get(std::uint8_t index);
// Index of the game with this id, or -1.
int find(const char* id);

}  // namespace registry
