#include "camera/core/camera_manager.h"
#include "common/log/log_macros.h"

namespace SZXCamera{
    CameraManager::CameraManager(std::shared_ptr<ICameraSdkFactory> factory)
    : factory_(std::move(factory)){
    }


    bool CameraManager::discover(std::string *errorMessage)
    {
        if (!factory_) {
            if (errorMessage) {
                *errorMessage = "camera sdk factory is null";
            }
            LOG_ERROR("camera manager discover failed: factory is null");
            return false;
        }

        // 拒绝重复创建相机
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& entry : cameras_) {
                if (!entry.second) {
                    continue;
                }

                const auto state = entry.second->runtimeState().state;
                if (state == CameraState::Opened ||
                    state == CameraState::Configured ||
                    state == CameraState::Streaming) {
                    if (errorMessage) {
                        *errorMessage = "discover is not allowed while cameras are active";
                    }
                    LOG_ERROR("camera manager discover rejected because cameras are active");
                    return false;
                }
            }
        }


        std::string localError;
        auto devices = factory_->enumerateDevices(&localError);
        if (!localError.empty()) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            LOG_ERROR("camera manager discover failed: {}", localError);
            return false;
        }

        std::unordered_map<std::string, std::shared_ptr<CameraDevice>> nextCameras;
        for (const auto &info : devices) {
            auto adapter = factory_->createAdapter(info, &localError);
            if (!adapter) {
                if (errorMessage) {
                    *errorMessage = localError.empty() ? "failed to create camera adapter" : localError;
                }
                LOG_ERROR("camera manager failed to create adapter serialNumber={} error={}",
                        info.serialNumber,
                        localError);
                return false;
            }

            auto [it, inserted] = nextCameras.emplace(
                info.serialNumber,
                std::make_shared<CameraDevice>(std::move(adapter)));

            if (!inserted) {
                if (errorMessage) {
                    *errorMessage = "duplicate serialNumber: " + info.serialNumber;
                }
                LOG_ERROR("camera manager discover failed: duplicate serialNumber={}", info.serialNumber);
                return false;
            }

        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            discovered_ = devices;
            cameras_ = std::move(nextCameras);
        }

        LOG_INFO("camera manager discovered {} cameras", discovered_.size());
        return true;
    }

    std::vector<CameraInfo> CameraManager::listDiscovered() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return discovered_;
    }

    std::shared_ptr<CameraDevice> CameraManager::getCameraBySerialNumber(const std::string &serialNumber) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = cameras_.find(serialNumber);
        if (it == cameras_.end()) {
            return nullptr;
        }
        return it->second;
    }

    std::vector<std::shared_ptr<CameraDevice>> CameraManager::listCameras() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<CameraDevice>> result;
        result.reserve(cameras_.size());
        for (const auto &entry : cameras_) {
            result.push_back(entry.second);
        }
        return result;
    }

    bool CameraManager::openAll(std::string *errorMessage)
    {
        for (const auto &camera : listCameras()) {
            std::string localError;
            if (!camera->open(&localError)) {
                if (errorMessage) {
                    *errorMessage = localError;
                }
                return false;
            }
        }
        LOG_INFO("camera manager opened all cameras");
        return true;
    }

    bool CameraManager::closeAll(std::string *errorMessage)
    {
        for (const auto &camera : listCameras()) {
            std::string localError;
            if (!camera->close(&localError)) {
                if (errorMessage) {
                    *errorMessage = localError;
                }
                return false;
            }
        }
        LOG_INFO("camera manager closed all cameras");
        return true;
    }
}
