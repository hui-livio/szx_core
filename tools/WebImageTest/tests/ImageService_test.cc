#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "FileHelper.hpp"
#include "FolderHelper.hpp"
#include "HttpClient.h"
#include "ImageCatalog.h"
#include "ImageService.h"
#include "JsonHelper.hpp"
#include "UUID.hpp"

using namespace SZX::WEBIMAGE;
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
        // FolderHelper has no recursive removal; cleanup is limited to this unique test directory.
        std::filesystem::remove_all(directory, ignored);
    }
    auto writeImage(const std::string &name, const cv::Mat &image) -> std::filesystem::path
    {
        const auto path = directory / name;
        std::vector<unsigned char> bytes;
        EXPECT_TRUE(cv::imencode(".png", image, bytes));
        EXPECT_TRUE(SZX::COMMON::FileHelper::WriteBytes(path, bytes));
        return path;
    }
};
} // namespace

TEST_F(WebImageTest, LoadPreservesEightBitGrayBgrAndBgraPixels)
{
    for (const auto type : {CV_8UC1, CV_8UC3, CV_8UC4})
    {
        SCOPED_TRACE(type);
        const cv::Mat input(9, 13, type, cv::Scalar(17, 83, 211, 137));
        const auto result = ImageSnapshot::Load(writeImage("fixture.png", input));
        EXPECT_TRUE(result.ok);
        if (!result.ok)
            continue;
        const auto &png = result.value->png();
        const std::vector<unsigned char> bytes(png.begin(), png.end());
        const auto decoded = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
        EXPECT_EQ(decoded.type(), type);
        EXPECT_EQ(decoded.cols, 13);
        EXPECT_EQ(decoded.rows, 9);
        EXPECT_EQ(cv::norm(decoded, input, cv::NORM_INF), 0);
        const auto info = SZX::COMMON::JsonHelper::Parse(result.value->info());
        EXPECT_TRUE(info.has_value());
        if (info)
        {
            EXPECT_EQ((*info)["width"], 13);
            EXPECT_EQ((*info)["channels"], input.channels());
            EXPECT_EQ((*info)["encoded_bytes"], png.size());
        }
    }
}

TEST_F(WebImageTest, LoadReturnsOwnedCodesWithoutDiagnosticLeakage)
{
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "corrupt.png", "not an image"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "empty.png", ""));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "too-large.png", "x"));
    // FileHelper has no resize API; a sparse test file validates the bound without allocating 64 MiB.
    std::filesystem::resize_file(directory / "too-large.png", ImageSnapshot::MaxFileBytes + 1);
    const auto sixteen = writeImage("sixteen.png", cv::Mat(2, 2, CV_16UC1, cv::Scalar(3000)));
    struct Case
    {
        std::filesystem::path path;
        ErrorCode code;
    };
    for (const auto &test : std::vector<Case>{{directory / "missing.png", ErrorCode::ImageReadFailed},
                                              {directory / "corrupt.png", ErrorCode::ImageDecodeFailed},
                                              {directory / "empty.png", ErrorCode::ImageDecodeFailed},
                                              {sixteen, ErrorCode::UnsupportedPixels},
                                              {directory / "too-large.png", ErrorCode::ImageTooLarge}})
    {
        SCOPED_TRACE(test.path.string());
        const auto got = ImageSnapshot::Load(test.path);
        EXPECT_FALSE(got.ok);
        EXPECT_EQ(got.error, static_cast<int>(test.code));
        EXPECT_TRUE(got.message.empty());
    }
}

