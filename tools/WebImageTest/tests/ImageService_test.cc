#include "ImageService.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <future>
#include <gtest/gtest.h>
#include <opencv2/imgcodecs.hpp>
#include <string>
#include <thread>
#include <vector>
#include "FileHelper.hpp"
#include "FolderHelper.hpp"
#include "HttpClient.h"
#include "ImageCatalog.h"
#include "JsonHelper.hpp"
#include "UUID.hpp"

using namespace SZX::WEBIMAGE;
using namespace SZX::TEST::HTTP;
namespace
{
class WebImageTest : public ::testing::Test
{
protected:
    std::filesystem::path directory;
    void SetUp() override
    {
        directory = std::filesystem::temp_directory_path() / ("szx-web-image-" + SZX::COMMON::Uuid::v4().toString());
        EXPECT_TRUE(SZX::COMMON::FolderHelper::EnsureExists(directory));
    }
    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    auto writeImage(const std::string &name, const cv::Mat &pixels) -> std::filesystem::path
    {
        std::vector<unsigned char> bytes;
        EXPECT_TRUE(cv::imencode(".png", pixels, bytes));
        const auto path = directory / std::filesystem::path(std::u8string(name.begin(), name.end()));
        EXPECT_TRUE(SZX::COMMON::FileHelper::WriteBytes(path, bytes));
        return path;
    }
};
auto awaitImage(ImageCatalog &catalog, const std::string &name) -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
{
    auto result = catalog.load(name);
    for (int i = 0; i < 1000 && !result.ok && result.error == static_cast<int>(ErrorCode::ImageLoading); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        result = catalog.get(name);
    }
    return result;
}
auto server() -> std::unique_ptr<SZX::NETWORK::HttpServer>
{
    auto result = SZX::NETWORK::HttpServer::Create({.ioThreadCount = 2});
    if (!result.ok)
        throw std::runtime_error("Test server creation failed");
    return std::move(result.value);
}
} // namespace

TEST_F(WebImageTest, LoadIsCompleteOnlyAfterPackedBgraConversionAndRawPreview)
{
    struct Case
    {
        int type;
        std::array<unsigned char, 4> expected;
    };
    for (const auto &test :
         std::vector<Case>{{CV_8UC1, {17, 17, 17, 255}}, {CV_8UC3, {17, 83, 211, 255}}, {CV_8UC4, {17, 83, 211, 137}}})
    {
        SCOPED_TRACE(test.type);
        const cv::Mat source(9, 13, test.type, cv::Scalar(17, 83, 211, 137));
        const auto loaded = ImageSnapshot::Load(writeImage("fixture.png", source));
        EXPECT_TRUE(loaded.ok);
        if (!loaded.ok)
            continue;
        const auto raw = loaded.value->raw();
        EXPECT_EQ(raw.size(), 13U * 9U * 4U);
        for (std::size_t i = 0; i < raw.size(); ++i)
            EXPECT_EQ(static_cast<unsigned char>(raw[i]), test.expected[i % 4]);
        EXPECT_EQ(loaded.value->preview(), raw);
        const auto info = SZX::COMMON::JsonHelper::Parse(loaded.value->info());
        EXPECT_TRUE(info.has_value());
        if (info)
        {
            EXPECT_EQ((*info)["state"], "ready");
            EXPECT_EQ((*info)["format"], "BGRA8");
            EXPECT_EQ((*info)["channels"], 4);
            EXPECT_EQ((*info)["stride"], 52);
            EXPECT_EQ((*info)["byte_length"], raw.size());
            EXPECT_EQ((*info)["preview_byte_length"], raw.size());
            EXPECT_EQ((*info)["origin"], "top-left");
            EXPECT_EQ((*info)["alpha"], "straight");
            EXPECT_EQ((*info)["content_type"], "application/octet-stream");
            EXPECT_EQ((*info)["id"], loaded.value->id());
            EXPECT_FALSE(loaded.value->id().empty());
            EXPECT_FALSE(info->contains("encoded_bytes"));
        }
    }
}

