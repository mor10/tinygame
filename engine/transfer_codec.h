#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tg::engine::transfer {

inline constexpr std::size_t CHUNK_BYTES = 768;

inline bool parseNumber(const char* text, std::uint32_t& value, unsigned base) {
    if (text == nullptr || *text == '\0' || (base != 10 && base != 16)) return false;
    std::uint32_t parsed = 0;
    for (const char* p = text; *p; ++p) {
        unsigned digit;
        if (*p >= '0' && *p <= '9') digit = static_cast<unsigned>(*p - '0');
        else if (base == 16 && *p >= 'a' && *p <= 'f')
            digit = static_cast<unsigned>(*p - 'a') + 10U;
        else if (base == 16 && *p >= 'A' && *p <= 'F')
            digit = static_cast<unsigned>(*p - 'A') + 10U;
        else
            return false;
        if (digit >= base || parsed > (UINT32_MAX - digit) / base) return false;
        parsed = parsed * base + digit;
    }
    value = parsed;
    return true;
}

inline int base64Value(char character) {
    if (character >= 'A' && character <= 'Z') return character - 'A';
    if (character >= 'a' && character <= 'z') return character - 'a' + 26;
    if (character >= '0' && character <= '9') return character - '0' + 52;
    if (character == '+') return 62;
    if (character == '/') return 63;
    return -1;
}

inline bool decodeBase64(const char* encoded, std::uint8_t* output,
                         std::size_t capacity, std::size_t& output_size) {
    output_size = 0;
    if (encoded == nullptr || output == nullptr) return false;
    const std::size_t length = std::strlen(encoded);
    if (length == 0 || length % 4 != 0 ||
        length > ((CHUNK_BYTES + 2) / 3) * 4) {
        return false;
    }
    for (std::size_t i = 0; i < length; i += 4) {
        const bool last = i + 4 == length;
        const int a = base64Value(encoded[i]);
        const int b = base64Value(encoded[i + 1]);
        const bool pad2 = encoded[i + 2] == '=';
        const bool pad3 = encoded[i + 3] == '=';
        const int c = pad2 ? 0 : base64Value(encoded[i + 2]);
        const int d = pad3 ? 0 : base64Value(encoded[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0 || (pad2 && !pad3) ||
            (!last && (pad2 || pad3)) ||
            (pad2 && (b & 0x0f) != 0) || (!pad2 && pad3 && (c & 0x03) != 0)) {
            return false;
        }
        const std::size_t needed = 1U + static_cast<std::size_t>(!pad2) +
                                   static_cast<std::size_t>(!pad3);
        if (needed > capacity - output_size) return false;
        output[output_size++] = static_cast<std::uint8_t>((a << 2) | (b >> 4));
        if (!pad2) output[output_size++] = static_cast<std::uint8_t>((b << 4) | (c >> 2));
        if (!pad3) output[output_size++] = static_cast<std::uint8_t>((c << 6) | d);
    }
    return output_size > 0 && output_size <= CHUNK_BYTES;
}

}  // namespace tg::engine::transfer
