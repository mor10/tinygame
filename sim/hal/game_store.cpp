#include "game_store.h"

#include <Arduino.h>
#include <emscripten.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <string_view>

#include "bundle_view.h"
#include "crc32.h"
#include "manifest_view.h"
#include "registry.h"

namespace {

constexpr std::size_t SIM_STORAGE_BYTES = 4U * 1024U * 1024U;
constexpr std::size_t MIN_FREE_RESERVE = 8192;
constexpr std::size_t MAX_CHUNK_BYTES = 1024;
constexpr char STORAGE_PREFIX[] = "tinygame.sim.game.v1.";

bool mounted = false;
std::uint8_t* staged_bundle = nullptr;
char staged_id[16] = {};
std::size_t expected_size = 0;
std::size_t written_size = 0;

bool validId(const char* id) {
    if (id == nullptr || id[0] < 'a' || id[0] > 'z' || std::strlen(id) > 15) return false;
    for (const char* p = id; *p; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            return false;
        }
    }
    return true;
}

void resetTransfer() {
    delete[] staged_bundle;
    staged_bundle = nullptr;
    staged_id[0] = '\0';
    expected_size = 0;
    written_size = 0;
}

EM_JS(int, storedBytes, (), {
    const prefix = "tinygame.sim.game.v1.";
    try {
        let total = 0;
        for (let i = 0; i < localStorage.length; i++) {
            const key = localStorage.key(i);
            if (key && key.startsWith(prefix)) {
                const value = localStorage.getItem(key);
                if (value !== null) total += atob(value).length;
            }
        }
        return Math.min(total, 0x7fffffff);
    } catch (error) {
        if (Module.onSerial) Module.onSerial("[STORAGE] error measuring installed bundles: " + error.name + "\n");
        return -1;
    }
});

EM_JS(int, listStoredIds, (char* output, int capacity), {
    const prefix = "tinygame.sim.game.v1.";
    try {
        const ids = [];
        for (let i = 0; i < localStorage.length; i++) {
            const key = localStorage.key(i);
            if (!key || !key.startsWith(prefix)) continue;
            const id = key.slice(prefix.length);
            if (/^[a-z][a-z0-9_]{0,14}$/.test(id)) ids.push(id);
        }
        ids.sort();
        if (ids.length > capacity) return -1;
        ids.forEach((id, i) => stringToUTF8(id, output + i * 16, 16));
        return ids.length;
    } catch (error) {
        if (Module.onSerial) Module.onSerial("[STORAGE] error listing installed bundles: " + error.name + "\n");
        return -1;
    }
});

EM_JS(int, storedBundleSize, (const char* id), {
    const key = "tinygame.sim.game.v1." + UTF8ToString(id);
    try {
        const value = localStorage.getItem(key);
        return value === null ? -1 : atob(value).length;
    } catch (error) {
        if (Module.onSerial) Module.onSerial("[STORAGE] error reading " + key + ": " + error.name + "\n");
        return -2;
    }
});

EM_JS(int, copyStoredBundle, (const char* id, std::uint8_t* output, int size), {
    const key = "tinygame.sim.game.v1." + UTF8ToString(id);
    try {
        const value = localStorage.getItem(key);
        if (value === null) return 0;
        const binary = atob(value);
        if (binary.length !== size) return 0;
        const bytes = new Uint8Array(size);
        for (let i = 0; i < size; i++) bytes[i] = binary.charCodeAt(i);
        HEAPU8.set(bytes, output);
        return 1;
    } catch (error) {
        if (Module.onSerial) Module.onSerial("[STORAGE] error reading " + key + ": " + error.name + "\n");
        return 0;
    }
});

EM_JS(int, saveStoredBundle, (const char* id, const std::uint8_t* input, int size), {
    const key = "tinygame.sim.game.v1." + UTF8ToString(id);
    try {
        const bytes = HEAPU8.subarray(input, input + size);
        let binary = "";
        for (let offset = 0; offset < size; offset += 32768) {
            binary += String.fromCharCode(...bytes.subarray(offset, Math.min(offset + 32768, size)));
        }
        localStorage.setItem(key, btoa(binary));
        return 1;
    } catch (error) {
        if (Module.onSerial) Module.onSerial("[STORAGE] error saving " + key + ": " + error.name + "\n");
        return 0;
    }
});

bool validBundle(const std::uint8_t* bytes, std::size_t size, const char* expected_id,
                 game_store::InstalledGame* metadata) {
    tg::engine::BundleView bundle;
    if (!bundle.parse(bytes, size)) return false;
    tg::engine::BundleFileView manifest_file;
    tg::engine::ManifestView manifest;
    if (!bundle.find("game.toml", manifest_file) ||
        !tg::engine::parseManifest(std::string_view(
            reinterpret_cast<const char*>(manifest_file.data), manifest_file.size), manifest) ||
        std::strcmp(expected_id, manifest.id) != 0) {
        return false;
    }
    if (metadata != nullptr) {
        std::memcpy(metadata->id, manifest.id, sizeof(metadata->id));
        std::memcpy(metadata->name, manifest.name, sizeof(metadata->name));
        metadata->scroll = manifest.scroll;
    }
    return true;
}

}  // namespace

