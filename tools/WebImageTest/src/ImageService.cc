#include "ImageService.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <new>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <utility>
#include <vector>
#include "FileHelper.hpp"
#include "ImageCatalog.h"
#include "Serializer.hpp"
#include "UUID.hpp"

namespace SZX::WEBIMAGE
{
inline namespace v1
{
namespace
{
template <typename T>
auto fail(ErrorCode code) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(code), {});
}
auto errorResponse(ErrorCode code, int status) -> SZX::NETWORK::HttpServerResponse
{
    return {.statusCode = status,
            .body       = SZX::COMMON::Json{{"code", static_cast<int>(code)}}.dump(),
            .headers    = {{"Content-Type", "application/json; charset=utf-8"}, {"Cache-Control", "no-store"}}};
}
auto sourceFailure(int code) -> SZX::NETWORK::HttpServerResponse
{
    int status = 500;
    switch (static_cast<ErrorCode>(code))
    {
    case ErrorCode::InvalidConfiguration:
    case ErrorCode::InvalidImageName:
        status = 400;
        break;
    case ErrorCode::ImageNotFound:
        status = 404;
        break;
    case ErrorCode::ImageLoading:
        status = 202;
        break;
    case ErrorCode::ImageBusy:
        status = 429;
        break;
    case ErrorCode::ImageNotLoaded:
    case ErrorCode::SnapshotMismatch:
        status = 409;
        break;
    case ErrorCode::ImageTooLarge:
    case ErrorCode::RangeRequired:
        status = 413;
        break;
    case ErrorCode::ImageDecodeFailed:
    case ErrorCode::UnsupportedPixels:
        status = 422;
        break;
    case ErrorCode::ResourceExhausted:
        status = 503;
        break;
    default:
        break;
    }
    auto result = errorResponse(static_cast<ErrorCode>(code), status);
    if (status == 202 || status == 429)
        result.headers["Retry-After"] = "1";
    return result;
}
auto jsonResponse(const std::string &body) -> SZX::NETWORK::HttpServerResponse
{
    return {.body = body, .headers = {{"Content-Type", "application/json; charset=utf-8"}, {"Cache-Control", "no-store"}}};
}
auto rawResponse(const SZX::NETWORK::HttpServerRequest &request, std::string_view data, int width, int height,
                 const std::string &id) -> SZX::NETWORK::HttpServerResponse
{
    constexpr std::size_t maxChunk = 1024U * 1024U;
    SZX::NETWORK::HttpServerResponse result;
    result.headers   = {{"Content-Type", "application/octet-stream"},
                        {"Cache-Control", "no-store"},
                        {"Accept-Ranges", "bytes"},
                        {"X-Pixel-Format", "BGRA8"},
                        {"X-Image-Width", std::to_string(width)},
                        {"X-Image-Height", std::to_string(height)},
                        {"X-Image-Stride", std::to_string(static_cast<std::size_t>(width) * 4)},
                        {"X-Image-Byte-Length", std::to_string(data.size())},
                        {"X-Image-Origin", "top-left"},
                        {"X-Image-Alpha", "straight"},
                        {"X-Image-Id", id}};
    const auto range = request.headers.find("Range");
    if (range == request.headers.end())
    {
        if (data.size() > 16U * maxChunk)
            return sourceFailure(static_cast<int>(ErrorCode::RangeRequired));
        result.body.assign(data);
        return result;
    }
    const std::string_view input = range->second;
    const auto invalid           = [&]
    {
        auto error                     = errorResponse(ErrorCode::InvalidConfiguration, 416);
        error.headers["Content-Range"] = "bytes */" + std::to_string(data.size());
        return error;
    };
    if (!input.starts_with("bytes="))
        return invalid();
    const auto dash = input.find('-', 6);
    if (dash == input.npos)
        return invalid();
    std::size_t first = 0, last = 0;
    const auto a = std::from_chars(input.data() + 6, input.data() + dash, first);
    const auto b = std::from_chars(input.data() + dash + 1, input.data() + input.size(), last);
    if (a.ec != std::errc{} || b.ec != std::errc{} || a.ptr != input.data() + dash || b.ptr != input.data() + input.size() ||
        first > last || first >= data.size() || last >= data.size() || last - first >= maxChunk)
        return invalid();
    result.statusCode = 206;
    result.body.assign(data.substr(first, last - first + 1));
    result.headers["Content-Range"] =
        "bytes " + std::to_string(first) + "-" + std::to_string(last) + "/" + std::to_string(data.size());
    return result;
}
// Encoding is a response representation only; the shared snapshot remains immutable BGRA8.
auto imageResponse(const SZX::NETWORK::HttpServerRequest &request, std::string_view data, int width, int height,
                   const std::string &id) -> SZX::NETWORK::HttpServerResponse
{
    const auto selected = request.query.find("encoding");
    const auto encoding = selected == request.query.end() ? std::string_view{"raw"} : std::string_view{selected->second};
    if (encoding == "raw")
        return rawResponse(request, data, width, height, id);
    if (encoding != "png" && encoding != "jpg")
        return errorResponse(ErrorCode::InvalidConfiguration, 400);
    if (request.headers.contains("Range"))
        return errorResponse(ErrorCode::InvalidConfiguration, 416);
    try
    {
        // OpenCV's encoder accepts an InputArray and does not mutate the source view.
        const cv::Mat bgra(height, width, CV_8UC4, const_cast<char *>(data.data()));
        cv::Mat bgr;
        if (encoding == "jpg")
            cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
        std::vector<unsigned char> encoded;
        const auto parameters =
            encoding == "png" ? std::vector<int>{cv::IMWRITE_PNG_COMPRESSION, 3} : std::vector<int>{cv::IMWRITE_JPEG_QUALITY, 95};
        if (!cv::imencode(encoding == "png" ? ".png" : ".jpg", encoding == "png" ? bgra : bgr, encoded, parameters))
            return errorResponse(ErrorCode::ImageProcessingFailed, 500);
        return {.body    = std::string(encoded.begin(), encoded.end()),
                .headers = {{"Content-Type", encoding == "png" ? "image/png" : "image/jpeg"},
                            {"Cache-Control", "no-store"},
                            {"X-Image-Width", std::to_string(width)},
                            {"X-Image-Height", std::to_string(height)},
                            {"X-Image-Byte-Length", std::to_string(encoded.size())},
                            {"X-Image-Id", id}}};
    } catch (const cv::Exception &)
    {
        return errorResponse(ErrorCode::ImageProcessingFailed, 500);
    }
}
} // namespace

