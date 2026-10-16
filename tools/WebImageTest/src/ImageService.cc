#include "ImageService.h"
#include "ImageCatalog.h"

#include "FileHelper.hpp"
#include "Serializer.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>

#include <new>
#include <utility>
#include <vector>

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

auto imageResponse(const SZX::NETWORK::HttpServerRequest &request, const std::string &png) -> SZX::NETWORK::HttpServerResponse
{
    try
    {
        constexpr std::size_t maxChunk = 1024U * 1024U;
        const auto range               = request.headers.find("Range");
        if (range == request.headers.end())
        {
            if (png.size() > 16U * maxChunk)
                return errorResponse(ErrorCode::RangeRequired, 413);
            return {.body    = png,
                    .headers = {{"Content-Type", "image/png"}, {"Cache-Control", "no-store"}, {"Accept-Ranges", "bytes"}}};
        }
        const std::string_view input = range->second;
        if (!input.starts_with("bytes="))
            return errorResponse(ErrorCode::InvalidConfiguration, 416);
        const auto dash = input.find('-', 6);
        if (dash == input.npos)
            return errorResponse(ErrorCode::InvalidConfiguration, 416);
        std::size_t first = 0, last = 0;
        const auto a = std::from_chars(input.data() + 6, input.data() + dash, first);
        const auto b = std::from_chars(input.data() + dash + 1, input.data() + input.size(), last);
        if (a.ec != std::errc{} || b.ec != std::errc{} || a.ptr != input.data() + dash || b.ptr != input.data() + input.size() ||
            first > last || first >= png.size() || last >= png.size() || last - first >= maxChunk)
        {
            auto result                     = errorResponse(ErrorCode::InvalidConfiguration, 416);
            result.headers["Content-Range"] = "bytes */" + std::to_string(png.size());
            return result;
        }
        return {.statusCode = 206,
                .body       = png.substr(first, last - first + 1),
                .headers = {{"Content-Type", "image/png"},
                            {"Cache-Control", "no-store"},
                            {"Accept-Ranges", "bytes"},
                            {"Content-Range",
                             "bytes " + std::to_string(first) + "-" + std::to_string(last) + "/" + std::to_string(png.size())}}};
    } catch (const std::bad_alloc &)
    {
        return errorResponse(ErrorCode::ResourceExhausted, 503);
    } catch (...)
    {
        return errorResponse(ErrorCode::UnexpectedFailure, 500);
    }
}

auto response(const std::string &body, std::string contentType) -> SZX::NETWORK::HttpServerResponse
{
    try
    {
        return {.statusCode = 200,
                .body       = body,
                .headers    = {{"Content-Type", std::move(contentType)}, {"Cache-Control", "no-store"}}};
    } catch (const std::bad_alloc &)
    {
        return errorResponse(ErrorCode::ResourceExhausted, 503);
    } catch (...)
    {
        return errorResponse(ErrorCode::UnexpectedFailure, 500);
    }
}
} // namespace

