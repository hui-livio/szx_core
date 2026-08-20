/**
 * @file camera_cv_adapter.h
 * @brief OpenCV 本地目录相机适配器 — 将图片目录模拟为相机帧源。
 */

#pragma once

#include "CameraSdkAdapter.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace SZXCamera
{

class CameraCvAdapter final : public ICameraSdkAdapter
{
public:
    CameraCvAdapter(CameraInfo info, std::vector<std::filesystem::path> imagePaths);
    ~CameraCvAdapter() override;

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
    bool publishNextFrame(std::string *errorMessage = nullptr);
    void streamLoop();

    CameraInfo info_{};
    std::vector<std::filesystem::path> imagePaths_;
    CameraConfig config_{};
    FrameCallback frameCallback_{nullptr};
    std::atomic_bool opened_{false};
    std::atomic_bool streaming_{false};
    std::atomic<std::uint64_t> nextFrameId_{1};
    std::size_t nextImageIndex_{0};
    mutable std::mutex mutex_;
    std::condition_variable streamCv_;
    std::thread streamThread_;
};

class CvCameraSdkFactory final : public ICameraSdkFactory
{
public:
    explicit CvCameraSdkFactory(std::string imageDirectory = {});
    ~CvCameraSdkFactory() override = default;

    std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
    std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info, std::string *errorMessage = nullptr) override;

private:
    bool loadImagePaths(std::string *errorMessage);

    std::string configuredImageDirectory_;
    std::filesystem::path imageDir_;
    std::vector<std::filesystem::path> imagePaths_;
    std::vector<std::string> deviceIds_;
    std::vector<std::string> serialNumbers_;
    std::vector<std::string> logicalNames_;
    std::vector<std::string> vendors_;
    std::vector<std::string> models_;
};

} // namespace SZXCamera