auto ImageSnapshot::raw() const noexcept -> std::string_view
{
    return {reinterpret_cast<const char *>(pixels_.data), pixels_.total() * pixels_.elemSize()};
}
auto ImageSnapshot::preview() const noexcept -> std::string_view
{
    return {reinterpret_cast<const char *>(preview_.data), preview_.total() * preview_.elemSize()};
}
auto ImageSnapshot::Load(const std::filesystem::path &path) -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
{
    using Value = std::shared_ptr<const ImageSnapshot>;
    auto phase  = ErrorCode::ImageDecodeFailed;
    try
    {
        auto image = std::make_shared<ImageSnapshot>();
        {
            const auto size = SZX::COMMON::FileHelper::Size(path);
            if (!size)
                return fail<Value>(ErrorCode::ImageReadFailed);
            if (*size > MaxFileBytes)
                return fail<Value>(ErrorCode::ImageTooLarge);
            const auto bytes = SZX::COMMON::FileHelper::ReadBytes(path, MaxFileBytes);
            if (!bytes)
                return fail<Value>(ErrorCode::ImageReadFailed);
            if (bytes->empty())
                return fail<Value>(ErrorCode::ImageDecodeFailed);
            auto decoded = cv::imdecode(*bytes, cv::IMREAD_UNCHANGED);
            if (decoded.empty())
                return fail<Value>(ErrorCode::ImageDecodeFailed);
            if (static_cast<std::uint64_t>(decoded.cols) * decoded.rows > MaxPixels)
                return fail<Value>(ErrorCode::ImageTooLarge);
            if (decoded.depth() != CV_8U || (decoded.channels() != 1 && decoded.channels() != 3 && decoded.channels() != 4))
                return fail<Value>(ErrorCode::UnsupportedPixels);
            phase = ErrorCode::ImageProcessingFailed;
            if (decoded.channels() == 1)
                cv::cvtColor(decoded, image->pixels_, cv::COLOR_GRAY2BGRA);
            else if (decoded.channels() == 3)
                cv::cvtColor(decoded, image->pixels_, cv::COLOR_BGR2BGRA);
            else
                image->pixels_ = std::move(decoded);
            if (!image->pixels_.isContinuous())
                image->pixels_ = image->pixels_.clone();
        } // Compressed input and non-BGRA decode buffers released before publication.
        const auto &pixels = image->pixels_;
        const double ratio = std::min(1.0, static_cast<double>(PreviewEdge) / std::max(pixels.cols, pixels.rows));
        if (ratio < 1.0)
            cv::resize(pixels, image->preview_,
                       {std::max(1, static_cast<int>(std::lround(pixels.cols * ratio))),
                        std::max(1, static_cast<int>(std::lround(pixels.rows * ratio)))},
                       0, 0, cv::INTER_AREA);
        else
            image->preview_ = pixels;
        if (!image->preview_.isContinuous())
            image->preview_ = image->preview_.clone();
        while ((std::max(pixels.cols, pixels.rows) + (1 << image->maxLevel_) - 1) / (1 << image->maxLevel_) > TileSize)
            ++image->maxLevel_;
        image->id_   = SZX::COMMON::Uuid::v4().toString();
        image->info_ = SZX::COMMON::Json{{"state", "ready"},
                                         {"id", image->id_},
                                         {"width", pixels.cols},
                                         {"height", pixels.rows},
                                         {"channels", 4},
                                         {"depth_bits", 8},
                                         {"format", "BGRA8"},
                                         {"stride", static_cast<std::uint64_t>(pixels.cols) * 4},
                                         {"byte_length", image->raw().size()},
                                         {"content_type", "application/octet-stream"},
                                         {"origin", "top-left"},
                                         {"alpha", "straight"},
                                         {"preview_width", image->preview_.cols},
                                         {"preview_height", image->preview_.rows},
                                         {"preview_byte_length", image->preview().size()},
                                         {"tile_size", TileSize},
                                         {"max_level", image->maxLevel_},
                                         {"tiled", std::max(pixels.cols, pixels.rows) > PreviewEdge}}
                           .dump();
        return SZX::COMMON::Result<Value>::Success(std::move(image));
    } catch (const std::bad_alloc &)
    {
        return fail<Value>(ErrorCode::ResourceExhausted);
    } catch (const cv::Exception &error)
    {
        return fail<Value>(error.code == cv::Error::StsNoMem ? ErrorCode::ResourceExhausted : phase);
    } catch (...)
    {
        return fail<Value>(ErrorCode::UnexpectedFailure);
    }
}

