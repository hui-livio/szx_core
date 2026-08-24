#include "SzxCompression/Zip.h"

#if SZX_COMPRESSION_HAS_MINIZIP
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#endif

#include <limits>
#include <utility>

namespace SZX::COMPRESSION
{
namespace
{
template <typename T>
auto failure(ZipError error, const char *message) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(error), message);
}

auto methodValue(ZipCompressionMethod method) -> int32_t
{
#if SZX_COMPRESSION_HAS_MINIZIP
    switch (method)
    {
    case ZipCompressionMethod::Store:
        return MZ_COMPRESS_METHOD_STORE;
    case ZipCompressionMethod::Deflate:
        return MZ_COMPRESS_METHOD_DEFLATE;
    case ZipCompressionMethod::Zstd:
        return MZ_COMPRESS_METHOD_ZSTD;
    }
    return -1;
#else
    static_cast<void>(method);
    return -1;
#endif
}

#if SZX_COMPRESSION_HAS_MINIZIP
auto utf8Path(const std::filesystem::path &path) -> std::string
{
    const auto value = path.u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}
#endif
} // namespace

struct ZipReader::Impl
{
#if SZX_COMPRESSION_HAS_MINIZIP
    void *handle = nullptr;
#endif
    std::vector<ZipEntry> entries;

    ~Impl()
    {
#if SZX_COMPRESSION_HAS_MINIZIP
        if (handle != nullptr)
        {
            static_cast<void>(mz_zip_reader_close(handle));
            mz_zip_reader_delete(&handle);
        }
#endif
    }
};

ZipReader::ZipReader() : impl_(std::make_unique<Impl>()) {}
ZipReader::ZipReader(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ZipReader::~ZipReader()
    = default;
ZipReader::ZipReader(ZipReader &&) noexcept = default;
ZipReader &ZipReader::operator=(ZipReader &&) noexcept = default;

auto ZipReader::Open(const std::filesystem::path &path) -> SZX::COMMON::Result<ZipReader>
{
#if !SZX_COMPRESSION_HAS_MINIZIP
    static_cast<void>(path);
    return failure<ZipReader>(ZipError::UnsupportedBackend, "ZIP backend is disabled");
#else
    auto impl = std::make_unique<Impl>();
    impl->handle = mz_zip_reader_create();
    const auto pathUtf8 = utf8Path(path);
    if (impl->handle == nullptr || mz_zip_reader_open_file(impl->handle, pathUtf8.c_str()) != MZ_OK)
        return failure<ZipReader>(ZipError::Io, "unable to open ZIP archive");

    auto status = mz_zip_reader_goto_first_entry(impl->handle);
    while (status == MZ_OK)
    {
        mz_zip_file *entry = nullptr;
        if (mz_zip_reader_entry_get_info(impl->handle, &entry) != MZ_OK || entry == nullptr || entry->filename == nullptr ||
            entry->uncompressed_size < 0)
            return failure<ZipReader>(ZipError::InvalidArchive, "invalid ZIP entry");
        impl->entries.push_back({entry->filename, static_cast<std::uint64_t>(entry->uncompressed_size),
                                 mz_zip_reader_entry_is_dir(impl->handle) == MZ_OK});
        status = mz_zip_reader_goto_next_entry(impl->handle);
    }
    if (status != MZ_END_OF_LIST)
        return failure<ZipReader>(ZipError::InvalidArchive, "unable to enumerate ZIP archive");
    return SZX::COMMON::Result<ZipReader>::Success(ZipReader(std::move(impl)));
#endif
}

auto ZipReader::entries() const -> const std::vector<ZipEntry> &
{
    static const std::vector<ZipEntry> empty;
    return impl_ == nullptr ? empty : impl_->entries;
}

auto ZipReader::read(std::string_view path) const -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
#if !SZX_COMPRESSION_HAS_MINIZIP
    static_cast<void>(path);
    return failure<std::vector<std::uint8_t>>(ZipError::UnsupportedBackend, "ZIP backend is disabled");
#else
    if (impl_ == nullptr || impl_->handle == nullptr || path.empty())
        return failure<std::vector<std::uint8_t>>(ZipError::InvalidArgument, "ZIP path is empty");
    const std::string name(path);
    if (mz_zip_reader_locate_entry(impl_->handle, name.c_str(), 0) != MZ_OK)
        return failure<std::vector<std::uint8_t>>(ZipError::EntryNotFound, "ZIP entry not found");
    mz_zip_file *entry = nullptr;
    if (mz_zip_reader_entry_get_info(impl_->handle, &entry) != MZ_OK || entry == nullptr || entry->uncompressed_size < 0 ||
        entry->uncompressed_size > (std::numeric_limits<int>::max)() || mz_zip_reader_entry_is_dir(impl_->handle) == MZ_OK)
        return failure<std::vector<std::uint8_t>>(ZipError::InvalidArchive, "invalid ZIP file entry");

    std::vector<std::uint8_t> data(static_cast<std::size_t>(entry->uncompressed_size));
    if (mz_zip_reader_entry_open(impl_->handle) != MZ_OK)
        return failure<std::vector<std::uint8_t>>(ZipError::ReadFailed, "unable to open ZIP entry");
    int64_t offset = 0;
    while (offset < entry->uncompressed_size)
    {
        const auto read = mz_zip_reader_entry_read(impl_->handle, data.data() + offset,
                                                    static_cast<int>(entry->uncompressed_size - offset));
        if (read <= 0)
        {
            static_cast<void>(mz_zip_reader_entry_close(impl_->handle));
            return failure<std::vector<std::uint8_t>>(ZipError::ReadFailed, "unable to read ZIP entry");
        }
        offset += read;
    }
    if (mz_zip_reader_entry_close(impl_->handle) != MZ_OK)
        return failure<std::vector<std::uint8_t>>(ZipError::ReadFailed, "unable to close ZIP entry");
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(data));
#endif
}