TEST_F(WebImageTest, BinaryEndpointUsesMemoryAfterSourceRemovalAndSupportsConcurrentRequests)
{
    const auto path     = writeImage("memory.png", cv::Mat(8, 12, CV_8UC3, cv::Scalar(5, 91, 203)));
    const auto snapshot = ImageSnapshot::Load(path);
    EXPECT_TRUE(snapshot.ok);
    if (!snapshot.ok)
        return;
    auto created = SZX::NETWORK::HttpServer::Create({.ioThreadCount = 2});
    EXPECT_TRUE(created.ok);
    if (!created.ok)
        return;
    auto server = std::move(created.value);
    EXPECT_TRUE(RegisterRoutes(*server, snapshot.value, SZX_WEB_IMAGE_SOURCE).ok);
    EXPECT_TRUE(server->start().ok);
    EXPECT_TRUE(SZX::COMMON::FileHelper::RemoveIfExists(path));
    using namespace SZX::TEST::HTTP;
    const auto image = request(server->port(), "GET", "/api/image");
    EXPECT_EQ(status(image), 200);
    EXPECT_EQ(body(image), snapshot.value->png());
    EXPECT_NE(image.find("image/png"), image.npos);
    EXPECT_NE(image.find("no-store"), image.npos);
    EXPECT_NE(image.find("nosniff"), image.npos);
    const auto partial = request(server->port(), "GET", "/api/image", {}, "Range: bytes=0-9\r\n");
    EXPECT_EQ(status(partial), 206);
    EXPECT_EQ(body(partial), snapshot.value->png().substr(0, 10));
    EXPECT_NE(partial.find("Content-Range: bytes 0-9/"), std::string::npos);
    const auto partialHead = request(server->port(), "HEAD", "/api/image", {}, "Range: bytes=0-9\r\n");
    EXPECT_EQ(status(partialHead), 206);
    EXPECT_TRUE(body(partialHead).empty());
    for (const auto &range : {"bytes=9-0", "bytes=0-999999999", "bytes=0-", "bytes=-10", "bytes=0-1,3-4", "unknown=0-9"})
    {
        EXPECT_EQ(status(request(server->port(), "GET", "/api/image", {}, std::string("Range: ") + range + "\r\n")), 416);
    }
    const auto info = request(server->port(), "GET", "/api/image/info");
    EXPECT_EQ(body(info), snapshot.value->info());
    EXPECT_EQ(status(request(server->port(), "GET", "/")), 200);
    EXPECT_EQ(status(request(server->port(), "GET", "/app.js")), 200);
    EXPECT_EQ(status(request(server->port(), "GET", "/missing.js")), 404);
    EXPECT_EQ(status(request(server->port(), "POST", "/api/image")), 405);
    const auto traversal = status(request(server->port(), "GET", "/%2e%2e/secret.txt"));
    EXPECT_TRUE(traversal == 403 || traversal == 404);
    const auto head = request(server->port(), "HEAD", "/api/image");
    EXPECT_EQ(status(head), 200);
    EXPECT_TRUE(body(head).empty());
    std::vector<std::future<std::string>> clients;
    for (int i = 0; i < 8; ++i)
        clients.push_back(std::async(std::launch::async, [&] { return body(request(server->port(), "GET", "/api/image")); }));
    for (auto &client : clients)
        EXPECT_EQ(client.get(), snapshot.value->png());
    EXPECT_TRUE(server->stop().ok);
}

TEST_F(WebImageTest, PreviewAndTilesPreserveOriginalCoordinatesAndValidateBounds)
{
    cv::Mat source(1025, 2305, CV_8UC3, cv::Scalar(3, 47, 191));
    source.at<cv::Vec3b>(1024, 2304) = cv::Vec3b{9, 17, 251};
    const auto path                  = writeImage("tiled.png", source);
    const auto loaded                = ImageSnapshot::Load(path);
    EXPECT_TRUE(loaded.ok);
    if (!loaded.ok)
        return;
    EXPECT_TRUE(SZX::COMMON::FileHelper::RemoveIfExists(path));
    const auto info = SZX::COMMON::JsonHelper::Parse(loaded.value->info());
    EXPECT_TRUE(info.has_value());
    if (info)
    {
        EXPECT_EQ((*info)["width"], 2305);
        EXPECT_EQ((*info)["height"], 1025);
        EXPECT_EQ((*info)["preview_width"], 2048);
        EXPECT_EQ((*info)["tiled"], true);
        EXPECT_EQ((*info)["tile_size"], 512);
    }
    const auto decode = [](const std::string &png)
    {
        return cv::imdecode(std::vector<unsigned char>(png.begin(), png.end()), cv::IMREAD_UNCHANGED);
    };
    EXPECT_EQ(decode(loaded.value->preview()).cols, 2048);
    const auto edge = loaded.value->tile(0, 4, 2);
    EXPECT_TRUE(edge.ok);
    if (edge.ok)
    {
        const auto pixels = decode(edge.value);
        EXPECT_EQ(pixels.cols, 257);
        EXPECT_EQ(pixels.rows, 1);
        EXPECT_EQ(pixels.at<cv::Vec3b>(0, 256), (cv::Vec3b{9, 17, 251}));
    }
    const auto downsample = loaded.value->tile(1, 0, 0);
    EXPECT_TRUE(downsample.ok);
    if (downsample.ok)
    {
        EXPECT_EQ(decode(downsample.value).cols, 512);
        EXPECT_EQ(decode(downsample.value).rows, 512);
    }
    for (const auto &indices :
         std::vector<std::array<int, 3>>{{-1, 0, 0}, {99, 0, 0}, {0, -1, 0}, {0, 5, 0}, {0, 0, 3}, {1, 999999, 0}})
    {
        SCOPED_TRACE(indices[0]);
        EXPECT_EQ(loaded.value->tile(indices[0], indices[1], indices[2]).error,
                  static_cast<int>(ErrorCode::InvalidConfiguration));
    }
    auto server = SZX::NETWORK::HttpServer::Create({});
    EXPECT_TRUE(server.ok);
    if (!server.ok)
        return;
    EXPECT_TRUE(RegisterRoutes(*server.value, loaded.value, SZX_WEB_IMAGE_SOURCE).ok);
    EXPECT_TRUE(server.value->start().ok);
    using namespace SZX::TEST::HTTP;
    EXPECT_EQ(body(request(server.value->port(), "GET", "/api/image/preview")), loaded.value->preview());
    EXPECT_EQ(body(request(server.value->port(), "GET", "/api/image/tile/0/4/2")), edge.value);
    EXPECT_EQ(status(request(server.value->port(), "GET", "/api/image/tile/0/5/0")), 400);
    EXPECT_EQ(status(request(server.value->port(), "GET", "/api/image/tile/invalid/0/0")), 400);
    EXPECT_TRUE(server.value->stop().ok);
}