namespace game_store {

bool initialize() {
    mounted = true;
    return true;
}

bool ready() {
    return mounted;
}

std::size_t totalBytes() {
    return mounted ? SIM_STORAGE_BYTES : 0;
}

std::size_t freeBytes() {
    if (!mounted) return 0;
    const int used = storedBytes();
    if (used < 0 || static_cast<std::size_t>(used) >= SIM_STORAGE_BYTES) return 0;
    return SIM_STORAGE_BYTES - static_cast<std::size_t>(used);
}

std::size_t maxBundleBytes() {
    const std::size_t available = freeBytes();
    return available > MIN_FREE_RESERVE
        ? std::min<std::size_t>(tg::engine::BundleView::MAX_BUNDLE_BYTES,
                                available - MIN_FREE_RESERVE)
        : 0;
}

Error beginInstall(const char* id, std::size_t size) {
    if (!mounted) return Error::NotReady;
    if (staged_bundle != nullptr) return Error::TransferActive;
    if (!validId(id)) return Error::InvalidId;
    if (size == 0 || size > tg::engine::BundleView::MAX_BUNDLE_BYTES) return Error::TooLarge;
    if (size > maxBundleBytes()) return Error::NoSpace;
    staged_bundle = new (std::nothrow) std::uint8_t[size];
    if (staged_bundle == nullptr) return Error::OutOfMemory;
    std::memcpy(staged_id, id, std::strlen(id) + 1);
    expected_size = size;
    return Error::None;
}

Error writeChunk(std::size_t offset, const std::uint8_t* bytes, std::size_t size) {
    if (staged_bundle == nullptr) return Error::NotReady;
    if (bytes == nullptr || size == 0 || size > MAX_CHUNK_BYTES ||
        offset != written_size || size > expected_size - written_size) {
        return Error::Incomplete;
    }
    std::memcpy(staged_bundle + offset, bytes, size);
    written_size += size;
    return Error::None;
}

Error finishInstall(std::uint32_t expected_crc32) {
    if (staged_bundle == nullptr) return Error::NotReady;
    if (written_size != expected_size) return Error::Incomplete;
    if (tg::engine::crc32(staged_bundle, expected_size) != expected_crc32) {
        resetTransfer();
        return Error::ChecksumMismatch;
    }
    if (!validBundle(staged_bundle, expected_size, staged_id, nullptr)) {
        resetTransfer();
        return Error::InvalidBundle;
    }
    if (!saveStoredBundle(staged_id, staged_bundle, static_cast<int>(expected_size))) {
        resetTransfer();
        return Error::WriteFailed;
    }
    resetTransfer();
    return Error::None;
}

Error cancelInstall() {
    if (staged_bundle == nullptr) return Error::NotReady;
    resetTransfer();
    return Error::None;
}

Error listInstalled(InstalledGame* games, std::size_t capacity, std::size_t& count) {
    count = 0;
    if (!mounted) return Error::NotReady;
    if (capacity > 0 && games == nullptr) return Error::InvalidBundle;
    char ids[registry::MAX_GAMES][16] = {};
    const int stored_count = listStoredIds(&ids[0][0], registry::MAX_GAMES);
    if (stored_count < 0) return Error::WriteFailed;
    for (int i = 0; i < stored_count; ++i) {
        if (count >= capacity) {
            Serial.printf("[STORE] installed game capacity reached at %s\n", ids[i]);
            continue;
        }
        std::uint8_t* bytes = nullptr;
        std::size_t size = 0;
        game_store::InstalledGame metadata = {};
        const Error result = loadBundle(ids[i], bytes, size, &metadata);
        if (result != Error::None) {
            Serial.printf("[STORE] skipping %s: %s\n", ids[i], errorMessage(result));
        } else {
            games[count++] = metadata;
        }
        releaseBundle(bytes);
    }
    return Error::None;
}

Error loadBundle(const char* id, std::uint8_t*& bytes, std::size_t& size,
                 InstalledGame* metadata) {
    bytes = nullptr;
    size = 0;
    if (!mounted) return Error::NotReady;
    if (!validId(id)) return Error::InvalidId;
    const int stored_size = storedBundleSize(id);
    if (stored_size == -1) return Error::NotFound;
    if (stored_size < 0) return Error::WriteFailed;
    size = static_cast<std::size_t>(stored_size);
    if (size == 0 || size > tg::engine::BundleView::MAX_BUNDLE_BYTES) {
        size = 0;
        return Error::TooLarge;
    }
    bytes = new (std::nothrow) std::uint8_t[size];
    if (bytes == nullptr) {
        size = 0;
        return Error::OutOfMemory;
    }
    if (!copyStoredBundle(id, bytes, static_cast<int>(size))) {
        releaseBundle(bytes);
        bytes = nullptr;
        size = 0;
        return Error::WriteFailed;
    }
    if (!validBundle(bytes, size, id, metadata)) {
        releaseBundle(bytes);
        bytes = nullptr;
        size = 0;
        return Error::InvalidBundle;
    }
    return Error::None;
}

void releaseBundle(const std::uint8_t* bytes) {
    delete[] bytes;
}

const char* errorMessage(Error error) {
    switch (error) {
        case Error::None: return "ok";
        case Error::NotReady: return "game storage is not ready";
        case Error::NotFound: return "installed game bundle was not found";
        case Error::InvalidId: return "invalid game id";
        case Error::TransferActive: return "another install is active";
        case Error::TooLarge: return "bundle exceeds the supported size";
        case Error::NoSpace: return "not enough free storage for a safe install";
        case Error::OutOfMemory: return "not enough memory to validate the staged bundle";
        case Error::ChecksumMismatch: return "bundle transfer checksum mismatch";
        case Error::InvalidManifest: return "bundle manifest is invalid or does not match its id";
        case Error::WriteFailed: return "could not write or read the persistent browser store";
        case Error::Incomplete: return "bundle transfer is incomplete or out of order";
        case Error::InvalidBundle: return "bundle validation failed";
        case Error::CommitFailed: return "could not replace the installed bundle";
    }
    return "unknown storage error";
}

}  // namespace game_store
