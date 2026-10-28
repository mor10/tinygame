#pragma once

#include <cstdint>

// Global console settings, edited under System > Settings. Each item is a
// number stepped between min and max. Add its enum and SPECS row together.
namespace settings {

enum Item : std::uint8_t { BRIGHTNESS, LED, HANDEDNESS, ITEM_COUNT };

struct Spec {
    const char* name;
    std::uint8_t min;
    std::uint8_t max;
    std::uint8_t step;
    std::uint8_t fallback;  // default value
    const char* unit;
};

inline constexpr Spec SPECS[ITEM_COUNT] = {
    {"BRIGHTNESS", 10, 100, 10, 90, "%"},
    {"LED", 0, 100, 25, 50, "%"},
    {"HANDEDNESS", 0, 1, 1, 0, ""},  // 0 = right handed (default), 1 = left handed
};

struct Settings {
    std::uint8_t value[ITEM_COUNT] = {};

    // Every item starts at its SPECS default, so new items need no extra code here.
    constexpr Settings() {
        for (std::uint8_t i = 0; i < ITEM_COUNT; ++i) value[i] = SPECS[i].fallback;
    }

    std::uint8_t brightness() const { return value[BRIGHTNESS]; }
    std::uint8_t led() const { return value[LED]; }
    bool leftHanded() const { return value[HANDEDNESS] == 1; }
};

bool operator==(const Settings& a, const Settings& b);
inline bool operator!=(const Settings& a, const Settings& b) { return !(a == b); }

// Replaces any out-of-range value (e.g. from corrupt storage) with its default.
Settings sanitize(const Settings& settings);

// Values are edited as an index 0..valueCount-1 picked with the main encoder.
std::uint8_t valueCount(std::uint8_t item);
std::uint8_t valueIndex(const Settings& settings, std::uint8_t item);
void setValueIndex(Settings& settings, std::uint8_t item, std::uint8_t index);

}  // namespace settings