TEST_F(WebImageTest, RejectsMissingSnapshotAndStaticRoot)
{
    const auto snapshot = ImageSnapshot::Load(writeImage("fixture.png", cv::Mat(2, 2, CV_8UC1, cv::Scalar(7))));
    auto created        = SZX::NETWORK::HttpServer::Create({});
    EXPECT_TRUE(created.ok);
    if (!created.ok || !snapshot.ok)
        return;
    EXPECT_EQ(RegisterRoutes(*created.value, {}, SZX_WEB_IMAGE_SOURCE).error, static_cast<int>(ErrorCode::InvalidConfiguration));
    EXPECT_EQ(RegisterRoutes(*created.value, snapshot.value, directory / "missing").error,
              static_cast<int>(ErrorCode::NetworkConfigurationFailed));
}


namespace
{
auto awaitImage(ImageCatalog &catalog, const std::string &name, bool retry = false)
    -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
{
    auto result = catalog.get(name, true, retry);
    for (int attempt = 0; attempt < 500 && !result.ok &&
         (result.error == static_cast<int>(ErrorCode::ImageLoading) || result.error == static_cast<int>(ErrorCode::ImageBusy));
         ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        result = catalog.get(name, true);
    }
    return result;
}
} // namespace

TEST_F(WebImageTest, CatalogListsOnlyImmediateImagesAndRejectsUnsafeNames)
{
    writeImage("b.PNG", cv::Mat(4, 5, CV_8UC3, cv::Scalar(4, 55, 200)));
    writeImage("a.png", cv::Mat(2, 3, CV_8UC1, cv::Scalar(88)));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "notes.txt", "not an image"));
    EXPECT_TRUE(SZX::COMMON::FolderHelper::EnsureExists(directory / "nested"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "nested" / "hidden.png", "not visible"));
    std::error_code error;
    std::filesystem::create_symlink(directory / "a.png", directory / "link.png", error);
    const auto catalog = ImageCatalog::Create(directory, "b.PNG");
    EXPECT_TRUE(catalog.ok);
    if (!catalog.ok)
        return;
    const auto list = catalog.value->list();
    EXPECT_TRUE(list.ok);
    if (!list.ok)
        return;
    const auto json = SZX::COMMON::JsonHelper::Parse(list.value);
    EXPECT_TRUE(json.has_value());
    if (!json)
        return;
    EXPECT_EQ((*json)["items"].size(), 2);
    EXPECT_EQ((*json)["items"][0]["name"], "a.png");
    EXPECT_EQ((*json)["items"][1]["name"], "b.PNG");
    EXPECT_EQ((*json)["default"], "b.PNG");
    EXPECT_TRUE((*json)["items"][0]["loadable"].get<bool>());
    EXPECT_EQ(catalog.value->get("a.png", false).error, static_cast<int>(ErrorCode::ImageNotLoaded));
    for (const auto &name : {"../a.png", "nested/hidden.png", "nested\\hidden.png", "C:a.png", "/a.png", "notes.txt", "..", "."})
    {
        SCOPED_TRACE(name);
        EXPECT_EQ(catalog.value->get(name, true).error, static_cast<int>(ErrorCode::InvalidImageName));
    }
    EXPECT_EQ(catalog.value->get("missing.png", true).error, static_cast<int>(ErrorCode::ImageNotFound));
    if (!error)
        EXPECT_EQ(catalog.value->get("link.png", true).error, static_cast<int>(ErrorCode::ImageNotFound));
    EXPECT_EQ(ImageCatalog::Create(directory / "missing", "").error, static_cast<int>(ErrorCode::DirectoryUnavailable));
}

