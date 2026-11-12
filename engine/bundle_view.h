#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tg::engine {

enum class BundleError : std::uint8_t {
    None,
    Truncated,
    TooLarge,
    InvalidHeader,
    UnsupportedVersion,
    InvalidTable,
    InvalidPath,
    InvalidFileData,
    InvalidText,
    MissingRequiredFile,
};

struct BundleFileView {
    std::string_view path;
    const std::uint8_t* data = nullptr;
    std::uint32_t size = 0;
};

class BundleView {
public:
    static constexpr std::size_t MAX_FILES = 256;
    static constexpr std::size_t MAX_PATH_BYTES = 128;
    static constexpr std::size_t MAX_FILE_BYTES = 1U << 20;
    static constexpr std::size_t MAX_MANIFEST_BYTES = 32U << 10;
    static constexpr std::size_t MAX_BUNDLE_BYTES = 4U << 20;

    // The input bytes must remain alive and unchanged while this view is used.
    bool parse(const std::uint8_t* bytes, std::size_t size);
    std::size_t fileCount() const { return file_count_; }
    bool fileAt(std::size_t index, BundleFileView& file) const;
    bool find(std::string_view path, BundleFileView& file) const;
    BundleError error() const { return error_; }

private:
    bool fail(BundleError error);

    const std::uint8_t* bytes_ = nullptr;
    std::uint32_t table_length_ = 0;
    std::size_t file_count_ = 0;
    BundleError error_ = BundleError::None;
};

}  // namespace tg::engine
