#include "game_store.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "bundle_view.h"
#include "crc32.h"
#include "game_store_transaction.h"
#include "manifest_view.h"

namespace {

constexpr std::size_t MIN_FREE_RESERVE = 8192;
constexpr std::size_t MAX_CHUNK_BYTES = 1024;
constexpr std::uint8_t LITTLEFS_SUBTYPE = 0x83;

bool mounted;
File stage_file;
char active_path[32] = {};
char stage_path[32] = {};
char backup_path[32] = {};
std::size_t expected_size;
std::size_t written_size;

bool validId(const char* id) {
    if (id == nullptr || id[0] < 'a' || id[0] > 'z' || std::strlen(id) > 15) return false;
    for (const char* p = id; *p; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            return false;
        }
    }
    return true;
}

bool makePaths(const char* id) {
    if (!validId(id)) return false;
    std::snprintf(active_path, sizeof(active_path), "/games/%s.tgb", id);
    std::snprintf(stage_path, sizeof(stage_path), "/games/%s.part", id);
    std::snprintf(backup_path, sizeof(backup_path), "/games/%s.prev", id);
    return true;
}

bool removeIfPresent(const char* path) {
    return !LittleFS.exists(path) || LittleFS.remove(path);
}

bool partitionIsBlank() {
    const auto* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(LITTLEFS_SUBTYPE),
        "littlefs");
    if (partition == nullptr) return false;

    std::uint8_t bytes[512];
    for (std::size_t offset = 0; offset < partition->size; offset += sizeof(bytes)) {
        const std::size_t amount = std::min<std::size_t>(sizeof(bytes), partition->size - offset);
        if (esp_partition_read(partition, offset, bytes, amount) != ESP_OK) return false;
        for (std::size_t i = 0; i < amount; ++i) {
            if (bytes[i] != 0xff) return false;
        }
    }
    return true;
}

bool recoverFiles() {
    if (!LittleFS.exists("/games") && !LittleFS.mkdir("/games")) return false;

    File directory = LittleFS.open("/games");
    if (!directory || !directory.isDirectory()) return false;
    File entry = directory.openNextFile();
    while (entry) {
        char filename[32] = {};
        const char* name = entry.name();
        const char* base = std::strrchr(name, '/');
        base = base == nullptr ? name : base + 1;
        const std::size_t length = std::strlen(base);
        const bool fits = length < sizeof(filename);
        if (fits) std::memcpy(filename, base, length + 1);
        entry.close();
        if (!fits) {
            directory.close();
            return false;
        }
        char path[32];
        std::snprintf(path, sizeof(path), "/games/%s", filename);
        const bool remove_stage = length > 5 &&
                                  std::strcmp(filename + length - 5, ".part") == 0;
        const bool is_backup = length > 5 &&
                               std::strcmp(filename + length - 5, ".prev") == 0;
        char active[32] = {};
        if (is_backup) {
            std::snprintf(active, sizeof(active), "/games/%.*s.tgb",
                          static_cast<int>(length - 5), filename);
        }
        if (remove_stage && !LittleFS.remove(path)) {
            directory.close();
            return false;
        }
        if (is_backup &&
            !game_store::transaction::recoverBackup(LittleFS, active, path)) {
            directory.close();
            return false;
        }
        entry = directory.openNextFile();
    }
    directory.close();
    return true;
}

void resetTransfer() {
    stage_file.close();
    active_path[0] = '\0';
    stage_path[0] = '\0';
    backup_path[0] = '\0';
    expected_size = 0;
    written_size = 0;
}

std::uint8_t* allocateBundle(std::size_t size) {
    auto* bytes = static_cast<std::uint8_t*>(
        heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (bytes == nullptr) bytes = static_cast<std::uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_8BIT));
    return bytes;
}

}  // namespace