TEST_F(WebImageTest, CatalogLoadsOnWorkerAndDoesNotReturnAnotherImageAfterEviction)
{
    const auto a = writeImage("a.png", cv::Mat(3, 7, CV_8UC3, cv::Scalar(9, 20, 33)));
    writeImage("b.png", cv::Mat(9, 4, CV_8UC3, cv::Scalar(2, 91, 211)));
    auto created = ImageCatalog::Create(directory, "a.png");
    EXPECT_TRUE(created.ok);
    if (!created.ok)
        return;
    auto &catalog     = *created.value;
    const auto aImage = awaitImage(catalog, "a.png");
    EXPECT_TRUE(aImage.ok);
    if (!aImage.ok)
        return;
    auto server = SZX::NETWORK::HttpServer::Create({});
    EXPECT_TRUE(server.ok);
    if (!server.ok)
        return;
    EXPECT_TRUE(RegisterCatalogRoutes(*server.value, created.value, SZX_WEB_IMAGE_SOURCE).ok);
    EXPECT_TRUE(server.value->start().ok);
    using namespace SZX::TEST::HTTP;
    const auto listed = request(server.value->port(), "GET", "/api/images");
    EXPECT_EQ(status(listed), 200);
    EXPECT_NE(body(listed).find("a.png"), std::string::npos);
    EXPECT_EQ(body(request(server.value->port(), "GET", "/api/image?name=a.png")), aImage.value->png());
    EXPECT_EQ(status(request(server.value->port(), "GET", "/api/image/info?name=..%2Fa.png")), 400);
    EXPECT_EQ(status(request(server.value->port(), "GET", "/api/image/info?name=missing.png")), 404);
    const auto bImage = awaitImage(catalog, "b.png");
    EXPECT_TRUE(bImage.ok);
    if (bImage.ok)
    {
        EXPECT_NE(bImage.value->png(), aImage.value->png());
        EXPECT_EQ(body(request(server.value->port(), "GET", "/api/image?name=b.png")), bImage.value->png());
    }
    EXPECT_EQ(status(request(server.value->port(), "GET", "/api/image?name=a.png")), 409);
    EXPECT_EQ(status(request(server.value->port(), "GET", "/api/image/tile/0/0/0?name=a.png")), 409);
    EXPECT_TRUE(SZX::COMMON::FileHelper::RemoveIfExists(a));
    EXPECT_EQ(aImage.value->png().empty(), false); // in-flight shared snapshots remain immutable.
    EXPECT_TRUE(server.value->stop().ok);
}

TEST_F(WebImageTest, CatalogSupportsEmptyDirectoryCorruptImagesAndRetry)
{
    const auto created = ImageCatalog::Create(directory, "moban.png");
    EXPECT_TRUE(created.ok);
    if (!created.ok)
        return;
    const auto listed = created.value->list();
    EXPECT_TRUE(listed.ok);
    if (listed.ok)
    {
        const auto json = SZX::COMMON::JsonHelper::Parse(listed.value);
        if (json)
        {
            EXPECT_TRUE((*json)["items"].empty());
            EXPECT_EQ((*json)["default"], "");
        }
    }
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "bad.png", "invalid"));
    const auto failed = awaitImage(*created.value, "bad.png");
    EXPECT_FALSE(failed.ok);
    EXPECT_EQ(failed.error, static_cast<int>(ErrorCode::ImageDecodeFailed));
    writeImage("bad.png", cv::Mat(3, 3, CV_8UC3, cv::Scalar(3, 9, 71)));
    const auto recovered = awaitImage(*created.value, "bad.png", true);
    EXPECT_TRUE(recovered.ok);
    EXPECT_TRUE(recovered.message.empty());
    EXPECT_EQ(created.value->list().ok, true);
}
