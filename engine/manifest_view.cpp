#include "manifest_view.h"

#include <cstdint>
#include <cstring>

namespace tg::engine {
namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

bool appendByte(char* output, std::size_t capacity, std::size_t& length, char value) {
    if (output != nullptr) {
        if (length + 1 >= capacity) return false;
        output[length] = value;
    }
    ++length;
    return true;
}

bool appendCodepoint(char* output, std::size_t capacity, std::size_t& length,
                     std::uint32_t codepoint) {
    if (codepoint > 0x10ffffU || (codepoint >= 0xd800U && codepoint <= 0xdfffU)) return false;
    if (codepoint <= 0x7fU) {
        return appendByte(output, capacity, length, static_cast<char>(codepoint));
    }
    if (codepoint <= 0x7ffU) {
        return appendByte(output, capacity, length, static_cast<char>(0xc0U | (codepoint >> 6))) &&
               appendByte(output, capacity, length, static_cast<char>(0x80U | (codepoint & 0x3fU)));
    }
    if (codepoint <= 0xffffU) {
        return appendByte(output, capacity, length, static_cast<char>(0xe0U | (codepoint >> 12))) &&
               appendByte(output, capacity, length, static_cast<char>(0x80U | ((codepoint >> 6) & 0x3fU))) &&
               appendByte(output, capacity, length, static_cast<char>(0x80U | (codepoint & 0x3fU)));
    }
    return appendByte(output, capacity, length, static_cast<char>(0xf0U | (codepoint >> 18))) &&
           appendByte(output, capacity, length, static_cast<char>(0x80U | ((codepoint >> 12) & 0x3fU))) &&
           appendByte(output, capacity, length, static_cast<char>(0x80U | ((codepoint >> 6) & 0x3fU))) &&
           appendByte(output, capacity, length, static_cast<char>(0x80U | (codepoint & 0x3fU)));
}

bool hexDigit(char value, std::uint32_t& digit) {
    if (value >= '0' && value <= '9') digit = static_cast<std::uint32_t>(value - '0');
    else if (value >= 'a' && value <= 'f') digit = static_cast<std::uint32_t>(value - 'a') + 10U;
    else if (value >= 'A' && value <= 'F') digit = static_cast<std::uint32_t>(value - 'A') + 10U;
    else return false;
    return true;
}

bool parseString(std::string_view value, char* output, std::size_t capacity,
                 std::size_t& output_length) {
    output_length = 0;
    value = trim(value);
    if (value.empty() || (value.front() != '"' && value.front() != '\'')) return false;
    const char quote = value.front();
    value.remove_prefix(1);
    bool closed = false;
    while (!value.empty()) {
        const char current = value.front();
        value.remove_prefix(1);
        if (current == quote) {
            closed = true;
            break;
        }
        if (current == '\\' && quote == '"') {
            if (value.empty()) return false;
            const char escape = value.front();
            value.remove_prefix(1);
            if (escape == 'u' || escape == 'U') {
                const std::size_t digits = escape == 'u' ? 4 : 8;
                if (value.size() < digits) return false;
                std::uint32_t codepoint = 0;
                for (std::size_t i = 0; i < digits; ++i) {
                    std::uint32_t digit = 0;
                    if (!hexDigit(value[i], digit)) return false;
                    codepoint = (codepoint << 4) | digit;
                }
                value.remove_prefix(digits);
                if (!appendCodepoint(output, capacity, output_length, codepoint)) return false;
            } else {
                char decoded = '\0';
                switch (escape) {
                    case '"': decoded = '"'; break;
                    case '\\': decoded = '\\'; break;
                    case 'b': decoded = '\b'; break;
                    case 't': decoded = '\t'; break;
                    case 'n': decoded = '\n'; break;
                    case 'f': decoded = '\f'; break;
                    case 'r': decoded = '\r'; break;
                    default: return false;
                }
                if (!appendByte(output, capacity, output_length, decoded)) return false;
            }
        } else {
            if ((static_cast<unsigned char>(current) < 0x20U && current != '\t') ||
                !appendByte(output, capacity, output_length, current)) {
                return false;
            }
        }
    }
    value = trim(value);
    if (!closed || (!value.empty() && value.front() != '#')) return false;
    if (output != nullptr) output[output_length] = '\0';
    return true;
}

bool apiVersionOne(std::string_view value) {
    const std::size_t comment = value.find('#');
    value = trim(value.substr(0, comment));
    if (value == "1" || value == "+1") return true;
    if (value.size() < 3 || value[0] != '0') return false;
    const unsigned base = value[1] == 'x' ? 16 : value[1] == 'o' ? 8 :
                          value[1] == 'b' ? 2 : 0;
    if (base == 0) return false;
    value.remove_prefix(2);
    unsigned number = 0;
    bool previous_digit = false;
    for (const char character : value) {
        if (character == '_') {
            if (!previous_digit) return false;
            previous_digit = false;
            continue;
        }
        std::uint32_t digit = 0;
        if (!hexDigit(character, digit) || digit >= base || number > 1) return false;
        number = number * base + digit;
        previous_digit = true;
    }
    return previous_digit && number == 1;
}

bool validId(const char* id) {
    if (id[0] < 'a' || id[0] > 'z') return false;
    for (const char* p = id; *p; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            return false;
        }
    }
    return true;
}