auto ImageSnapshot::tile(int level, int column, int row) const -> SZX::COMMON::Result<RawTile>
{
    try
    {
        if (level < 0 || level > maxLevel_ || column < 0 || row < 0)
            return fail<RawTile>(ErrorCode::InvalidConfiguration);
        const int scale = 1 << level, step = TileSize * scale;
        if (column > (pixels_.cols - 1) / step || row > (pixels_.rows - 1) / step)
            return fail<RawTile>(ErrorCode::InvalidConfiguration);
        const int x = column * step, y = row * step;
        const cv::Rect bounds{x, y, std::min(step, pixels_.cols - x), std::min(step, pixels_.rows - y)};
        const auto roi = pixels_(bounds);
        cv::Mat tile;
        if (scale == 1)
            tile = roi;
        else
            cv::resize(roi, tile, {(bounds.width + scale - 1) / scale, (bounds.height + scale - 1) / scale}, 0, 0,
                       cv::INTER_AREA);
        RawTile result{tile.cols, tile.rows, std::string(static_cast<std::size_t>(tile.cols) * tile.rows * 4, '\0')};
        const std::size_t rowBytes = static_cast<std::size_t>(tile.cols) * 4;
        // ROI step can be the full original width; never expose its row padding/neighbour pixels.
        for (int rowIndex = 0; rowIndex < tile.rows; ++rowIndex)
            std::memcpy(result.bytes.data() + rowBytes * rowIndex, tile.ptr(rowIndex), rowBytes);
        return SZX::COMMON::Result<RawTile>::Success(std::move(result));
    } catch (const std::bad_alloc &)
    {
        return fail<RawTile>(ErrorCode::ResourceExhausted);
    } catch (const cv::Exception &error)
    {
        return fail<RawTile>(error.code == cv::Error::StsNoMem ? ErrorCode::ResourceExhausted : ErrorCode::ImageProcessingFailed);
    } catch (...)
    {
        return fail<RawTile>(ErrorCode::UnexpectedFailure);
    }
}