TEST_F(WebImageTest, LoadFailuresUseOwnedCodesAndNeverPublishCompressedPixels)
{
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "bad.png", "not an image"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "empty.png", ""));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "huge.png", "x"));
    std::filesystem::resize_file(directory / "huge.png", ImageSnapshot::MaxFileBytes + 1);
    const auto sixteen = writeImage("sixteen.png", cv::Mat(2, 2, CV_16UC1, cv::Scalar(2345)));
    for (const auto &[path, code] :
         std::vector<std::pair<std::filesystem::path, ErrorCode>>{{directory / "missing.png", ErrorCode::ImageReadFailed},
                                                                  {directory / "bad.png", ErrorCode::ImageDecodeFailed},
                                                                  {directory / "empty.png", ErrorCode::ImageDecodeFailed},
                                                                  {directory / "huge.png", ErrorCode::ImageTooLarge},
                                                                  {sixteen, ErrorCode::UnsupportedPixels}})
    {
        SCOPED_TRACE(path.string());
        const auto result = ImageSnapshot::Load(path);
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error, static_cast<int>(code));
        EXPECT_TRUE(result.message.empty());
        EXPECT_EQ(result.value, nullptr);
    }
}

TEST_F(WebImageTest, RawRoiTilesPackRowsWithoutNeighbourPaddingAndPreserveAlpha)
{
    cv::Mat source(1025, 2305, CV_8UC4, cv::Scalar(3, 47, 191, 133));
    for (int y = 0; y < source.rows; y++)
        for (int x = 0; x < source.cols; x++)
            source.at<cv::Vec4b>(y, x) =
                cv::Vec4b{static_cast<unsigned char>(x % 251), static_cast<unsigned char>(y % 239), 191, 133};
    const auto path   = writeImage("tiled.png", source);
    const auto result = ImageSnapshot::Load(path);
    EXPECT_TRUE(result.ok);
    if (!result.ok)
        return;
    EXPECT_TRUE(SZX::COMMON::FileHelper::RemoveIfExists(path));
    EXPECT_EQ(result.value->previewWidth(), 2048);
    EXPECT_EQ(result.value->preview().size(),
              static_cast<std::size_t>(result.value->previewWidth()) * result.value->previewHeight() * 4);
    const auto tile = result.value->tile(0, 1, 0);
    EXPECT_TRUE(tile.ok);
    if (tile.ok)
    {
        EXPECT_EQ(tile.value.width, 512);
        EXPECT_EQ(tile.value.height, 512);
        EXPECT_EQ(tile.value.bytes.size(), 512U * 512U * 4U);
        for (const int y : {0, 1, 255, 511})
            for (const int x : {0, 1, 255, 511})
                for (int c = 0; c < 4; c++)
                    EXPECT_EQ(static_cast<unsigned char>(tile.value.bytes[(y * 512 + x) * 4 + c]),
                              source.at<cv::Vec4b>(y, x + 512)[c]);
    }
    const auto edge = result.value->tile(0, 4, 2);
    EXPECT_TRUE(edge.ok);
    if (edge.ok)
    {
        EXPECT_EQ(edge.value.width, 257);
        EXPECT_EQ(edge.value.height, 1);
        EXPECT_EQ(edge.value.bytes.size(), 257U * 4U);
    }
    const auto down = result.value->tile(1, 0, 0);
    EXPECT_TRUE(down.ok);
    if (down.ok)
    {
        EXPECT_EQ(down.value.width, 512);
        EXPECT_EQ(down.value.height, 512);
        EXPECT_EQ(down.value.bytes.size(), 512U * 512U * 4U);
    }
    for (const auto &indices : std::vector<std::array<int, 3>>{{-1, 0, 0}, {99, 0, 0}, {0, -1, 0}, {0, 5, 0}, {0, 0, 3}})
        EXPECT_EQ(result.value->tile(indices[0], indices[1], indices[2]).error,
                  static_cast<int>(ErrorCode::InvalidConfiguration));
}