namespace game_store {

bool initialize() {
    if (mounted) return true;
    if (!LittleFS.begin(false, "/littlefs", 4, "littlefs")) {
        if (!partitionIsBlank() || !LittleFS.format() ||
            !LittleFS.begin(false, "/littlefs", 4, "littlefs")) {
            return false;
        }
    }
    if (!recoverFiles()) {
        LittleFS.end();
        return false;
    }
    mounted = true;
    return true;
}

bool ready() {
    return mounted;
}

std::size_t totalBytes() {
    return mounted ? LittleFS.totalBytes() : 0;
}

std::size_t freeBytes() {
    if (!mounted) return 0;
    const std::size_t total = LittleFS.totalBytes();
    const std::size_t used = LittleFS.usedBytes();
    return used < total ? total - used : 0;
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
    if (stage_file) return Error::TransferActive;
    if (!makePaths(id)) return Error::InvalidId;
    if (size == 0 || size > tg::engine::BundleView::MAX_BUNDLE_BYTES) return Error::TooLarge;
    if (!LittleFS.exists("/games") && !LittleFS.mkdir("/games")) return Error::WriteFailed;
    if (!transaction::recoverBackup(LittleFS, active_path, backup_path) ||
        !removeIfPresent(stage_path) || !removeIfPresent(backup_path)) {
        return Error::WriteFailed;
    }
    if (size > maxBundleBytes()) return Error::NoSpace;

    stage_file = LittleFS.open(stage_path, FILE_WRITE);
    if (!stage_file) return Error::WriteFailed;
    expected_size = size;
    written_size = 0;
    return Error::None;
}

Error writeChunk(std::size_t offset, const std::uint8_t* bytes, std::size_t size) {
    if (!stage_file) return Error::NotReady;
    if (bytes == nullptr || size == 0 || size > MAX_CHUNK_BYTES ||
        offset != written_size || size > expected_size - written_size) {
        return Error::Incomplete;
    }
    if (stage_file.write(bytes, size) != size) {
        stage_file.close();
        LittleFS.remove(stage_path);
        resetTransfer();
        return Error::WriteFailed;
    }
    written_size += size;
    return Error::None;
}

Error finishInstall(std::uint32_t expected_crc32) {
    if (!stage_file) return Error::NotReady;
    if (written_size != expected_size) return Error::Incomplete;
    stage_file.flush();
    stage_file.close();

    File staged = LittleFS.open(stage_path, FILE_READ);
    if (!staged || staged.size() != expected_size) {
        staged.close();
        LittleFS.remove(stage_path);
        resetTransfer();
        return Error::WriteFailed;
    }
    std::uint8_t* bytes = allocateBundle(expected_size);
    if (bytes == nullptr) {
        staged.close();
        LittleFS.remove(stage_path);
        resetTransfer();
        return Error::OutOfMemory;
    }
    const std::size_t read = staged.read(bytes, expected_size);
    staged.close();
    if (read != expected_size) {
        heap_caps_free(bytes);
        LittleFS.remove(stage_path);
        resetTransfer();
        return Error::WriteFailed;
    }
    if (tg::engine::crc32(bytes, expected_size) != expected_crc32) {
        heap_caps_free(bytes);
        LittleFS.remove(stage_path);
        resetTransfer();
        return Error::ChecksumMismatch;
    }
    tg::engine::BundleView bundle;
    const bool structure_valid = bundle.parse(bytes, expected_size);
    tg::engine::ManifestView manifest;
    const char* id = active_path + 7;
    const char* suffix = std::strrchr(id, '.');
    char expected_id[16] = {};
    const bool valid_path = suffix != nullptr && std::strcmp(suffix, ".tgb") == 0 &&
                            static_cast<std::size_t>(suffix - id) < sizeof(expected_id);
    if (valid_path) {
        std::memcpy(expected_id, id, static_cast<std::size_t>(suffix - id));
    }
    tg::engine::BundleFileView manifest_file;
    const bool manifest_valid =
        structure_valid && valid_path && bundle.find("game.toml", manifest_file) &&
        tg::engine::parseManifest(std::string_view(
            reinterpret_cast<const char*>(manifest_file.data), manifest_file.size), manifest) &&
        std::strcmp(expected_id, manifest.id) == 0;
    heap_caps_free(bytes);
    if (!structure_valid || !manifest_valid) {
        LittleFS.remove(stage_path);
        resetTransfer();
        return structure_valid ? Error::InvalidManifest : Error::InvalidBundle;
    }

    if (!transaction::commit(LittleFS, active_path, stage_path, backup_path)) {
        resetTransfer();
        return Error::CommitFailed;
    }
    resetTransfer();
    return Error::None;
}

Error cancelInstall() {
    if (!stage_file) return Error::NotReady;
    stage_file.close();
    const bool removed = LittleFS.remove(stage_path);
    resetTransfer();
    return removed ? Error::None : Error::WriteFailed;
}

Error listInstalled(InstalledGame* games, std::size_t capacity, std::size_t& count) {
    count = 0;
    if (!mounted) return Error::NotReady;
    if (capacity > 0 && games == nullptr) return Error::InvalidBundle;
    File directory = LittleFS.open("/games");
    if (!directory || !directory.isDirectory()) return Error::WriteFailed;

    File entry = directory.openNextFile();
    while (entry) {
        char filename[32] = {};
        const char* name = entry.name();
        const char* base = std::strrchr(name, '/');
        base = base == nullptr ? name : base + 1;
        const std::size_t source_length = std::strlen(base);
        const bool filename_fits = source_length < sizeof(filename);
        if (filename_fits) std::memcpy(filename, base, source_length + 1);
        const std::size_t length = filename_fits ? source_length : 0;
        const bool bundle_file = length > 4 &&
                                 std::strcmp(filename + length - 4, ".tgb") == 0;
        entry.close();
        if (!filename_fits) {
            Serial.println("[STORE] skipping overlong installed filename");
            entry = directory.openNextFile();
            continue;
        }
        if (bundle_file) {
            const std::size_t id_length = length - 4;
            if (id_length > 15) {
                Serial.printf("[STORE] skipping invalid installed game filename %s\n", filename);
            } else {
                char id[16];
                std::memcpy(id, filename, id_length);
                id[id_length] = '\0';
                if (count >= capacity) {
                    Serial.printf("[STORE] installed game capacity reached at %s\n", id);
                    entry = directory.openNextFile();
                    continue;
                }
                std::uint8_t* bytes = nullptr;
                std::size_t size = 0;
                InstalledGame metadata = {};
                const Error result = loadBundle(id, bytes, size, &metadata);
                if (result != Error::None) {
                    Serial.printf("[STORE] skipping %s: %s\n", id, errorMessage(result));
                } else {
                    games[count++] = metadata;
                }
                releaseBundle(bytes);
            }
        }
        entry = directory.openNextFile();
    }
    directory.close();
    return Error::None;
}

Error loadBundle(const char* id, std::uint8_t*& bytes, std::size_t& size,
                 InstalledGame* metadata) {
    bytes = nullptr;
    size = 0;
    if (!mounted) return Error::NotReady;
    if (!validId(id)) return Error::InvalidId;
    char path[32];
    std::snprintf(path, sizeof(path), "/games/%s.tgb", id);
    File file = LittleFS.open(path, FILE_READ);
    if (!file) return LittleFS.exists(path) ? Error::WriteFailed : Error::NotFound;
    size = file.size();
    if (size == 0 || size > tg::engine::BundleView::MAX_BUNDLE_BYTES) {
        file.close();
        size = 0;
        return Error::TooLarge;
    }
    bytes = allocateBundle(size);
    if (bytes == nullptr) {
        file.close();
        size = 0;
        return Error::OutOfMemory;
    }
    const std::size_t read = file.read(bytes, size);
    file.close();
    if (read != size) {
        releaseBundle(bytes);
        bytes = nullptr;
        size = 0;
        return Error::WriteFailed;
    }
    tg::engine::ManifestView manifest;
    tg::engine::BundleView bundle;
    if (!bundle.parse(bytes, size)) {
        releaseBundle(bytes);
        bytes = nullptr;
        size = 0;
        return Error::InvalidBundle;
    }
    tg::engine::BundleFileView manifest_file;
    if (!bundle.find("game.toml", manifest_file) ||
        !tg::engine::parseManifest(std::string_view(
            reinterpret_cast<const char*>(manifest_file.data), manifest_file.size), manifest) ||
        std::strcmp(id, manifest.id) != 0) {
        releaseBundle(bytes);
        bytes = nullptr;
        size = 0;
        return Error::InvalidManifest;
    }
    if (metadata != nullptr) {
        std::memcpy(metadata->id, manifest.id, sizeof(metadata->id));
        std::memcpy(metadata->name, manifest.name, sizeof(metadata->name));
        metadata->scroll = manifest.scroll;
    }
    return Error::None;
}

void releaseBundle(const std::uint8_t* bytes) {
    heap_caps_free(const_cast<std::uint8_t*>(bytes));
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
        case Error::WriteFailed: return "could not write the staged bundle";
        case Error::Incomplete: return "bundle transfer is incomplete or out of order";
        case Error::InvalidBundle: return "bundle validation failed";
        case Error::CommitFailed: return "could not replace the installed bundle";
    }
    return "unknown storage error";
}

}  // namespace game_store
