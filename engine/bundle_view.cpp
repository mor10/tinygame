#include "bundle_view.h"

#include <algorithm>
#include <array>

#include "crc32.h"
#include "lua_limits.h"
#include "lua_module_name.h"

namespace tg::engine {
namespace {

constexpr std::size_t HEADER_SIZE = 40;
constexpr std::size_t ENTRY_SIZE = 16;
constexpr std::uint16_t FORMAT_VERSION = 1;
constexpr std::array<std::uint8_t, 4> MAGIC = {'T', 'G', 'B', 0};

std::uint16_t read16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t read32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::size_t align4(std::size_t value) {
    return (value + 3U) & ~std::size_t{3U};
}

bool zeroFilled(const std::uint8_t* bytes, std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
        if (bytes[i] != 0) return false;
    }
    return true;
}

bool validUtf8(const std::uint8_t* bytes, std::size_t size) {
    std::size_t i = 0;
    while (i < size) {
        const std::uint8_t first = bytes[i++];
        if (first < 0x80U) {
            if (first == 0 || first == '\r') return false;
            continue;
        }

        std::uint32_t codepoint = 0;
        std::uint32_t minimum = 0;
        unsigned continuation = 0;
        if ((first & 0xe0U) == 0xc0U) {
            codepoint = first & 0x1fU;
            minimum = 0x80U;
            continuation = 1;
        } else if ((first & 0xf0U) == 0xe0U) {
            codepoint = first & 0x0fU;
            minimum = 0x800U;
            continuation = 2;
        } else if ((first & 0xf8U) == 0xf0U) {
            codepoint = first & 0x07U;
            minimum = 0x10000U;
            continuation = 3;
        } else {
            return false;
        }

        if (continuation > size - i) return false;
        for (unsigned j = 0; j < continuation; ++j) {
            const std::uint8_t next = bytes[i++];
            if ((next & 0xc0U) != 0x80U) return false;
            codepoint = (codepoint << 6) | (next & 0x3fU);
        }
        if (codepoint < minimum || codepoint > 0x10ffffU ||
            (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
            return false;
        }
    }
    return true;
}

bool validPath(std::string_view path) {
    if (path.empty() || path.size() > BundleView::MAX_PATH_BYTES || path.front() == '/') {
        return false;
    }
    for (const unsigned char character : path) {
        if (character >= 0x80U || character < 0x20U || character == 0x7fU ||
            character == '\\') {
            return false;
        }
    }
    std::size_t component_start = 0;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != '/') continue;
        const std::string_view component = path.substr(component_start, i - component_start);
        if (component.empty() || component == "." || component == "..") return false;
        component_start = i + 1;
    }

    constexpr std::array<std::string_view, 5> required = {
        "game.toml", "sprites.txt", "tiles.txt", "levels.txt", "README.md",
    };
    if (std::find(required.begin(), required.end(), path) != required.end() ||
        path == "rules.lua") {
        return true;
    }
    const std::size_t slash = path.find('/');
    if (slash == std::string_view::npos || path.find('/', slash + 1) != std::string_view::npos) {
        return false;
    }
    const std::string_view directory = path.substr(0, slash);
    const std::string_view filename = path.substr(slash + 1);
    return (directory == "modules" && filename.size() > 4 &&
            filename.substr(filename.size() - 4) == ".lua" &&
            validLuaModuleName(filename.substr(0, filename.size() - 4))) ||
           (directory == "test" && filename.size() > 6 &&
            filename.substr(filename.size() - 6) == ".steps");
}

bool isLuaPath(std::string_view path) {
    return path == "rules.lua" ||
           (path.size() > 4 && path.substr(path.size() - 4) == ".lua");
}

struct FileRecord {
    std::string_view path;
    std::uint16_t flags = 0;
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
    std::uint32_t checksum = 0;
};

bool readRecord(const std::uint8_t* table, std::size_t table_length,
                std::size_t& position, FileRecord& record) {
    if (position > table_length || table_length - position < ENTRY_SIZE) return false;
    const std::uint8_t* entry = table + position;
    const std::uint16_t path_length = read16(entry);
    record.flags = read16(entry + 2);
    record.offset = read32(entry + 4);
    record.length = read32(entry + 8);
    record.checksum = read32(entry + 12);
    position += ENTRY_SIZE;

    const std::size_t padded_length = align4(path_length);
    if (path_length == 0 || path_length > BundleView::MAX_PATH_BYTES ||
        padded_length > table_length - position) {
        return false;
    }
    record.path = std::string_view(reinterpret_cast<const char*>(table + position), path_length);
    if (!zeroFilled(table, position + path_length, position + padded_length)) return false;
    position += padded_length;
    return true;
}

}  // namespace

bool BundleView::fail(BundleError error) {
    bytes_ = nullptr;
    table_length_ = 0;
    file_count_ = 0;
    error_ = error;
    return false;
}

