#pragma once

#include "Result.hpp"
#include "SzxCompression/CompressionExport.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::COMPRESSION
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

enum class ZipError : int
{
    InvalidArgument = 1,
    Io,
    InvalidArchive,
    EntryNotFound,
    UnsupportedBackend,
    ReadFailed,
    WriteFailed
};

enum class ZipCompressionMethod : std::uint8_t
{
    Store,
    Deflate,
    Zstd
};

struct ZipEntry
{
    std::string path;
    std::uint64_t uncompressedSize = 0;
    bool directory = false;
};

class SZX_COMPRESSION_API ZipReader final
{
public:
    static auto Open(const std::filesystem::path &path) -> SZX::COMMON::Result<ZipReader>;

    ZipReader();
    ~ZipReader();
    ZipReader(ZipReader &&) noexcept;
    ZipReader &operator=(ZipReader &&) noexcept;
    ZipReader(const ZipReader &) = delete;
    ZipReader &operator=(const ZipReader &) = delete;

    [[nodiscard]] auto entries() const -> const std::vector<ZipEntry> &;
    auto read(std::string_view path) const -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

private:
    struct Impl;
    explicit ZipReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class SZX_COMPRESSION_API ZipWriter final
{
public:
    static auto Create(const std::filesystem::path &path) -> SZX::COMMON::Result<ZipWriter>;

    ZipWriter();
    ~ZipWriter();
    ZipWriter(ZipWriter &&) noexcept;
    ZipWriter &operator=(ZipWriter &&) noexcept;
    ZipWriter(const ZipWriter &) = delete;
    ZipWriter &operator=(const ZipWriter &) = delete;

    auto addFile(std::string_view path, std::span<const std::uint8_t> data,
                 ZipCompressionMethod method = ZipCompressionMethod::Zstd) -> SZX::COMMON::Result<void>;
    auto close() -> SZX::COMMON::Result<void>;

private:
    struct Impl;
    explicit ZipWriter(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace SZX::COMPRESSION
