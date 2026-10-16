#pragma once

#include "Result.hpp"
#include "SzxNetwork/HttpServer.h"

#include <opencv2/core/mat.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace SZX::WEBIMAGE
{
inline namespace v1
{
/** Application-owned codes; network/OpenCV diagnostics never form the HTTP error contract. */
enum class ErrorCode : int
{
    InvalidConfiguration       = 1001,
    ImageReadFailed            = 1002,
    ImageDecodeFailed          = 1003,
    UnsupportedPixels          = 1004,
    ImageTooLarge              = 1005,
    ImageEncodeFailed          = 1006,
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
    NetworkConfigurationFailed = 1101,
    ListenFailed               = 1102,
};

/** Immutable pixels/PNG/preview loaded by the application worker. Tiles encode only in-memory ROIs, never reread the file. */
class ImageSnapshot final
{
public:
    static constexpr std::uintmax_t MaxFileBytes = 512U * 1024U * 1024U;
    static constexpr std::uint64_t MaxPixels     = 256U * 1024U * 1024U;

    static constexpr int TileSize    = 512;
    static constexpr int PreviewEdge = 2048;

    /** Read a fixed administrator-selected path. Supports 8-bit gray/BGR/BGRA only. */
    [[nodiscard]] static auto Load(const std::filesystem::path &path)
        -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>;
    [[nodiscard]] auto png() const noexcept -> const std::string &
    {
        return png_;
    }
    [[nodiscard]] auto preview() const noexcept -> const std::string &
    {
        return preview_;
    }
    /** Encode a bounded tile from immutable in-memory pixels; level is a power-of-two downsample. */
    [[nodiscard]] auto tile(int level, int column, int row) const -> SZX::COMMON::Result<std::string>;
    [[nodiscard]] auto info() const noexcept -> const std::string &
    {
        return info_;
    }

private:
    cv::Mat pixels_;
    std::string png_;
    std::string info_;
    std::string preview_;
    int maxLevel_ = 0;
};

class ImageCatalog;
[[nodiscard]] auto RegisterCatalogRoutes(SZX::NETWORK::HttpServer &server, std::shared_ptr<ImageCatalog> catalog,
                                         const std::filesystem::path &webRoot) -> SZX::COMMON::Result<void>;

/** Configure routes/static hosting while stopped. Snapshot lives through all callbacks. */
[[nodiscard]] auto RegisterRoutes(SZX::NETWORK::HttpServer &server, std::shared_ptr<const ImageSnapshot> snapshot,
                                  const std::filesystem::path &webRoot) -> SZX::COMMON::Result<void>;
} // namespace v1
} // namespace SZX::WEBIMAGE
