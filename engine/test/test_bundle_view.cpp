#include <cassert>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "bundle_view.h"

using tg::engine::BundleError;
using tg::engine::BundleView;

std::uint32_t crc32(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

std::uint32_t read32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

void write32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

std::vector<std::uint8_t> readHex(const char* path) {
    std::ifstream stream(path);
    assert(stream.good());
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    std::vector<std::uint8_t> bytes;
    int high = -1;
    for (const unsigned char character : text) {
        if (std::isspace(character)) continue;
        const int value = character >= '0' && character <= '9' ? character - '0' :
                          character >= 'a' && character <= 'f' ? character - 'a' + 10 :
                          character >= 'A' && character <= 'F' ? character - 'A' + 10 : -1;
        assert(value >= 0);
        if (high < 0) {
            high = value;
        } else {
            bytes.push_back(static_cast<std::uint8_t>((high << 4) | value));
            high = -1;
        }
    }
    assert(high < 0);
    return bytes;
}

void acceptsTheSharedValidGoldenVector() {
    const auto bytes = readHex("tests/fixtures/tgb_v1_valid.hex");
    BundleView bundle;
    assert(bundle.parse(bytes.data(), bytes.size()));
    assert(bundle.error() == BundleError::None);
    assert(bundle.fileCount() == 5);
    tg::engine::BundleFileView file;
    assert(bundle.fileAt(0, file));
    assert(file.path == "README.md");
    assert(bundle.find("game.toml", file) && file.size > 0);
    assert(!bundle.find("../outside", file));
    assert(!bundle.fileAt(bundle.fileCount(), file));
}

void rejectsInvalidAndDamagedGoldenVectors() {
    auto invalid = readHex("tests/fixtures/tgb_v1_invalid.hex");
    BundleView bundle;
    assert(!bundle.parse(invalid.data(), invalid.size()));
    assert(bundle.error() != BundleError::None);

    auto damaged = readHex("tests/fixtures/tgb_v1_valid.hex");
    assert(bundle.parse(damaged.data(), damaged.size()));
    tg::engine::BundleFileView readme;
    assert(bundle.find("README.md", readme));
    const auto offset = static_cast<std::size_t>(readme.data - damaged.data());
    damaged[offset] ^= 1U;
    assert(!bundle.parse(damaged.data(), damaged.size()));
    assert(bundle.error() == BundleError::InvalidFileData);

    assert(!bundle.parse(damaged.data(), 39));
    assert(bundle.error() == BundleError::Truncated);
}

void rejectsLuaBytecodeEvenWithValidChecksums() {
    auto bytes = readHex("tests/fixtures/tgb_v1_valid.hex");
    BundleView bundle;
    assert(bundle.parse(bytes.data(), bytes.size()));
    tg::engine::BundleFileView file;
    assert(bundle.find("levels.txt", file) && file.size >= 4);
    const std::size_t data_offset = static_cast<std::size_t>(file.data - bytes.data());
    constexpr char old_path[] = "levels.txt";
    constexpr char lua_path[] = "rules.lua";
    const auto path = std::search(bytes.begin() + 40, bytes.end(),
                                  old_path, old_path + sizeof(old_path) - 1);
    assert(path != bytes.end());
    const std::size_t path_offset = static_cast<std::size_t>(path - bytes.begin());
    const std::size_t entry_offset = path_offset - 16;
    bytes[entry_offset] = sizeof(lua_path) - 1;
    bytes[entry_offset + 1] = 0;
    std::memcpy(bytes.data() + path_offset, lua_path, sizeof(lua_path) - 1);
    bytes[path_offset + sizeof(lua_path) - 1] = 0;
    bytes[data_offset] = 0x1b;
    bytes[data_offset + 1] = 'L';
    bytes[data_offset + 2] = 'u';
    bytes[data_offset + 3] = 'a';
    write32(bytes, entry_offset + 12, crc32(bytes.data() + data_offset, file.size));
    const std::uint32_t table_length = read32(bytes, 16);
    write32(bytes, 32, crc32(bytes.data() + 40, table_length));

    assert(!bundle.parse(bytes.data(), bytes.size()));
    assert(bundle.error() == BundleError::InvalidText);
}

void rejectsUnaddressableModuleNames() {
    const auto original = readHex("tests/fixtures/tgb_v1_lua_modules.hex");
    BundleView bundle;
    assert(bundle.parse(original.data(), original.size()));
    constexpr char path[] = "modules/palette.lua";
    for (const char invalid : {'1', '-'}) {
        auto bytes = original;
        const auto found = std::search(bytes.begin() + 40, bytes.end(),
                                       path, path + sizeof(path) - 1);
        assert(found != bytes.end());
        const auto index = static_cast<std::size_t>(found - bytes.begin());
        bytes[index + (invalid == '1' ? 8 : 10)] = static_cast<std::uint8_t>(invalid);
        write32(bytes, 32, crc32(bytes.data() + 40, read32(bytes, 16)));
        assert(!bundle.parse(bytes.data(), bytes.size()));
        assert(bundle.error() == BundleError::InvalidPath);
    }
}

void rejectsOversizedLuaWithValidChecksums() {
    auto bytes = readHex("tests/fixtures/tgb_v1_lua_modules.hex");
    BundleView bundle;
    assert(bundle.parse(bytes.data(), bytes.size()));
    tg::engine::BundleFileView rules;
    assert(bundle.find("rules.lua", rules));
    const auto offset = static_cast<std::size_t>(rules.data - bytes.data());
    const auto length = rules.size;
    constexpr std::size_t extra = 64 * 1024;
    bytes.insert(bytes.begin() + offset + length, extra, ' ');

    std::size_t entry = 40;
    const std::uint16_t count = bytes[8] | (static_cast<std::uint16_t>(bytes[9]) << 8);
    for (std::size_t i = 0; i < count; ++i) {
        const auto name_length = static_cast<std::size_t>(bytes[entry]) |
                                 (static_cast<std::size_t>(bytes[entry + 1]) << 8);
        const std::string name(reinterpret_cast<const char*>(bytes.data() + entry + 16),
                               name_length);
        const auto data_offset = read32(bytes, entry + 4);
        if (data_offset > offset) write32(bytes, entry + 4, data_offset + extra);
        if (name == "rules.lua") {
            write32(bytes, entry + 8, length + extra);
            write32(bytes, entry + 12, crc32(bytes.data() + offset, length + extra));
        }
        entry += 16 + ((name_length + 3) & ~std::size_t{3});
    }
    write32(bytes, 24, read32(bytes, 24) + extra);
    write32(bytes, 28, read32(bytes, 28) + extra);
    write32(bytes, 32, crc32(bytes.data() + 40, read32(bytes, 16)));
    assert(!bundle.parse(bytes.data(), bytes.size()));
    assert(bundle.error() == BundleError::InvalidFileData);
}

int main() {
    acceptsTheSharedValidGoldenVector();
    rejectsInvalidAndDamagedGoldenVectors();
    rejectsLuaBytecodeEvenWithValidChecksums();
    rejectsUnaddressableModuleNames();
    rejectsOversizedLuaWithValidChecksums();
    std::puts("bundle view tests passed");
}
