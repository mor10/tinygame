#pragma once

#include <cstdint>

#include "settings.h"

// Flash storage (ESP32 NVS via Arduino Preferences): console settings and one
// best score per game.
namespace storage {

// Returns stored settings (sanitized), or defaults when nothing valid is stored.
settings::Settings loadSettings();
bool saveSettings(const settings::Settings& values);

// Best score per game id (max 15 characters). 0 when none is stored.
std::uint32_t loadBest(const char* game_id);
bool saveBest(const char* game_id, std::uint32_t score);

}  // namespace storage
