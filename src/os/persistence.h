#pragma once

#include "settings.h"

namespace persistence {

struct BestScore {
    std::uint32_t value = 0;

    template <typename Save>
    bool submit(std::uint32_t score, Save save) {
        if (score <= value || !save(score)) return false;
        value = score;
        return true;
    }
};

class SettingsSave {
public:
    void begin(const settings::Settings& initial) { saved_ = attempted_ = initial; }

    bool needed(const settings::Settings& current, bool editing) {
        const bool confirmed = was_editing_ && !editing;
        was_editing_ = editing;
        return !editing && current != saved_ && (current != attempted_ || confirmed);
    }

    void attempted(const settings::Settings& current, bool success) {
        attempted_ = current;
        if (success) saved_ = current;
    }

private:
    settings::Settings saved_;
    settings::Settings attempted_;
    bool was_editing_ = false;
};

}  // namespace persistence
