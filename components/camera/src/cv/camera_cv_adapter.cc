/**
 * @file camera_cv_adapter.cc
 * @brief OpenCV 本地目录相机适配器实现。
 */

#include "camera_cv_adapter.h"

#include "FileHelper.hpp"
#include "FolderHelper.hpp"
#include "Logger.h"
#include "TimeHelper.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <utility>

using namespace SZX::LOGGER;
using namespace SZX::COMMON;

namespace
{
constexpr const char LOG_TAG[] = "Camera";

void clearError(std::string *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
}

bool isSupportedImage(const std::filesystem::path &path)
{
    const auto ext = FileHelper::Extension(path);
    return ext == ".bmp" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".tif" || ext == ".tiff";
}

SZXCamera::PixelFormat pixelFormatFor(const cv::Mat &image)
{
    if (image.channels() == 1)
        return SZXCamera::PixelFormat::Mono8;
    if (image.channels() == 3)
        return SZXCamera::PixelFormat::BGR8;
    return SZXCamera::PixelFormat::Unknown;
}

struct CvFrameStorage
{
    SZXCamera::FramePacket packet{};
    cv::Mat image;
    std::string cameraName;
    std::string serialNumber;
};
} // namespace

namespace SZXCamera
{

CameraCvAdapter::CameraCvAdapter(CameraInfo info, std::vector<std::filesystem::path> imagePaths) :
    info_(info),
    imagePaths_(std::move(imagePaths))
{
    config_.triggerMode = TriggerMode::Continuous;
    config_.pixelFormat = PixelFormat::BGR8;
}

CameraCvAdapter::~CameraCvAdapter()
{
    std::string ignored;
    stopGrabbing(&ignored);
    close(&ignored);
}

std::vector<CameraInfo> CameraCvAdapter::enumerateDevices(std::string *errorMessage)
{
    clearError(errorMessage);
    return {info_};
}

bool CameraCvAdapter::open(std::string *errorMessage)
{
    clearError(errorMessage);
    if (imagePaths_.empty())
    {
        if (errorMessage)
            *errorMessage = "cv camera image directory has no supported images";
        return false;
    }
    opened_.store(true, std::memory_order_release);
    return true;
}

bool CameraCvAdapter::close(std::string *errorMessage)
{
    stopGrabbing(errorMessage);
    opened_.store(false, std::memory_order_release);
    clearError(errorMessage);
    return true;
}

bool CameraCvAdapter::applyConfig(const CameraConfig &config, std::string *errorMessage)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        if (config_.pixelFormat == PixelFormat::Unknown)
            config_.pixelFormat = PixelFormat::BGR8;
    }
    clearError(errorMessage);
    return true;
}

bool CameraCvAdapter::startGrabbing(std::string *errorMessage)
{
    if (!opened_.load(std::memory_order_acquire))
    {
        if (errorMessage)
            *errorMessage = "cv camera is not opened";
        return false;
    }

    bool expected = false;
    if (!streaming_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        clearError(errorMessage);
        return true;
    }

    streamThread_ = std::thread([this] { streamLoop(); });
    clearError(errorMessage);
    return true;
}

bool CameraCvAdapter::stopGrabbing(std::string *errorMessage)
{
    streaming_.store(false, std::memory_order_release);
    streamCv_.notify_all();
    if (streamThread_.joinable())
        streamThread_.join();
    clearError(errorMessage);
    return true;
}

bool CameraCvAdapter::executeSoftwareTrigger(std::string *errorMessage)
{
    if (!opened_.load(std::memory_order_acquire))
    {
        if (errorMessage)
            *errorMessage = "cv camera is not opened";
        return false;
    }
    return publishNextFrame(errorMessage);
}

void CameraCvAdapter::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    frameCallback_ = std::move(callback);
}

CameraInfo CameraCvAdapter::info() const
{
    return info_;
}