TEST_F(WebImageTest, ReadMetadataAndDataNeverStartDecodeOnlyPostLoadDoes)
{
    const auto path = writeImage("a.png", cv::Mat(8, 12, CV_8UC3, cv::Scalar(5, 91, 203)));
    auto catalog    = ImageCatalog::Create(directory, "a.png");
    EXPECT_TRUE(catalog.ok);
    if (!catalog.ok)
        return;
    auto http = server();
    EXPECT_TRUE(RegisterCatalogRoutes(*http, catalog.value, SZX_WEB_IMAGE_SOURCE).ok);
    EXPECT_TRUE(http->start().ok);
    for (const auto &url : {"/api/image/info?name=a.png", "/api/image/load?name=a.png", "/api/image?name=a.png",
                            "/api/image/preview?name=a.png", "/api/image/tile/0/0/0?name=a.png"})
        EXPECT_EQ(status(request(http->port(), "GET", url)), 409);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(catalog.value->get("a.png").error, static_cast<int>(ErrorCode::ImageNotLoaded));
    const auto started = request(http->port(), "POST", "/api/image/load?name=a.png");
    EXPECT_TRUE(status(started) == 202 || status(started) == 200);
    auto loaded = catalog.value->get("a.png");
    for (int i = 0; i < 500 && !loaded.ok; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        loaded = catalog.value->get("a.png");
    }
    EXPECT_TRUE(loaded.ok);
    if (!loaded.ok)
        return;
    const auto id = loaded.value->id();
    EXPECT_EQ(status(request(http->port(), "GET", "/api/image/load?name=a.png")), 200);
    EXPECT_TRUE(SZX::COMMON::FileHelper::RemoveIfExists(path));
    const auto raw = request(http->port(), "GET", "/api/image?name=a.png&id=" + id);
    EXPECT_EQ(status(raw), 200);
    EXPECT_EQ(body(raw), loaded.value->raw());
    EXPECT_NE(raw.find("application/octet-stream"), raw.npos);
    EXPECT_NE(raw.find("X-Pixel-Format: BGRA8"), raw.npos);
    EXPECT_NE(raw.find("X-Image-Stride: 48"), raw.npos);
    EXPECT_NE(raw.find("X-Image-Id: " + id), raw.npos);
    const auto partial = request(http->port(), "GET", "/api/image?name=a.png&id=" + id, {}, "Range: bytes=0-9\r\n");
    EXPECT_EQ(status(partial), 206);
    EXPECT_EQ(body(partial), loaded.value->raw().substr(0, 10));
    const auto head = request(http->port(), "HEAD", "/api/image?name=a.png&id=" + id, {}, "Range: bytes=0-9\r\n");
    EXPECT_EQ(status(head), 206);
    EXPECT_TRUE(body(head).empty());
    for (const auto &range : {"bytes=9-0", "bytes=0-999999999", "bytes=0-", "bytes=-10", "bytes=0-1,3-4", "unknown=0-9"})
        EXPECT_EQ(status(request(http->port(), "GET", "/api/image?name=a.png", {}, std::string("Range: ") + range + "\r\n")),
                  416);
    EXPECT_EQ(status(request(http->port(), "GET", "/api/image?name=a.png&id=old-id")), 409);
    EXPECT_EQ(status(request(http->port(), "GET", "/api/image/info?name=..%2Fa.png")), 400);
    EXPECT_EQ(status(request(http->port(), "GET", "/")), 200);
    EXPECT_EQ(status(request(http->port(), "GET", "/app.js")), 200);
    EXPECT_EQ(status(request(http->port(), "GET", "/missing.js")), 404);
    EXPECT_EQ(status(request(http->port(), "GET", "/api/image/tile/no/0/0?name=a.png")), 400);
    const auto preview = request(http->port(), "GET", "/api/image/preview?name=a.png");
    EXPECT_EQ(body(preview), loaded.value->preview());
    std::vector<std::future<std::string>> clients;
    for (int i = 0; i < 8; i++)
        clients.push_back(
            std::async(std::launch::async, [&] { return body(request(http->port(), "GET", "/api/image?name=a.png")); }));
    for (auto &client : clients)
        EXPECT_EQ(client.get(), loaded.value->raw());
    EXPECT_TRUE(http->stop().ok);
}

TEST_F(WebImageTest, LargeRawDataRequiresBoundedRangesAndNeverReturnsPng)
{
    const auto loaded = ImageSnapshot::Load(writeImage("large.png", cv::Mat(2100, 2100, CV_8UC3, cv::Scalar(17, 65, 211))));
    EXPECT_TRUE(loaded.ok);
    if (!loaded.ok)
        return;
    auto http = server();
    EXPECT_TRUE(RegisterRoutes(*http, loaded.value, SZX_WEB_IMAGE_SOURCE).ok);
    EXPECT_TRUE(http->start().ok);
    EXPECT_EQ(status(request(http->port(), "GET", "/api/image")), 413);
    const auto range = request(http->port(), "GET", "/api/image", {}, "Range: bytes=0-1048575\r\n");
    EXPECT_EQ(status(range), 206);
    EXPECT_EQ(body(range), loaded.value->raw().substr(0, 1048576));
    EXPECT_EQ(status(request(http->port(), "GET", "/api/image", {}, "Range: bytes=0-1048576\r\n")), 416);
    const auto tile = request(http->port(), "GET", "/api/image/tile/0/4/4");
    EXPECT_EQ(status(tile), 200);
    EXPECT_EQ(body(tile).size(), 52U * 52U * 4U);
    EXPECT_NE(tile.find("X-Image-Width: 52"), tile.npos);
    EXPECT_TRUE(http->stop().ok);
}