bool BundleView::parse(const std::uint8_t* bytes, std::size_t size) {
    bytes_ = nullptr;
    table_length_ = 0;
    file_count_ = 0;
    error_ = BundleError::None;
    if (bytes == nullptr || size < HEADER_SIZE) return fail(BundleError::Truncated);
    if (size > MAX_BUNDLE_BYTES) return fail(BundleError::TooLarge);
    if (!std::equal(MAGIC.begin(), MAGIC.end(), bytes)) return fail(BundleError::InvalidHeader);

    const std::uint16_t version = read16(bytes + 4);
    if (version != FORMAT_VERSION) return fail(BundleError::UnsupportedVersion);
    const std::uint16_t header_size = read16(bytes + 6);
    const std::uint16_t count = read16(bytes + 8);
    const std::uint16_t flags = read16(bytes + 10);
    const std::uint32_t table_offset = read32(bytes + 12);
    const std::uint32_t table_length = read32(bytes + 16);
    const std::uint32_t payload_offset = read32(bytes + 20);
    const std::uint32_t payload_length = read32(bytes + 24);
    const std::uint32_t total_length = read32(bytes + 28);
    const std::uint32_t table_checksum = read32(bytes + 32);
    if (header_size != HEADER_SIZE || flags != 0 || count == 0 || count > MAX_FILES ||
        table_offset != HEADER_SIZE || !zeroFilled(bytes, 36, HEADER_SIZE)) {
        return fail(BundleError::InvalidHeader);
    }

    const std::uint64_t table_end = static_cast<std::uint64_t>(table_offset) + table_length;
    const std::uint64_t expected_payload = align4(static_cast<std::size_t>(table_end));
    const std::uint64_t expected_total =
        static_cast<std::uint64_t>(payload_offset) + payload_length;
    if (table_end > size || expected_payload != payload_offset ||
        expected_total != total_length || total_length != size ||
        payload_offset > size || !zeroFilled(bytes, static_cast<std::size_t>(table_end),
                                             payload_offset)) {
        return fail(BundleError::InvalidHeader);
    }

    const std::uint8_t* table = bytes + table_offset;
    if (crc32(table, table_length) != table_checksum) return fail(BundleError::InvalidTable);

    std::size_t position = 0;
    std::string_view previous_path;
    for (std::size_t index = 0; index < count; ++index) {
        FileRecord record;
        if (!readRecord(table, table_length, position, record)) {
            return fail(BundleError::InvalidTable);
        }
        if (record.flags != 0 || !validPath(record.path) ||
            (!previous_path.empty() && !(previous_path < record.path))) {
            return fail(BundleError::InvalidPath);
        }

        const std::uint64_t data_end = static_cast<std::uint64_t>(record.offset) + record.length;
        if (record.length == 0 || record.length > MAX_FILE_BYTES ||
            (record.path == "game.toml" && record.length > MAX_MANIFEST_BYTES) ||
            (isLuaPath(record.path) && record.length > MAX_LUA_SOURCE_BYTES) ||
            record.offset % 4 != 0 || record.offset < payload_offset || data_end > total_length) {
            return fail(BundleError::InvalidFileData);
        }
        previous_path = record.path;
    }
    if (position != table_length) return fail(BundleError::InvalidTable);

    std::size_t cursor = payload_offset;
    position = 0;
    bool has_manifest = false;
    bool has_sprites = false;
    bool has_tiles = false;
    bool has_levels = false;
    bool has_readme = false;
    for (std::size_t index = 0; index < count; ++index) {
        FileRecord record;
        if (!readRecord(table, table_length, position, record)) {
            return fail(BundleError::InvalidTable);
        }
        const std::size_t offset = record.offset;
        const std::size_t end = offset + record.length;
        if (offset < cursor || !zeroFilled(bytes, cursor, offset)) {
            return fail(BundleError::InvalidFileData);
        }
        const std::uint8_t* data = bytes + offset;
        if (crc32(data, record.length) != record.checksum) {
            return fail(BundleError::InvalidFileData);
        }
        if (!validUtf8(data, record.length)) return fail(BundleError::InvalidText);
        if (isLuaPath(record.path) && record.length >= 4 && data[0] == 0x1bU &&
            data[1] == 'L' && data[2] == 'u' && data[3] == 'a') {
            return fail(BundleError::InvalidText);
        }
        has_manifest |= record.path == "game.toml";
        has_sprites |= record.path == "sprites.txt";
        has_tiles |= record.path == "tiles.txt";
        has_levels |= record.path == "levels.txt";
        has_readme |= record.path == "README.md";
        cursor = end;
    }
    if (position != table_length) return fail(BundleError::InvalidTable);
    if (cursor != total_length) return fail(BundleError::InvalidFileData);
    if (!has_manifest || !has_sprites || !has_tiles || !has_levels || !has_readme) {
        return fail(BundleError::MissingRequiredFile);
    }

    bytes_ = bytes;
    table_length_ = table_length;
    file_count_ = count;
    return true;
}

bool BundleView::fileAt(std::size_t index, BundleFileView& file) const {
    file = {};
    if (bytes_ == nullptr || index >= file_count_) return false;
    const std::uint8_t* table = bytes_ + HEADER_SIZE;
    std::size_t position = 0;
    for (std::size_t current = 0; current <= index; ++current) {
        FileRecord record;
        if (!readRecord(table, table_length_, position, record)) return false;
        if (current == index) {
            file = {record.path, bytes_ + record.offset, record.length};
            return true;
        }
    }
    return false;
}

bool BundleView::find(std::string_view path, BundleFileView& file) const {
    file = {};
    if (bytes_ == nullptr) return false;
    const std::uint8_t* table = bytes_ + HEADER_SIZE;
    std::size_t position = 0;
    for (std::size_t index = 0; index < file_count_; ++index) {
        FileRecord record;
        if (!readRecord(table, table_length_, position, record)) return false;
        if (record.path == path) {
            file = {record.path, bytes_ + record.offset, record.length};
            return true;
        }
    }
    return false;
}

}  // namespace tg::engine
