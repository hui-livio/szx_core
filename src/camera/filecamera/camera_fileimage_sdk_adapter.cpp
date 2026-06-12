// src/file_image_camera_sdk_adapter.cpp
#include "camera/filecamera/camera_fileimage_sdk_adapter.h"
#include "common/log/log_macros.h"

#include <algorithm>
#include <chrono>
#include <cctype>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace SZXCamera {
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool isSupportedImage(const std::filesystem::path &p) {
    if (!std::filesystem::is_regular_file(p)) return false;
    const auto ext = toLower(p.extension().string());
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}

} // namespace

FileImageCameraSdkAdapter::FileImageCameraSdkAdapter(
    CameraInfo info, std::filesystem::path imageDir, int playbackFps, bool loop, std::size_t maxFrames)
    : info_(std::move(info)),
      imageDir_(std::move(imageDir)),
      playbackFps_(std::max(1, playbackFps)),
      loop_(loop),
      maxFrames_(maxFrames) {}

FileImageCameraSdkAdapter::~FileImageCameraSdkAdapter() {
    stopGrabbing(nullptr);
    close(nullptr);
}

std::vector<CameraInfo> FileImageCameraSdkAdapter::enumerateDevices(std::string *errorMessage) {
    if (errorMessage) errorMessage->clear();
    return {info_};
}

bool FileImageCameraSdkAdapter::collectImages(std::string *errorMessage) {
    imageFiles_.clear();

    if (!std::filesystem::exists(imageDir_) || !std::filesystem::is_directory(imageDir_)) {
        if (errorMessage) *errorMessage = "image directory does not exist: " + imageDir_.string();
        return false;
    }

    for (const auto &entry : std::filesystem::directory_iterator(imageDir_)) {
        if (isSupportedImage(entry.path())) {
            imageFiles_.push_back(entry.path());
        }
    }

    std::sort(imageFiles_.begin(), imageFiles_.end());
    if (imageFiles_.empty()) {
        if (errorMessage) *errorMessage = "no supported images found in: " + imageDir_.string();
        return false;
    }

    return true;
}

bool FileImageCameraSdkAdapter::open(std::string *errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (opened_) {
        if (errorMessage) errorMessage->clear();
        return true;
    }

    if (!collectImages(errorMessage)) return false;

    opened_ = true;
    nextIndex_ = 0;
    nextFrameId_ = 1;
    if (errorMessage) errorMessage->clear();

    LOG_INFO("file image camera opened logical_name={} image_count={}",
             info_.logicalName, imageFiles_.size());
    return true;
}

bool FileImageCameraSdkAdapter::close(std::string *errorMessage) {
    stopGrabbing(nullptr);

    std::lock_guard<std::mutex> lock(mutex_);
    imageFiles_.clear();
    nextIndex_ = 0;
    opened_ = false;
    if (errorMessage) errorMessage->clear();

    LOG_INFO("file image camera closed logical_name={}", info_.logicalName);
    return true;
}

bool FileImageCameraSdkAdapter::applyConfig(const CameraConfig &config, std::string *errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    if (errorMessage) errorMessage->clear();
    return true;
}

bool FileImageCameraSdkAdapter::startGrabbing(std::string *errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!opened_) {
        if (errorMessage) *errorMessage = "camera is not opened";
        return false;
    }

    if (running_.load(std::memory_order_acquire)) {
        if (errorMessage) errorMessage->clear();
        return true;
    }

    stopRequested_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    worker_ = std::thread([this]() { runLoop(); });

    if (errorMessage) errorMessage->clear();
    LOG_INFO("file image camera started logical_name={} fps={} loop={}",
             info_.logicalName, playbackFps_, loop_);
    return true;
}

bool FileImageCameraSdkAdapter::stopGrabbing(std::string *errorMessage) {
    stopRequested_.store(true, std::memory_order_release);

    if (worker_.joinable()) {
        worker_.join();
    }

    running_.store(false, std::memory_order_release);
    if (errorMessage) errorMessage->clear();
    return true;
}

    bool FileImageCameraSdkAdapter::executeSoftwareTrigger(std::string *errorMessage)
{
    if (errorMessage) {
        *errorMessage = "file image camera does not support software trigger";
    }
    return false;
}


void FileImageCameraSdkAdapter::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

CameraInfo FileImageCameraSdkAdapter::info() const {
    return info_;
}

