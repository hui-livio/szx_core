#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <opencv2/core/mat.hpp>
#include <string>
#include <string_view>
#include "Result.hpp"
#include "SzxNetwork/HttpServer.h"

namespace SZX::WEBIMAGE
{
inline namespace v1
{
enum class ErrorCode : int
{
    InvalidConfiguration       = 1001,
    ImageReadFailed            = 1002,
    ImageDecodeFailed          = 1003,
    UnsupportedPixels          = 1004,
    ImageTooLarge              = 1005,
    ImageProcessingFailed      = 1006,
    ResourceExhausted          = 1007,
    UnexpectedFailure          = 1008,
    DirectoryUnavailable       = 1010,
    InvalidImageName           = 1011,
    ImageNotFound              = 1012,
    ImageLoading               = 1013,
    ImageBusy                  = 1014,
    ImageNotLoaded             = 1015,
    DirectoryLimit             = 1016,
    RangeRequired              = 1017,
    SnapshotMismatch           = 1018,
    NetworkConfigurationFailed = 1101,
    ListenFailed               = 1102,
};

/** Packed top-left, straight-alpha BGRA8 bytes. Each row contains width*4 bytes. */
struct RawTile final
{
    int width  = 0;
    int height = 0;
    std::string bytes;
};

/** Immutable decoded CV_8UC4 snapshot. No compressed image bytes are retained or served. */
class ImageSnapshot final
{
public:
    static constexpr std::uintmax_t MaxFileBytes = 512U * 1024U * 1024U;
    static constexpr std::uint64_t MaxPixels     = 256U * 1024U * 1024U;
    static constexpr int TileSize                = 512;
    static constexpr int PreviewEdge             = 2048;
    /** Read/decode/convert/pack BGRA8 and prepare raw preview before publishing success. */
    [[nodiscard]] static auto Load(const std::filesystem::path &path)
        -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>;
    [[nodiscard]] auto raw() const noexcept -> std::string_view;
    [[nodiscard]] auto preview() const noexcept -> std::string_view;
    [[nodiscard]] auto width() const noexcept -> int
    {
        return pixels_.cols;
    }
    [[nodiscard]] auto height() const noexcept -> int
    {
        return pixels_.rows;
    }
    [[nodiscard]] auto previewWidth() const noexcept -> int
    {
        return preview_.cols;
    }
    [[nodiscard]] auto previewHeight() const noexcept -> int
    {
        return preview_.rows;
    }
    [[nodiscard]] auto id() const noexcept -> const std::string &
    {
        return id_;
    }
    /** ROI views may be non-contiguous; returned bytes always tightly pack each row. */
    [[nodiscard]] auto tile(int level, int column, int row) const -> SZX::COMMON::Result<RawTile>;
    [[nodiscard]] auto info() const noexcept -> const std::string &
    {
        return info_;
    }

private:
    cv::Mat pixels_;
    cv::Mat preview_;
    std::string id_;
    std::string info_;
    int maxLevel_ = 0;
};

class ImageCatalog;
[[nodiscard]] auto RegisterCatalogRoutes(SZX::NETWORK::HttpServer &server, std::shared_ptr<ImageCatalog> catalog,
                                         const std::filesystem::path &webRoot) -> SZX::COMMON::Result<void>;
/** For tests or preloaded snapshots. Production catalog exposes separate load/read operations. */
[[nodiscard]] auto RegisterRoutes(SZX::NETWORK::HttpServer &server, std::shared_ptr<const ImageSnapshot> snapshot,
                                  const std::filesystem::path &webRoot) -> SZX::COMMON::Result<void>;
} // namespace v1
} // namespace SZX::WEBIMAGE
