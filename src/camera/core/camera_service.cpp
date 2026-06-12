#include "camera/core/camera_service.h"

#include "common/log/log_macros.h"

namespace SZXCamera {
    namespace {
        template <typename Action>
        bool runOnSelectedCameras(const std::shared_ptr<CameraManager> &manager,
                                  const std::vector<std::string> &serialNumbers,
                                  Action &&action,
                                  const char *actionName,
                                  std::string *errorMessage)
        {
            if (!manager) {
                if (errorMessage) {
                    *errorMessage = "camera manager is null";
                }
                LOG_ERROR("camera service {} failed: manager is null", actionName);
                return false;
            }

            for (const auto &serialNumber : serialNumbers) {
                if (serialNumber.empty()) {
                    if (errorMessage) {
                        *errorMessage = "camera serial number must not be empty";
                    }
                    LOG_WARN("camera service {} rejected empty serial number", actionName);
                    return false;
                }

                auto camera = manager->getCameraBySerialNumber(serialNumber);
                if (!camera) {
                    if (errorMessage) {
                        *errorMessage = "camera not found: " + serialNumber;
                    }
                    LOG_WARN("camera service {} skipped missing camera serial={}", actionName, serialNumber);
                    return false;
                }

                std::string localError;
                if (!action(*camera, &localError)) {
                    if (errorMessage) {
                        if (localError.empty()) {
                            *errorMessage = std::string(actionName) + " failed for camera: " + serialNumber;
                        } else {
                            *errorMessage = localError;
                        }
                    }
                    LOG_ERROR("camera service {} failed serial={} error={}",
                              actionName,
                              serialNumber,
                              localError);
                    return false;
                }
            }

            return true;
        }
    }

    CameraService::CameraService(std::shared_ptr<ICameraSdkFactory> factory)
        : manager_(std::make_shared<CameraManager>(std::move(factory))),
          pipeline_(std::make_shared<CapturePipeline>())
    {
    }

    bool CameraService::discover(std::string *errorMessage)
    {
        LOG_INFO("camera service discover requested");
        const auto ok = manager_->discover(errorMessage);
        if (ok) {
            bindFrameHandlers();
        }
        return ok;
    }

    bool CameraService::initialize(std::string *errorMessage)
    {
        LOG_INFO("camera service initialize requested");
        return manager_->openAll(errorMessage);
    }

    bool CameraService::shutdown(std::string *errorMessage)
    {
        LOG_INFO("camera service shutdown requested");
        return manager_->closeAll(errorMessage);
    }

    bool CameraService::applyCameraConfigs(const std::unordered_map<std::string, CameraConfig> &configs,
                                           std::string *errorMessage)
    {
        LOG_INFO("camera service applying configs for {} cameras", configs.size());
        for (const auto &entry : configs) {
            auto camera = manager_->getCameraBySerialNumber(entry.first);
            if (!camera) {
                if (errorMessage) {
                    *errorMessage = "camera not found: " + entry.first;
                }
                LOG_WARN("camera service skipped missing camera serial={}", entry.first);
                return false;
            }

            std::string localError;
            if (!camera->applyConfig(entry.second, &localError)) {
                if (errorMessage) {
                    *errorMessage = localError;
                }
                return false;
            }
        }
        return true;
    }

    std::vector<CameraInfo> CameraService::cameras() const
    {
        return manager_->listDiscovered();
    }

    std::shared_ptr<CameraDevice> CameraService::getCameraBySerialNumber(const std::string &serialNumber) const
    {
        return manager_->getCameraBySerialNumber(serialNumber);
    }

    bool CameraService::startCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                                    std::string *errorMessage)
    {
        LOG_INFO("camera service starting {} selected cameras", serialNumbers.size());
        return runOnSelectedCameras(
            manager_,
            serialNumbers,
            [](CameraDevice &camera, std::string *localError) {
                return camera.start(localError);
            },
            "start",
            errorMessage);
    }

    bool CameraService::stopCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                                   std::string *errorMessage)
    {
        LOG_INFO("camera service stopping {} selected cameras", serialNumbers.size());
        return runOnSelectedCameras(
            manager_,
            serialNumbers,
            [](CameraDevice &camera, std::string *localError) {
                return camera.stop(localError);
            },
            "stop",
            errorMessage);
    }

    bool CameraService::executeSoftwareTriggerBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                                          std::string *errorMessage)
    {
        LOG_INFO("camera service executing software trigger on {} selected cameras", serialNumbers.size());
        return runOnSelectedCameras(
            manager_,
            serialNumbers,
            [](CameraDevice &camera, std::string *localError) {
                return camera.executeSoftwareTrigger(localError);
            },
            "software trigger",
            errorMessage);
    }

    void CameraService::addFrameSink(std::shared_ptr<IFrameSink> sink)
    {
        pipeline_->addSink(std::move(sink));
    }

    void CameraService::addFrameSink(std::shared_ptr<IFrameSink> sink, FrameSubscription subscription)
    {
        pipeline_->addSink(std::move(sink), std::move(subscription));
    }

    void CameraService::clearFrameSinks()
    {
        pipeline_->removeAllSinks();
    }

    void CameraService::bindFrameHandlers()
    {
        for (const auto &camera : manager_->listCameras()) {
            camera->setFrameHandler([pipeline = pipeline_](FramePtr frame) {
                pipeline->publish(frame);
            });
        }
        LOG_INFO("camera service bound frame handlers to pipeline");
    }

    std::unordered_map<std::string, CameraRuntimeState> CameraService::cameraRuntimeStates() const
    {
        std::unordered_map<std::string, CameraRuntimeState> result;

        for (const auto &camera : manager_->listCameras()) {
            if (!camera) {
                continue;
            }
            result.emplace(camera->info().serialNumber, camera->runtimeState());
        }

        return result;
    }

    CapturePipelineStats CameraService::pipelineStats() const
    {
        return pipeline_->stats();
    }

    void CameraService::resetPipelineStats()
    {
        pipeline_->resetStats();
    }
}
