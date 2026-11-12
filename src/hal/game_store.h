#pragma once

#include <cstddef>
#include <cstdint>

namespace game_store {

enum class Error : std::uint8_t {
    None,
    NotReady,
    NotFound,
    InvalidId,
    TransferActive,
    TooLarge,
    NoSpace,
    OutOfMemory,
    ChecksumMismatch,
    InvalidManifest,
    WriteFailed,
    Incomplete,
    InvalidBundle,
    CommitFailed,
};

struct InstalledGame {
    char id[16];
    char name[14];
    bool scroll;
};

bool initialize();
bool ready();
std::size_t totalBytes();
std::size_t freeBytes();
std::size_t maxBundleBytes();
Error beginInstall(const char* id, std::size_t size);
Error writeChunk(std::size_t offset, const std::uint8_t* bytes, std::size_t size);
Error finishInstall(std::uint32_t expected_crc32);
Error cancelInstall();
Error listInstalled(InstalledGame* games, std::size_t capacity, std::size_t& count);
Error loadBundle(const char* id, std::uint8_t*& bytes, std::size_t& size,
                 InstalledGame* metadata = nullptr);
void releaseBundle(const std::uint8_t* bytes);
const char* errorMessage(Error error);

}  // namespace game_store
