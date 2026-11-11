// Carried over from v1.0 src/hal/storage.cpp.
// Unchanged apart from this comment.

#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

constexpr const char* SETTINGS_NAMESPACE = "tinygame";
constexpr const char* BEST_NAMESPACE = "tg_best";
// Version 3 appends auto-off settings; version 2's original bytes still migrate.
constexpr std::uint8_t SCHEMA_VERSION = 3;
constexpr const char* KEY_VERSION = "ver";
constexpr const char* KEY_SETTINGS = "settings";

// Read-only open fails until a namespace exists (first boot); the core then
// logs a harmless "nvs_open failed: NOT_FOUND".
bool openReadOnly(Preferences& prefs, const char* name) {
    return prefs.begin(name, true);
}

}  // namespace

namespace storage {

settings::Settings loadSettings() {
    settings::Settings result;
    Preferences prefs;
    if (!openReadOnly(prefs, SETTINGS_NAMESPACE)) {
        return result;
    }
    const auto version = prefs.getUChar(KEY_VERSION, 0);
    const std::size_t size = version == 2 ? 3 : sizeof(result.value);
    if ((version == 2 || version == SCHEMA_VERSION) &&
        prefs.getBytesLength(KEY_SETTINGS) == size) {
        prefs.getBytes(KEY_SETTINGS, result.value, size);
    }
    prefs.end();
    return settings::sanitize(result);
}

bool saveSettings(const settings::Settings& values) {
    Preferences prefs;
    if (!prefs.begin(SETTINGS_NAMESPACE, false)) {
        return false;
    }
    const bool ok = prefs.putBytes(KEY_SETTINGS, values.value, sizeof(values.value)) ==
                        sizeof(values.value) &&
                    prefs.putUChar(KEY_VERSION, SCHEMA_VERSION) == 1;
    prefs.end();
    return ok;
}

std::uint32_t loadBest(const char* game_id) {
    Preferences prefs;
    if (!openReadOnly(prefs, BEST_NAMESPACE)) {
        return 0;
    }
    const std::uint32_t best = prefs.getULong(game_id, 0);
    prefs.end();
    return best;
}

bool saveBest(const char* game_id, std::uint32_t score) {
    Preferences prefs;
    if (!prefs.begin(BEST_NAMESPACE, false)) {
        return false;
    }
    const bool ok = prefs.putULong(game_id, score) == sizeof(std::uint32_t);
    prefs.end();
    return ok;
}

}  // namespace storage
