#include <gtest/gtest.h>

#include "CameraService.h"
#include "IFrameSink.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{

class CollectingSink final : public SZXCamera::IFrameSink
{
public:
    void consume(const std::shared_ptr<const SZXCamera::FramePacket> &frame) override
    {
        if (!frame || !frame->data)
            return;
        std::lock_guard<std::mutex> lock(mutex_);
        frames_.push_back({frame->width, frame->height, frame->channels, frame->pixelFormat, frame->dataSize,
                           frame->data[0], frame->meta.cameraSerialNumber ? frame->meta.cameraSerialNumber : ""});
        condition_.notify_all();
    }

    struct Frame
    {
        int width;
        int height;
        int channels;
        SZXCamera::PixelFormat format;
        std::size_t dataSize;
        std::uint8_t firstByte;
        std::string serial;
    };

    bool waitFor(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, std::chrono::seconds(2), [&] { return frames_.size() >= count; });
    }

    std::vector<Frame> frames() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<Frame> frames_;
};

} // namespace

TEST(CameraServiceCvTest, ReadsFramesFromConfiguredImageDirectory)
{
    const auto imageDir = std::filesystem::temp_directory_path() / "szx_camera_cv_ut";
    std::error_code error;
    std::filesystem::remove_all(imageDir, error);
    ASSERT_TRUE(std::filesystem::create_directories(imageDir, error));

    cv::Mat first(8, 12, CV_8UC3, cv::Scalar(11, 22, 33));
    cv::Mat second(8, 12, CV_8UC3, cv::Scalar(44, 55, 66));
    ASSERT_TRUE(cv::imwrite((imageDir / "001.png").string(), first));
    ASSERT_TRUE(cv::imwrite((imageDir / "002.png").string(), second));

    SZXCamera::CameraServiceConfig config;
    config.backend          = SZXCamera::CameraBackend::CV;
    config.cvImageDirectory = imageDir.string();
    SZXCamera::CameraService service(config);

    std::string errorMessage;
    ASSERT_TRUE(service.discover(&errorMessage)) << errorMessage;
    const auto cameras = service.cameras();
    ASSERT_EQ(cameras.size(), 1U);
    ASSERT_STREQ(cameras[0].serialNumber, "CV_CAMERA_0");
    ASSERT_TRUE(service.initialize(&errorMessage)) << errorMessage;

    auto sink = std::make_shared<CollectingSink>();
    service.addFrameSink(sink);
    ASSERT_TRUE(service.startCamerasBySerialNumbers({cameras[0].serialNumber}, &errorMessage)) << errorMessage;
    ASSERT_TRUE(sink->waitFor(2));
    ASSERT_TRUE(service.stopCamerasBySerialNumbers({cameras[0].serialNumber}, &errorMessage)) << errorMessage;

    const auto frames = sink->frames();
    ASSERT_GE(frames.size(), 2U);
    EXPECT_EQ(frames[0].width, 12);
    EXPECT_EQ(frames[0].height, 8);
    EXPECT_EQ(frames[0].channels, 3);
    EXPECT_EQ(frames[0].format, SZXCamera::PixelFormat::BGR8);
    EXPECT_EQ(frames[0].dataSize, 12U * 8U * 3U);
    EXPECT_EQ(frames[0].firstByte, 11);
    EXPECT_EQ(frames[0].serial, "CV_CAMERA_0");

    EXPECT_TRUE(service.shutdown(&errorMessage)) << errorMessage;
    std::filesystem::remove_all(imageDir, error);
}
