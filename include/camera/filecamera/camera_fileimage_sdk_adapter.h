#pragma once

#include "camera/camera_core_api.h"
#include "camera/core/camera_sdk_adapter.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace SZXCamera {

class CAMERA_CORE_API FileImageCameraSdkAdapter final : public ICameraSdkAdapter {
public:
    FileImageCameraSdkAdapter(CameraInfo info,
                              std::filesystem::path imageDir,
                              int playbackFps = 5,
                              bool loop = true,
                              std::size_t maxFrames = 0);
    ~FileImageCameraSdkAdapter() override;

    // 兼容你当前接口（虽然这个方法更适合放在 Factory）
    std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;

    bool open(std::string *errorMessage = nullptr) override;
    bool close(std::string *errorMessage = nullptr) override;
    bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr) override;
    bool startGrabbing(std::string *errorMessage = nullptr) override;
    bool stopGrabbing(std::string *errorMessage = nullptr) override;
    bool executeSoftwareTrigger(std::string *errorMessage = nullptr) override;
    void setFrameCallback(FrameCallback callback) override;
    CameraInfo info() const override;

private:
    void runLoop();
    bool collectImages(std::string *errorMessage);
    FramePtr buildFrame(const std::filesystem::path &imagePath);

private:
    mutable std::mutex mutex_;
    CameraInfo info_;
    CameraConfig config_;
    std::filesystem::path imageDir_;
    std::vector<std::filesystem::path> imageFiles_;
    std::size_t nextIndex_{0};

    FrameCallback callback_;
    std::thread worker_;
    std::atomic_bool running_{false};
    std::atomic_bool stopRequested_{false};

    std::uint64_t nextFrameId_{1};
    int playbackFps_{5};
    bool loop_{true};
    std::size_t maxFrames_{0};
    bool opened_{false};
};

class CAMERA_CORE_API FileImageCameraSdkFactory final : public ICameraSdkFactory {
public:
    FileImageCameraSdkFactory(std::filesystem::path rootDir,
                              int playbackFps = 5,
                              bool loop = true,
                              std::size_t maxFrames = 0);

    std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
    std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info,
                                                     std::string *errorMessage = nullptr) override;

private:
    bool hasSupportedImages(const std::filesystem::path &dir) const;
    static bool isCameraName(const std::string &name);

private:
    std::filesystem::path rootDir_;
    int playbackFps_{5};
    bool loop_{true};
    std::size_t maxFrames_{0};

    std::mutex mutex_;
    std::unordered_map<std::string, std::filesystem::path> logicalToPath_;
};

} // namespace SZXCamera