auto ImageSnapshot::Load(const std::filesystem::path &path) -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
{
    using Outcome = SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>;
    auto phase    = ErrorCode::ImageDecodeFailed;
    try
    {
        const auto size = SZX::COMMON::FileHelper::Size(path);
        if (!size)
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageReadFailed);
        if (*size > MaxFileBytes)
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageTooLarge);
        const auto bytes = SZX::COMMON::FileHelper::ReadBytes(path, MaxFileBytes);
        if (!bytes)
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageReadFailed);
        if (bytes->empty())
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageDecodeFailed);
        auto image = std::make_shared<ImageSnapshot>();
        // Byte loading handles Unicode filesystem paths independently of OpenCV's filename API.
        image->pixels_ = cv::imdecode(*bytes, cv::IMREAD_UNCHANGED);
        if (image->pixels_.empty())
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageDecodeFailed);
        const auto &pixels = image->pixels_;
        if (static_cast<std::uint64_t>(pixels.cols) * static_cast<std::uint64_t>(pixels.rows) > MaxPixels)
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageTooLarge);
        if (pixels.depth() != CV_8U || (pixels.channels() != 1 && pixels.channels() != 3 && pixels.channels() != 4))
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::UnsupportedPixels);
        phase = ErrorCode::ImageEncodeFailed;
        std::vector<unsigned char> encoded;
        // PNG input is already encoded; retain its bytes rather than re-encoding a 237MP source.
        constexpr unsigned char signature[]{137, 80, 78, 71, 13, 10, 26, 10};
        if (bytes->size() >= 8 && std::equal(std::begin(signature), std::end(signature), bytes->begin()))
            image->png_.assign(reinterpret_cast<const char *>(bytes->data()), bytes->size());
        else
        {
            if (!cv::imencode(".png", pixels, encoded, {cv::IMWRITE_PNG_COMPRESSION, 1}))
                return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageEncodeFailed);
            if (encoded.size() > MaxFileBytes)
                return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageTooLarge);
            image->png_.assign(reinterpret_cast<const char *>(encoded.data()), encoded.size());
        }
        const double ratio = std::min(1.0, static_cast<double>(PreviewEdge) / std::max(pixels.cols, pixels.rows));
        cv::Mat preview;
        if (ratio < 1.0)
            cv::resize(pixels, preview,
                       {std::max(1, static_cast<int>(std::lround(pixels.cols * ratio))),
                        std::max(1, static_cast<int>(std::lround(pixels.rows * ratio)))},
                       0, 0, cv::INTER_AREA);
        else
            preview = pixels;
        if (!cv::imencode(".png", preview, encoded, {cv::IMWRITE_PNG_COMPRESSION, 1}))
            return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ImageEncodeFailed);
        image->preview_.assign(reinterpret_cast<const char *>(encoded.data()), encoded.size());
        while ((std::max(pixels.cols, pixels.rows) + (1 << image->maxLevel_) - 1) / (1 << image->maxLevel_) > TileSize)
            ++image->maxLevel_;
        phase        = ErrorCode::UnexpectedFailure;
        image->info_ = SZX::COMMON::Json{{"width", pixels.cols},
                                         {"height", pixels.rows},
                                         {"channels", pixels.channels()},
                                         {"depth_bits", 8},
                                         {"encoded_bytes", image->png_.size()},
                                         {"content_type", "image/png"},
                                         {"preview_width", preview.cols},
                                         {"preview_height", preview.rows},
                                         {"tile_size", TileSize},
                                         {"max_level", image->maxLevel_},
                                         {"tiled", std::max(pixels.cols, pixels.rows) > PreviewEdge}}
                           .dump();
        return Outcome::Success(std::move(image));
    } catch (const std::bad_alloc &)
    {
        return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ResourceExhausted);
    } catch (const cv::Exception &error)
    {
        return fail<std::shared_ptr<const ImageSnapshot>>(error.code == cv::Error::StsNoMem ? ErrorCode::ResourceExhausted :
                                                                                              phase);
    } catch (...)
    {
        return fail<std::shared_ptr<const ImageSnapshot>>(ErrorCode::UnexpectedFailure);
    }
}

auto ImageSnapshot::tile(int level, int column, int row) const -> SZX::COMMON::Result<std::string>
{
    try
    {
        if (level < 0 || level > maxLevel_ || column < 0 || row < 0)
            return fail<std::string>(ErrorCode::InvalidConfiguration);
        const int scale = 1 << level;
        const int step  = TileSize * scale;
        if (column > (pixels_.cols - 1) / step || row > (pixels_.rows - 1) / step)
            return fail<std::string>(ErrorCode::InvalidConfiguration);
        const int x = column * step;
        const int y = row * step;
        const cv::Rect bounds{x, y, std::min(step, pixels_.cols - x), std::min(step, pixels_.rows - y)};
        const auto roi = pixels_(bounds);
        cv::Mat tile;
        if (scale == 1)
            tile = roi;
        else
            cv::resize(roi, tile, {(bounds.width + scale - 1) / scale, (bounds.height + scale - 1) / scale}, 0, 0,
                       cv::INTER_AREA);
        std::vector<unsigned char> encoded;
        if (!cv::imencode(".png", tile, encoded, {cv::IMWRITE_PNG_COMPRESSION, 1}))
            return fail<std::string>(ErrorCode::ImageEncodeFailed);
        return SZX::COMMON::Result<std::string>::Success(
            std::string(reinterpret_cast<const char *>(encoded.data()), encoded.size()));
    } catch (const std::bad_alloc &)
    {
        return fail<std::string>(ErrorCode::ResourceExhausted);
    } catch (const cv::Exception &error)
    {
        return fail<std::string>(error.code == cv::Error::StsNoMem ? ErrorCode::ResourceExhausted : ErrorCode::ImageEncodeFailed);
    } catch (...)
    {
        return fail<std::string>(ErrorCode::UnexpectedFailure);
    }
}