bool validName(const char* name, std::size_t length) {
    if (length == 0 || length > 13) return false;
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char value = static_cast<unsigned char>(name[i]);
        if (value < 32U || value > 126U) return false;
    }
    return true;
}

}  // namespace

bool parseManifest(std::string_view text, ManifestView& manifest) {
    manifest = {};
    bool has_api = false;
    bool has_id = false;
    bool has_name = false;
    bool has_starter = false;
    bool has_view = false;
    bool root = true;
    std::size_t position = 0;
    while (position < text.size()) {
        const std::size_t end = text.find('\n', position);
        std::string_view line = text.substr(
            position, end == std::string_view::npos ? text.size() - position : end - position);
        position = end == std::string_view::npos ? text.size() : end + 1;
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        if (line.front() == '[') {
            const std::size_t comment = line.find('#');
            if (comment != std::string_view::npos) line = trim(line.substr(0, comment));
            if (line.size() < 3 || line.back() != ']' ||
                line.find(']') != line.size() - 1) return false;
            root = false;
            continue;
        }
        if (!root) continue;

        const std::size_t equal = line.find('=');
        if (equal == std::string_view::npos) return false;
        std::string_view key = trim(line.substr(0, equal));
        const std::string_view value = trim(line.substr(equal + 1));
        char decoded_key[32] = {};
        if (!key.empty() && (key.front() == '"' || key.front() == '\'')) {
            std::size_t length = 0;
            if (!parseString(key, nullptr, 0, length)) return false;
            if (length >= sizeof(decoded_key)) continue;
            if (!parseString(key, decoded_key, sizeof(decoded_key), length)) return false;
            key = std::string_view(decoded_key, length);
        }
        if (key == "api") {
            if (has_api || !apiVersionOne(value)) return false;
            has_api = true;
        } else if (key == "id") {
            std::size_t length = 0;
            if (has_id || !parseString(value, manifest.id, sizeof(manifest.id), length) ||
                std::strlen(manifest.id) != length || !validId(manifest.id)) return false;
            has_id = true;
        } else if (key == "name") {
            std::size_t length = 0;
            if (has_name || !parseString(value, manifest.name, sizeof(manifest.name), length) ||
                !validName(manifest.name, length)) return false;
            has_name = true;
        } else if (key == "starter") {
            std::size_t length = 0;
            if (has_starter || !parseString(value, nullptr, 0, length) || length == 0) return false;
            has_starter = true;
        } else if (key == "view") {
            char view[8] = {};
            std::size_t length = 0;
            if (has_view || !parseString(value, view, sizeof(view), length) ||
                (std::string_view(view, length) != "scroll" &&
                 std::string_view(view, length) != "fixed")) {
                return false;
            }
            manifest.scroll = std::strcmp(view, "scroll") == 0;
            has_view = true;
        }
    }
    return has_api && has_id && has_name && has_starter && has_view;
}

}  // namespace tg::engine
