#include "storage.h"

#include "Arduino.h"
#include <cstdio>
#include <emscripten.h>

namespace {
EM_JS(int, loadBytes, (const char* key, std::uint8_t* out, int size), {
    const name = UTF8ToString(key);
    try {
        const stored = localStorage.getItem(name);
        if (stored === null) return 0;
        const data = JSON.parse(stored);
        if (!Array.isArray(data) || data.length !== size ||
            data.some(x => !Number.isInteger(x) || x < 0 || x > 255)) {
            if (Module.onSerial) Module.onSerial("[STORAGE] error loading " + name + ": invalid byte array\n");
            return 0;
        }
        HEAPU8.set(data, out);
        return 1;
    } catch (error) {
        if (Module.onSerial) Module.onSerial("[STORAGE] error loading " + name + ": " + error.name + "\n");
        return 0;
    }
});

EM_JS(int, saveBytes, (const char* key, const std::uint8_t* data, int size), {
    const name = UTF8ToString(key);
    try {
        localStorage.setItem(name,
            JSON.stringify(Array.from(HEAPU8.subarray(data, data + size))));
        return 1;
    } catch (error) {
        if (Module.onSerial) Module.onSerial("[STORAGE] error saving " + name + ": " + error.name + "\n");
        return 0;
    }
});

bool bestKey(const char* id, char* key, std::size_t size) {
    if (!id || !*id) return false;
    const int length = std::snprintf(key, size, "tinygame.sim.best.%s", id);
    return length > 0 && static_cast<std::size_t>(length) < size;
}
}  // namespace

namespace storage {
settings::Settings loadSettings() {
    settings::Settings result;
    const bool loaded = loadBytes("tinygame.sim.settings.v2", result.value, sizeof(result.value));
    const auto valid = settings::sanitize(result);
    if (loaded && valid != result) {
        Serial.println("[STORAGE] error loading tinygame.sim.settings.v2: values out of range");
    }
    return valid;
}
bool saveSettings(const settings::Settings& values) {
    const auto valid = settings::sanitize(values);
    return saveBytes("tinygame.sim.settings.v2", valid.value, sizeof(valid.value)) != 0;
}
std::uint32_t loadBest(const char* game_id) {
    char key[64];
    std::uint32_t score = 0;
    if (bestKey(game_id, key, sizeof(key))) {
        loadBytes(key, reinterpret_cast<std::uint8_t*>(&score), sizeof(score));
    }
    return score;
}
bool saveBest(const char* game_id, std::uint32_t score) {
    char key[64];
    return bestKey(game_id, key, sizeof(key)) &&
           saveBytes(key, reinterpret_cast<const std::uint8_t*>(&score), sizeof(score));
}
}  // namespace storage