namespace
{
using SourceLookup =
    std::function<SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>(const SZX::NETWORK::HttpServerRequest &, bool)>;
using ListImages = std::function<SZX::COMMON::Result<std::string>()>;
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
        status = 409;
        break;
    case ErrorCode::ImageTooLarge:
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
auto registerSources(SZX::NETWORK::HttpServer &server, SourceLookup source, ListImages list, const std::filesystem::path &webRoot)
    -> SZX::COMMON::Result<void>
{
    try
    {
        if (!source || !list)
            return fail<void>(ErrorCode::InvalidConfiguration);
        // The reusable network component owns traversal, symlink and static MIME handling.
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
                    "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; "
                    "connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'";
                return result;
            });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = server.get("/api/images",
                                [list](const auto &)
                                {
                                    const auto result = list();
                                    return result.ok ? response(result.value, "application/json; charset=utf-8") :
                                                       sourceFailure(result.error);
                                });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured =
            server.get("/api/image",
                       [source](const auto &request)
                       {
                           const auto snapshot = source(request, false);
                           return snapshot.ok ? imageResponse(request, snapshot.value->png()) : sourceFailure(snapshot.error);
                       });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured =
            server.get("/api/image/preview",
                       [source](const auto &request)
                       {
                           const auto snapshot = source(request, false);
                           return snapshot.ok ? response(snapshot.value->preview(), "image/png") : sourceFailure(snapshot.error);
                       });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured =
            server.get("/api/image/tile/{level}/{column}/{row}",
                       [source](const auto &request)
                       {
                           try
                           {
                               const auto snapshot = source(request, false);
                               if (!snapshot.ok)
                                   return sourceFailure(snapshot.error);
                               const auto parse = [&](const char *name)
                               {
                                   const auto &text  = request.pathParams.at(name);
                                   int value         = -1;
                                   const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
                                   return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? value : -1;
                               };
                               const auto tile = snapshot.value->tile(parse("level"), parse("column"), parse("row"));
                               return tile.ok ? response(tile.value, "image/png") : sourceFailure(tile.error);
                           } catch (const std::bad_alloc &)
                           {
                               return errorResponse(ErrorCode::ResourceExhausted, 503);
                           } catch (...)
                           {
                               return errorResponse(ErrorCode::UnexpectedFailure, 500);
                           }
                       });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = server.get("/api/image/info",
                                [source](const auto &request)
                                {
                                    const auto snapshot = source(request, true);
                                    return snapshot.ok ? response(snapshot.value->info(), "application/json; charset=utf-8") :
                                                         sourceFailure(snapshot.error);
                                });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = server.get("/api/health", [](const auto &)
                                { return response(R"({"status":"ready"})", "application/json; charset=utf-8"); });
        if (!configured.ok)
            return fail<void>(ErrorCode::NetworkConfigurationFailed);
        configured = server.onRequestError(
            [](const auto &, int status, auto)
            {
                // Network owns the HTTP status; publish only application-owned numeric codes.
                return SZX::NETWORK::HttpServerResponse{
                    .statusCode = status,
                    .body       = SZX::COMMON::Json{{"code", static_cast<int>(ErrorCode::UnexpectedFailure)}}.dump(),
                    .headers    = {{"Content-Type", "application/json; charset=utf-8"}, {"Cache-Control", "no-store"}}};
            });
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
        return registerSources(
            server, [snapshot](const auto &, bool)
            { return SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>::Success(snapshot); },
            [snapshot]
            {
                return SZX::COMMON::Result<std::string>::Success(SZX::COMMON::Json{
                    {"items",
                     SZX::COMMON::Json::array({{{"name", "image"}, {"size_bytes", snapshot->png().size()}, {"loadable", true}}})},
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
        return registerSources(
            server,
            [catalog](const auto &request, bool allowLoad)
            {
                const auto found = request.query.find("name");
                const auto retry = request.query.find("retry");
                return catalog->get(found == request.query.end() ? std::string{} : found->second, allowLoad,
                                    retry != request.query.end() && retry->second == "1");
            },
            [catalog] { return catalog->list(); }, webRoot);
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