namespace
{
using Snapshot = SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>;
using Lookup   = std::function<Snapshot(const SZX::NETWORK::HttpServerRequest &)>;
using List     = std::function<SZX::COMMON::Result<std::string>()>;
auto registerSources(SZX::NETWORK::HttpServer &server, Lookup read, Lookup load, List list, const std::filesystem::path &webRoot)
    -> SZX::COMMON::Result<void>
{
    try
    {
        if (!read || !load || !list)
            return fail<void>(ErrorCode::InvalidConfiguration);
        auto configured = server.mountStatic("/", webRoot);
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = server.use(
            [](auto &request, const auto &next)
            {
                auto result                              = next(request);
                result.headers["X-Content-Type-Options"] = "nosniff";
                result.headers["Referrer-Policy"]        = "no-referrer";
                result.headers["Content-Security-Policy"] =
                    "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'";
                return result;
            });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        // One containment boundary for application callbacks, including allocation failures.
        const auto route = [&](const char *method, const char *path, SZX::NETWORK::HttpRouteHandler handler)
        {
            return server.route(method, path,
                                [handler = std::move(handler)](const auto &request)
                                {
                                    try
                                    {
                                        return handler(request);
                                    } catch (const std::bad_alloc &)
                                    {
                                        return errorResponse(ErrorCode::ResourceExhausted, 503);
                                    } catch (...)
                                    {
                                        return errorResponse(ErrorCode::UnexpectedFailure, 500);
                                    }
                                });
        };
        configured = route("GET", "/api/images",
                           [list](const auto &)
                           {
                               const auto items = list();
                               return items.ok ? jsonResponse(items.value) : sourceFailure(items.error);
                           });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        const auto metadata = [read](const auto &request)
        {
            const auto snapshot = read(request);
            return snapshot.ok ? jsonResponse(snapshot.value->info()) : sourceFailure(snapshot.error);
        };
        configured = route("POST", "/api/image/load",
                           [load](const auto &request)
                           {
                               const auto snapshot = load(request);
                               return snapshot.ok ? jsonResponse(snapshot.value->info()) : sourceFailure(snapshot.error);
                           });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = route("GET", "/api/image/load", metadata);
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = route("GET", "/api/image/info", metadata);
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = route("GET", "/api/image",
                           [read](const auto &request)
                           {
                               const auto snapshot = read(request);
                               if (!snapshot.ok)
                                   return sourceFailure(snapshot.error);
                               return imageResponse(request, snapshot.value->raw(), snapshot.value->width(),
                                                    snapshot.value->height(), snapshot.value->id());
                           });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = route("GET", "/api/image/preview",
                           [read](const auto &request)
                           {
                               const auto snapshot = read(request);
                               if (!snapshot.ok)
                                   return sourceFailure(snapshot.error);
                               return imageResponse(request, snapshot.value->preview(), snapshot.value->previewWidth(),
                                                    snapshot.value->previewHeight(), snapshot.value->id());
                           });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = route("GET", "/api/image/tile/{level}/{column}/{row}",
                           [read](const auto &request)
                           {
                               const auto snapshot = read(request);
                               if (!snapshot.ok)
                                   return sourceFailure(snapshot.error);
                               const auto parse = [&](const char *name)
                               {
                                   const auto &text  = request.pathParams.at(name);
                                   int value         = -1;
                                   const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
                                   return result.ec == std::errc{} && result.ptr == text.data() + text.size() ? value : -1;
                               };
                               const auto tile = snapshot.value->tile(parse("level"), parse("column"), parse("row"));
                               return tile.ok ? imageResponse(request, tile.value.bytes, tile.value.width, tile.value.height,
                                                              snapshot.value->id()) :
                                                sourceFailure(tile.error);
                           });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = route("GET", "/api/health", [](const auto &) { return jsonResponse(R"({"status":"ready"})"); });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = server.onRequestError([](const auto &, int status, auto)
                                           { return errorResponse(ErrorCode::UnexpectedFailure, status); });
        return configured.ok ? SZX::COMMON::Result<void>::Success() : fail<void>(ErrorCode::NetworkConfigurationFailed);
    } catch (const std::bad_alloc &)
    {
        return fail<void>(ErrorCode::ResourceExhausted);
    } catch (...)
    {
        return fail<void>(ErrorCode::UnexpectedFailure);
    }
}
} // namespace