TEST_F(WebImageTest, CatalogListsImmediateSafeFilesAndIsolatesSnapshots)
{
    writeImage("a.png", cv::Mat(3, 7, CV_8UC1, cv::Scalar(88)));
    writeImage("b.PNG", cv::Mat(9, 4, CV_8UC3, cv::Scalar(2, 91, 211)));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "notes.txt", "text"));
    EXPECT_TRUE(SZX::COMMON::FolderHelper::EnsureExists(directory / "nested"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "nested" / "hidden.png", "hidden"));
    std::error_code error;
    std::filesystem::create_symlink(directory / "a.png", directory / "link.png", error);
    auto catalog = ImageCatalog::Create(directory, "a.png");
    EXPECT_TRUE(catalog.ok);
    if (!catalog.ok)
        return;
    auto list = catalog.value->list();
    EXPECT_TRUE(list.ok);
    if (list.ok)
    {
        const auto json = SZX::COMMON::JsonHelper::Parse(list.value);
        if (json)
        {
            EXPECT_EQ((*json)["items"].size(), 2);
            EXPECT_EQ((*json)["default"], "a.png");
        }
    }
    for (const auto &name : {"../a.png", "nested/hidden.png", "nested\\hidden.png", "C:a.png", "/a.png", "notes.txt", "..", "."})
        EXPECT_EQ(catalog.value->load(name).error, static_cast<int>(ErrorCode::InvalidImageName));
    if (!error)
        EXPECT_EQ(catalog.value->load("link.png").error, static_cast<int>(ErrorCode::ImageNotFound));
    EXPECT_EQ(catalog.value->load("missing.png").error, static_cast<int>(ErrorCode::ImageNotFound));
    auto a = awaitImage(*catalog.value, "a.png");
    EXPECT_TRUE(a.ok);
    if (!a.ok)
        return;
    auto b = awaitImage(*catalog.value, "b.PNG");
    EXPECT_TRUE(b.ok);
    if (!b.ok)
        return;
    EXPECT_NE(a.value->id(), b.value->id());
    EXPECT_EQ(catalog.value->get("a.png").error, static_cast<int>(ErrorCode::ImageNotLoaded));
    EXPECT_EQ(a.value->raw().size(), 7U * 3U * 4U);
    EXPECT_EQ(ImageCatalog::Create(directory / "missing", "").error, static_cast<int>(ErrorCode::DirectoryUnavailable));
}

TEST_F(WebImageTest, FailedDecodeNeedsExplicitNewPostAndCanRecover)
{
    auto catalog = ImageCatalog::Create(directory, "bad.png");
    EXPECT_TRUE(catalog.ok);
    if (!catalog.ok)
        return;
    auto empty = catalog.value->list();
    EXPECT_TRUE(empty.ok);
    if (empty.ok)
    {
        const auto json = SZX::COMMON::JsonHelper::Parse(empty.value);
        if (json)
            EXPECT_TRUE((*json)["items"].empty());
    }
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "bad.png", "invalid"));
    const auto failed = awaitImage(*catalog.value, "bad.png");
    EXPECT_FALSE(failed.ok);
    EXPECT_EQ(failed.error, static_cast<int>(ErrorCode::ImageDecodeFailed));
    writeImage("bad.png", cv::Mat(3, 3, CV_8UC3, cv::Scalar(3, 9, 71)));
    EXPECT_EQ(catalog.value->get("bad.png").error, static_cast<int>(ErrorCode::ImageDecodeFailed));
    const auto recovered = awaitImage(*catalog.value, "bad.png");
    EXPECT_TRUE(recovered.ok);
    if (recovered.ok)
    {
        EXPECT_EQ(recovered.value->raw().size(), 3U * 3U * 4U);
        EXPECT_EQ(static_cast<unsigned char>(recovered.value->raw()[3]), 255);
    }
}