struct ZipWriter::Impl
{
#if SZX_COMPRESSION_HAS_MINIZIP
    void *handle = nullptr;
    bool closed = false;
#endif

    ~Impl()
    {
#if SZX_COMPRESSION_HAS_MINIZIP
        if (handle != nullptr && !closed)
            static_cast<void>(mz_zip_writer_close(handle));
        if (handle != nullptr)
            mz_zip_writer_delete(&handle);
#endif
    }
};

ZipWriter::ZipWriter() : impl_(std::make_unique<Impl>()) {}
ZipWriter::ZipWriter(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ZipWriter::~ZipWriter()
    = default;
ZipWriter::ZipWriter(ZipWriter &&) noexcept = default;
ZipWriter &ZipWriter::operator=(ZipWriter &&) noexcept = default;

auto ZipWriter::Create(const std::filesystem::path &path) -> SZX::COMMON::Result<ZipWriter>
{
#if !SZX_COMPRESSION_HAS_MINIZIP
    static_cast<void>(path);
    return failure<ZipWriter>(ZipError::UnsupportedBackend, "ZIP backend is disabled");
#else
    auto impl = std::make_unique<Impl>();
    impl->handle = mz_zip_writer_create();
    const auto pathUtf8 = utf8Path(path);
    if (impl->handle == nullptr || mz_zip_writer_open_file(impl->handle, pathUtf8.c_str(), 0, 0) != MZ_OK)
        return failure<ZipWriter>(ZipError::Io, "unable to create ZIP archive");
    return SZX::COMMON::Result<ZipWriter>::Success(ZipWriter(std::move(impl)));
#endif
}

auto ZipWriter::addFile(std::string_view path, std::span<const std::uint8_t> data, ZipCompressionMethod method)
    -> SZX::COMMON::Result<void>
{
#if !SZX_COMPRESSION_HAS_MINIZIP
    static_cast<void>(path);
    static_cast<void>(data);
    static_cast<void>(method);
    return failure<void>(ZipError::UnsupportedBackend, "ZIP backend is disabled");
#else
    if (impl_ == nullptr || impl_->handle == nullptr || impl_->closed || path.empty())
        return failure<void>(ZipError::InvalidArgument, "ZIP writer is not open");
    const auto compressionMethod = methodValue(method);
    if (compressionMethod < 0)
        return failure<void>(ZipError::UnsupportedBackend, "unsupported ZIP compression method");
    if (data.size() > (std::numeric_limits<int32_t>::max)())
        return failure<void>(ZipError::InvalidArgument, "ZIP entry is too large");
    mz_zip_file info = {};
    const std::string name(path);
    info.filename = name.c_str();
    info.compression_method = static_cast<uint16_t>(compressionMethod);
    info.uncompressed_size = static_cast<int64_t>(data.size());
    if (mz_zip_writer_add_buffer(impl_->handle, data.data(), static_cast<int32_t>(data.size()), &info) != MZ_OK)
        return failure<void>(ZipError::WriteFailed, "unable to write ZIP entry");
    return SZX::COMMON::Result<void>::Success();
#endif
}

auto ZipWriter::close() -> SZX::COMMON::Result<void>
{
#if !SZX_COMPRESSION_HAS_MINIZIP
    return failure<void>(ZipError::UnsupportedBackend, "ZIP backend is disabled");
#else
    if (impl_ == nullptr || impl_->handle == nullptr || impl_->closed)
        return failure<void>(ZipError::InvalidArgument, "ZIP writer is not open");
    if (mz_zip_writer_close(impl_->handle) != MZ_OK)
        return failure<void>(ZipError::WriteFailed, "unable to close ZIP archive");
    impl_->closed = true;
    return SZX::COMMON::Result<void>::Success();
#endif
}

} // namespace SZX::COMPRESSION
