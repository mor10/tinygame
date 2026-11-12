#pragma once

#include <cstddef>
#include <string_view>

namespace tg::engine {

struct ManifestView {
    char id[16] = {};
    char name[14] = {};
    bool scroll = false;
};

bool parseManifest(std::string_view text, ManifestView& manifest);

}  // namespace tg::engine