auto RegisterRoutes(SZX::NETWORK::HttpServer &server, std::shared_ptr<const ImageSnapshot> snapshot,
                    const std::filesystem::path &webRoot) -> SZX::COMMON::Result<void>
{
    try
    {
        if (!snapshot)
            return fail<void>(ErrorCode::InvalidConfiguration);
        const auto lookup = [snapshot](const auto &)
        {
            return Snapshot::Success(snapshot);
        };
        return registerSources(
            server, lookup, lookup,
            [snapshot]
            {
                return SZX::COMMON::Result<std::string>::Success(SZX::COMMON::Json{
                    {"items",
                     SZX::COMMON::Json::array({{{"name", "image"}, {"size_bytes", snapshot->raw().size()}, {"loadable", true}}})},
                    {"default", "image"}}
                                                                     .dump());
            },
            webRoot);
    } catch (const std::bad_alloc &)
    {
        return fail<void>(ErrorCode::ResourceExhausted);
    } catch (...)
    {
        return fail<void>(ErrorCode::UnexpectedFailure);
    }
}

auto RegisterCatalogRoutes(SZX::NETWORK::HttpServer &server, std::shared_ptr<ImageCatalog> catalog,
                           const std::filesystem::path &webRoot) -> SZX::COMMON::Result<void>
{
    try
    {
        if (!catalog)
            return fail<void>(ErrorCode::InvalidConfiguration);
        const auto name = [](const auto &request)
        {
            const auto found = request.query.find("name");
            return found == request.query.end() ? std::string{} : found->second;
        };
        return registerSources(
            server,
            [catalog, name](const auto &request)
            {
                auto result   = catalog->get(name(request));
                const auto id = request.query.find("id");
                if (result.ok && id != request.query.end() && id->second != result.value->id())
                    return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::SnapshotMismatch);
                return result;
            },
            [catalog, name](const auto &request) { return catalog->load(name(request)); }, [catalog] { return catalog->list(); },
            webRoot);
    } catch (const std::bad_alloc &)
    {
        return fail<void>(ErrorCode::ResourceExhausted);
    } catch (...)
    {
        return fail<void>(ErrorCode::UnexpectedFailure);
    }
}
} // namespace v1
} // namespace SZX::WEBIMAGE
