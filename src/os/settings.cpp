#include "settings.h"

namespace settings {

std::uint8_t valueCount(std::uint8_t item) {
    if (item >= ITEM_COUNT) {
        return 0;
    }
    const Spec& s = SPECS[item];
    return static_cast<std::uint8_t>((s.max - s.min) / s.step + 1);
}

std::uint8_t valueIndex(const Settings& settings, std::uint8_t item) {
    if (item >= ITEM_COUNT) {
        return 0;
    }
    const Spec& s = SPECS[item];
    return static_cast<std::uint8_t>((settings.value[item] - s.min) / s.step);
}

void setValueIndex(Settings& settings, std::uint8_t item, std::uint8_t index) {
    const std::uint8_t count = valueCount(item);
    if (count == 0) {
        return;
    }
    if (index >= count) {
        index = static_cast<std::uint8_t>(count - 1);
    }
    const Spec& s = SPECS[item];
    settings.value[item] = static_cast<std::uint8_t>(s.min + index * s.step);
}

bool operator==(const Settings& a, const Settings& b) {
    for (std::uint8_t i = 0; i < ITEM_COUNT; ++i) {
        if (a.value[i] != b.value[i]) {
            return false;
        }
    }
    return true;
}

Settings sanitize(const Settings& settings) {
    Settings result = settings;
    for (std::uint8_t i = 0; i < ITEM_COUNT; ++i) {
        const Spec& s = SPECS[i];
        const std::uint8_t v = result.value[i];
        if (v < s.min || v > s.max || (v - s.min) % s.step != 0) {
            result.value[i] = s.fallback;
        }
    }
    return result;
}

}  // namespace settings