FramePtr FileImageCameraSdkAdapter::buildFrame(const std::filesystem::path &imagePath) {
    cv::Mat img = cv::imread(imagePath.string(), cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        LOG_WARN("failed to read image: {}", imagePath.string());
        return nullptr;
    }

    if (img.depth() != CV_8U) {
        cv::Mat converted;
        const double scale = (img.depth() == CV_16U) ? (1.0 / 256.0) : 1.0;
        img.convertTo(converted, CV_MAKETYPE(CV_8U, img.channels()), scale);
        img = converted;
    }

    if (img.channels() == 4) {
        cv::Mat bgr;
        cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
        img = bgr;
    }

    if (img.channels() != 1 && img.channels() != 3) {
        LOG_WARN("unsupported channel count={} file={}", img.channels(), imagePath.string());
        return nullptr;
    }

    if (!img.isContinuous()) {
        img = img.clone();
    }

    auto packet = std::make_shared<FramePacket>();
    packet->meta.cameraName = info_.logicalName;
    packet->meta.cameraSerialNumber = info_.serialNumber;
    packet->meta.frameId = nextFrameId_++;
    packet->meta.timestamp = std::chrono::system_clock::now();
    packet->meta.taskId = "file_image_camera";

    packet->width = img.cols;
    packet->height = img.rows;
    packet->channels = img.channels();
    packet->pixelFormat = (img.channels() == 1) ? PixelFormat::Mono8 : PixelFormat::BGR8;

    const std::size_t bytes = static_cast<std::size_t>(img.total() * img.elemSize());
    packet->data.assign(img.data, img.data + bytes);

    return packet;
}

void FileImageCameraSdkAdapter::runLoop() {
    const auto period = std::chrono::milliseconds(1000 / std::max(1, playbackFps_));
    auto nextTick = std::chrono::steady_clock::now();
    std::size_t emitted = 0;

    while (!stopRequested_.load(std::memory_order_acquire)) {
        std::filesystem::path pathToRead;
        FrameCallback cb;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!opened_ || imageFiles_.empty()) break;

            if (nextIndex_ >= imageFiles_.size()) {
                if (!loop_) break;
                nextIndex_ = 0;
            }

            pathToRead = imageFiles_[nextIndex_++];
            cb = callback_;
        }

        FramePtr frame = buildFrame(pathToRead);
        if (frame && cb) {
            cb(frame);
            ++emitted;
        }

        if (maxFrames_ > 0 && emitted >= maxFrames_) break;

        nextTick += period;
        std::this_thread::sleep_until(nextTick);
        if (nextTick < std::chrono::steady_clock::now()) {
            nextTick = std::chrono::steady_clock::now();
        }
    }

    running_.store(false, std::memory_order_release);
    LOG_INFO("file image camera loop exited logical_name={}", info_.logicalName);
}

// ---------------- Factory ----------------

FileImageCameraSdkFactory::FileImageCameraSdkFactory(
    std::filesystem::path rootDir, int playbackFps, bool loop, std::size_t maxFrames)
    : rootDir_(std::move(rootDir)),
      playbackFps_(std::max(1, playbackFps)),
      loop_(loop),
      maxFrames_(maxFrames) {}

bool FileImageCameraSdkFactory::isCameraName(const std::string &name) {
    return name.rfind("camera_", 0) == 0;
}

bool FileImageCameraSdkFactory::hasSupportedImages(const std::filesystem::path &dir) const {
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return false;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (isSupportedImage(entry.path())) return true;
    }
    return false;
}

std::vector<CameraInfo> FileImageCameraSdkFactory::enumerateDevices(std::string *errorMessage) {
    std::vector<CameraInfo> result;
    std::unordered_map<std::string, std::filesystem::path> mapping;

    if (!std::filesystem::exists(rootDir_) || !std::filesystem::is_directory(rootDir_)) {
        if (errorMessage) *errorMessage = "root dir not found: " + rootDir_.string();
        return result;
    }

    int autoIdx = 0;
    for (const auto &entry : std::filesystem::directory_iterator(rootDir_)) {
        if (!entry.is_directory()) continue;
        if (!hasSupportedImages(entry.path())) continue;

        CameraInfo info;
        const std::string dirName = entry.path().filename().string();
        info.logicalName = isCameraName(dirName) ? dirName : ("camera_" + std::to_string(autoIdx++));
        info.deviceId = entry.path().string();
        info.serialNumber = dirName;
        info.vendor = "FileImage";
        info.model = "ImageSequence";
        info.role = CameraRole::Unknown;

        mapping[info.logicalName] = entry.path();
        result.push_back(std::move(info));
    }

    if (result.empty() && hasSupportedImages(rootDir_)) {
        CameraInfo info;
        info.logicalName = "camera_0";
        info.deviceId = rootDir_.string();
        info.serialNumber = "root";
        info.vendor = "FileImage";
        info.model = "ImageSequence";
        info.role = CameraRole::Unknown;

        mapping[info.logicalName] = rootDir_;
        result.push_back(std::move(info));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        logicalToPath_ = std::move(mapping);
    }

    if (errorMessage) errorMessage->clear();
    return result;
}

std::unique_ptr<ICameraSdkAdapter> FileImageCameraSdkFactory::createAdapter(
    const CameraInfo &info, std::string *errorMessage) {
    std::filesystem::path path;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = logicalToPath_.find(info.logicalName);
        if (it != logicalToPath_.end()) {
            path = it->second;
        }
    }

    if (path.empty()) {
        path = info.deviceId;
    }

    if (path.empty()) {
        if (errorMessage) *errorMessage = "adapter path is empty for " + info.logicalName;
        return nullptr;
    }

    if (errorMessage) errorMessage->clear();
    return std::make_unique<FileImageCameraSdkAdapter>(info, path, playbackFps_, loop_, maxFrames_);
}

} // namespace SZXCamera
