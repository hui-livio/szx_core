/**
 * @file CameraServiceCv_test.cc
 * @brief CameraServiceCv_test.cc 覆盖的单元测试与测试辅助。
 */

#include "CameraService.h"
#include "IFrameSink.h"

#include <gtest/gtest.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

class ScopedPathCleanup
{
public:
    explicit ScopedPathCleanup(std::filesystem::path path) :
        path_(std::move(path))
    {
    }

    ~ScopedPathCleanup()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

private:
    std::filesystem::path path_;
};

class CollectingSink final : public SZXCamera::IFrameSink
{
public:
    struct CapturedFrame
    {
        int width{0};
        int height{0};
        int channels{0};
        SZXCamera::PixelFormat pixelFormat{SZXCamera::PixelFormat::Unknown};
        std::size_t dataSize{0};
        std::uint8_t firstByte{0};
        std::string serialNumber;
    };

    void consume(const std::shared_ptr<const SZXCamera::FramePacket> &frame) override
    {
        if (!frame || !frame->data)
            return;

        CapturedFrame captured;
        captured.width        = frame->width;
        captured.height       = frame->height;
        captured.channels     = frame->channels;
        captured.pixelFormat  = frame->pixelFormat;
        captured.dataSize     = frame->dataSize;
        captured.firstByte    = frame->data[0];
        captured.serialNumber = frame->meta.cameraSerialNumber ? frame->meta.cameraSerialNumber : "";

        {
            std::lock_guard<std::mutex> lock(mutex_);
            frames_.push_back(std::move(captured));
        }
        cv_.notify_all();
    }

    bool waitForCount(std::size_t expectedCount, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return frames_.size() >= expectedCount; });
    }

    std::vector<CapturedFrame> frames() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<CapturedFrame> frames_;
};

std::filesystem::path makeTempImageDir()
{
    const auto dir = std::filesystem::temp_directory_path() / "aoi_pcb_cv_camera_test_images";
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::create_directories(dir);

    cv::Mat first(8, 12, CV_8UC3, cv::Scalar(11, 22, 33));
    cv::Mat second(8, 12, CV_8UC3, cv::Scalar(44, 55, 66));
    EXPECT_TRUE(cv::imwrite((dir / "001.png").string(), first));
    EXPECT_TRUE(cv::imwrite((dir / "002.png").string(), second));
    return dir;
}

} // namespace

TEST(CameraServiceCvTest, ReadsFramesFromConfiguredImageDirectory)
{
    const auto imageDir = makeTempImageDir();
    const ScopedPathCleanup cleanup(imageDir);

    SZXCamera::CameraServiceConfig config;
    config.backend          = SZXCamera::CameraBackend::CV;
    config.cvImageDirectory = imageDir.string();

    SZXCamera::CameraService service(config);

    std::string error;
    ASSERT_TRUE(service.discover(&error)) << error;

    const auto cameras = service.cameras();
    ASSERT_EQ(cameras.size(), 1);
    ASSERT_STREQ(cameras[0].serialNumber, "CV_CAMERA_0");

    ASSERT_TRUE(service.initialize(&error)) << error;

    auto sink = std::make_shared<CollectingSink>();
    service.addFrameSink(sink);

    ASSERT_TRUE(service.startCamerasBySerialNumbers({cameras[0].serialNumber}, &error)) << error;
    ASSERT_TRUE(sink->waitForCount(2, std::chrono::seconds(2)));
    ASSERT_TRUE(service.stopCamerasBySerialNumbers({cameras[0].serialNumber}, &error)) << error;

    const auto frames = sink->frames();
    ASSERT_GE(frames.size(), 2);
    EXPECT_EQ(frames[0].width, 12);
    EXPECT_EQ(frames[0].height, 8);
    EXPECT_EQ(frames[0].channels, 3);
    EXPECT_EQ(frames[0].pixelFormat, SZXCamera::PixelFormat::BGR8);
    EXPECT_EQ(frames[0].dataSize, 12U * 8U * 3U);
    EXPECT_EQ(frames[0].firstByte, 11);
    EXPECT_EQ(frames[0].serialNumber, "CV_CAMERA_0");

    ASSERT_TRUE(service.shutdown(&error)) << error;
}
