#include <cassert>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "bundle_script.h"
#include "crc32.h"

using tg::engine::BundleError;
using tg::engine::BundleScript;

std::vector<std::uint8_t> readHex(const char* path) {
    std::ifstream stream(path);
    assert(stream.good());
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    std::vector<std::uint8_t> bytes;
    int high = -1;
    for (const unsigned char character : text) {
        if (character == ' ' || character == '\n' || character == '\r' || character == '\t') {
            continue;
        }
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

void loadsAndTicksLuaFromATextBundle() {
    const auto bytes = readHex("tests/fixtures/tgb_v1_lua_tick.hex");
    BundleScript game(17);
    assert(game.load(bytes.data(), bytes.size()));
    assert(game.ready());
    tg::Input input;
    input.main.delta = 6;
    input.left.delta = -3;
    input.left.down = true;
    input.right.pressed = true;
    input.click = true;
    input.held_ms = 24;
    assert(game.update(1.0f / 60.0f, input));
    assert(game.frame().size() == 2);
    assert(game.frame().at(0).kind == tg::engine::DrawKind::Rectangle);
    assert(game.frame().at(0).x == 2.0f && game.frame().at(0).width == 4.0f);
    assert(game.frame().at(1).kind == tg::engine::DrawKind::Text);
    assert(std::string(game.frame().at(1).text) == "OK");
    assert(game.update(1.0f / 30.0f, input));
    assert(!game.update(1.0f / 60.0f, input));
    assert(!game.ready());
    assert(game.frame().size() == 0);
}

void loadsBundleLocalModulesThroughTheSandbox() {
    const auto bytes = readHex("tests/fixtures/tgb_v1_lua_modules.hex");
    BundleScript game;
    assert(game.load(bytes.data(), bytes.size()));
    assert(game.update(1.0f / 60.0f));
    assert(game.frame().size() == 1);
    assert(game.frame().at(0).kind == tg::engine::DrawKind::Rectangle);
    assert(game.frame().at(0).palette_index == 7);
}

void rejectsInvalidBundlesAndMissingScripts() {
    const auto invalid = readHex("tests/fixtures/tgb_v1_invalid.hex");
    BundleScript invalid_game;
    assert(!invalid_game.load(invalid.data(), invalid.size()));
    assert(invalid_game.bundleError() != BundleError::None);

    const auto valid = readHex("tests/fixtures/tgb_v1_valid.hex");
    BundleScript missing_script;
    assert(!missing_script.load(valid.data(), valid.size()));
    assert(missing_script.bundleError() == BundleError::None);
}

void rejectsInvalidMetadataBeforeRunningScript() {
    auto bytes = readHex("tests/fixtures/tgb_v1_lua_tick.hex");
    tg::engine::BundleView bundle;
    assert(bundle.parse(bytes.data(), bytes.size()));
    tg::engine::BundleFileView file;
    assert(bundle.find("game.toml", file));
    const auto offset = static_cast<std::size_t>(file.data - bytes.data());
    const std::string text(reinterpret_cast<const char*>(file.data), file.size);
    const auto api = text.find("api = 1");
    assert(api != std::string::npos);
    bytes[offset + api + 6] = '2';
    const char path[] = "game.toml";
    const auto found = std::search(bytes.begin() + 40, bytes.end(), path, path + 9);
    assert(found != bytes.end());
    const auto entry = static_cast<std::size_t>(found - bytes.begin()) - 16;
    const auto write32 = [&](std::size_t position, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) bytes[position + i] = value >> (i * 8);
    };
    const std::uint32_t table_length = bytes[16] | (std::uint32_t(bytes[17]) << 8) |
                                       (std::uint32_t(bytes[18]) << 16) |
                                       (std::uint32_t(bytes[19]) << 24);
    write32(entry + 12, tg::engine::crc32(bytes.data() + offset, file.size));
    write32(32, tg::engine::crc32(bytes.data() + 40, table_length));
    assert(bundle.parse(bytes.data(), bytes.size()));

    BundleScript game;
    assert(!game.load(bytes.data(), bytes.size()));
    assert(!game.ready());
    assert(std::string(game.error()) == "Invalid game manifest");
}

int main() {
    loadsAndTicksLuaFromATextBundle();
    loadsBundleLocalModulesThroughTheSandbox();
    rejectsInvalidBundlesAndMissingScripts();
    rejectsInvalidMetadataBeforeRunningScript();
    std::puts("bundle script tests passed");
}
