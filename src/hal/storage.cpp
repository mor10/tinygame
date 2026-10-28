#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

constexpr const char* SETTINGS_NAMESPACE = "tinygame";
constexpr const char* BEST_NAMESPACE = "tg_best";
// Bump when the stored settings layout changes; older data is then ignored.
constexpr std::uint8_t SCHEMA_VERSION = 2;
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
    if (prefs.getUChar(KEY_VERSION, 0) == SCHEMA_VERSION &&
        prefs.getBytesLength(KEY_SETTINGS) == sizeof(result.value)) {
        prefs.getBytes(KEY_SETTINGS, result.value, sizeof(result.value));
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
