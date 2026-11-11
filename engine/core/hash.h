#pragma once

#include <cstddef>
#include <cstdint>

namespace tg {

// FNV-1a over 16-bit pixels; used to fingerprint a rendered frame.
inline std::uint64_t hashPixels(const std::uint16_t* pixels, std::size_t count) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (std::size_t i = 0; i < count; ++i) {
        hash = (hash ^ (pixels[i] & 0xFF)) * 0x100000001b3ULL;
        hash = (hash ^ (pixels[i] >> 8)) * 0x100000001b3ULL;
    }
    return hash;
}

}  // namespace tg