bool CameraCvAdapter::publishNextFrame(std::string *errorMessage)
{
    std::filesystem::path imagePath;
    FrameCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (imagePaths_.empty())
        {
            if (errorMessage)
                *errorMessage = "cv camera image list is empty";
            return false;
        }
        imagePath       = imagePaths_[nextImageIndex_];
        nextImageIndex_ = (nextImageIndex_ + 1) % imagePaths_.size();
        callback        = frameCallback_;
    }

    cv::Mat image = cv::imread(imagePath.string(), cv::IMREAD_UNCHANGED);
    if (image.empty())
    {
        if (errorMessage)
            *errorMessage = "failed to read image: " + imagePath.string();
        LOGM_WARN(LOG_TAG, "cv camera failed to read image {}", imagePath.string());
        return false;
    }

    if (image.depth() != CV_8U)
        image.convertTo(image, CV_8U);
    if (image.channels() == 4)
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
    else if (image.channels() != 1 && image.channels() != 3)
    {
        if (errorMessage)
            *errorMessage = "unsupported image channel count: " + imagePath.string();
        return false;
    }

    auto storage          = std::make_shared<CvFrameStorage>();
    storage->image        = std::move(image);
    storage->cameraName   = info_.logicalName ? info_.logicalName : "cv_camera";
    storage->serialNumber = info_.serialNumber ? info_.serialNumber : "CV_CAMERA_0";

    storage->packet.meta.cameraName         = storage->cameraName.c_str();
    storage->packet.meta.cameraSerialNumber = storage->serialNumber.c_str();
    storage->packet.meta.frameId            = nextFrameId_.fetch_add(1, std::memory_order_acq_rel);
    storage->packet.meta.timestampMs        = static_cast<std::uint64_t>(TimeHelper::NowTimestamp());
    storage->packet.width                   = storage->image.cols;
    storage->packet.height                  = storage->image.rows;
    storage->packet.channels                = storage->image.channels();
    storage->packet.pixelFormat             = pixelFormatFor(storage->image);
    storage->packet.data                    = storage->image.data;
    storage->packet.dataSize                = storage->image.total() * storage->image.elemSize();

    if (callback)
        callback(std::shared_ptr<const FramePacket>(storage, &storage->packet));

    clearError(errorMessage);
    return true;
}

void CameraCvAdapter::streamLoop()
{
    while (streaming_.load(std::memory_order_acquire))
    {
        std::string error;
        publishNextFrame(&error);
        if (!error.empty())
            LOGM_WARN(LOG_TAG, "cv camera publish frame warning: {}", error);

        std::unique_lock<std::mutex> lock(mutex_);
        streamCv_.wait_for(lock, std::chrono::milliseconds(30), [this] { return !streaming_.load(std::memory_order_acquire); });
    }
}

CvCameraSdkFactory::CvCameraSdkFactory(std::string imageDirectory) :
    configuredImageDirectory_(std::move(imageDirectory))
{
}

bool CvCameraSdkFactory::loadImagePaths(std::string *errorMessage)
{
    imageDir_ = std::filesystem::path(configuredImageDirectory_);
    if (imageDir_.empty())
    {
        if (errorMessage)
            *errorMessage = "cv image directory is required for CameraBackend::CV";
        return false;
    }
    if (!FolderHelper::Exists(imageDir_))
    {
        if (errorMessage)
            *errorMessage = "cv camera image directory does not exist: " + imageDir_.string();
        return false;
    }

    imagePaths_.clear();
    for (const auto &entry : std::filesystem::directory_iterator(imageDir_))
    {
        if (!entry.is_regular_file())
            continue;
        if (isSupportedImage(entry.path()))
            imagePaths_.push_back(entry.path());
    }
    std::sort(imagePaths_.begin(), imagePaths_.end());

    if (imagePaths_.empty())
    {
        if (errorMessage)
            *errorMessage = "cv camera image directory has no supported images: " + imageDir_.string();
        return false;
    }

    clearError(errorMessage);
    return true;
}

std::vector<CameraInfo> CvCameraSdkFactory::enumerateDevices(std::string *errorMessage)
{
    if (!loadImagePaths(errorMessage))
        return {};

    deviceIds_     = {"cv://" + imageDir_.string()};
    serialNumbers_ = {"CV_CAMERA_0"};
    logicalNames_  = {"cv_camera_0"};
    vendors_       = {"OpenCV"};
    models_        = {"LocalImageDirectory"};

    CameraInfo info;
    info.deviceId     = deviceIds_[0].c_str();
    info.serialNumber = serialNumbers_[0].c_str();
    info.logicalName  = logicalNames_[0].c_str();
    info.vendor       = vendors_[0].c_str();
    info.model        = models_[0].c_str();
    info.role         = CameraRole::Top;

    LOGM_INFO(LOG_TAG, "cv camera discovered {} images from {}", imagePaths_.size(), imageDir_.string());
    clearError(errorMessage);
    return {info};
}

std::unique_ptr<ICameraSdkAdapter> CvCameraSdkFactory::createAdapter(const CameraInfo &info, std::string *errorMessage)
{
    if (imagePaths_.empty() && !loadImagePaths(errorMessage))
        return nullptr;

    clearError(errorMessage);
    return std::make_unique<CameraCvAdapter>(info, imagePaths_);
}

} // namespace SZXCamera
