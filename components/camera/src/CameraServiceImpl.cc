/**
 * @file CameraServiceImpl.cc
 * @brief 相机服务实现 — 连接 CameraService API 与 CameraManager。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "CameraServiceImpl.h"

#include "FrameSubscription.h"
#include "Logger.h"

#if defined(SZX_HAS_OPENCV)
    #include "cv/camera_cv_adapter.h"
#endif

#if defined(AOI_HAS_IKAP)
    #include "ikap/camera_ikapsdk_adapter.h"
#endif

#if defined(SZX_HAS_HUARAY)
    #include "huaray/camera_huaray_usb_adapter.h"
    #include "huaray/camera_huaraysdk_adapter.h"
#endif

using namespace SZX::LOGGER;

namespace
{
constexpr const char LOG_TAG[] = "Camera";
} // namespace

namespace SZXCamera
{

namespace
{

template <typename Action>
bool runOnSelectedCameras(const std::shared_ptr<CameraManager> &manager, const std::vector<std::string> &serialNumbers,
                          Action &&action, const char *actionName, std::string *errorMessage)
{
    if (!manager)
    {
        if (errorMessage)
            *errorMessage = "camera manager is null";
        LOGM_ERROR(LOG_TAG, "camera service {} failed: manager is null", actionName);
        return false;
    }

    for (const auto &serialNumber : serialNumbers)
    {
        if (serialNumber.empty())
        {
            if (errorMessage)
                *errorMessage = "camera serial number must not be empty";
            LOGM_WARN(LOG_TAG, "camera service {} rejected empty serial number", actionName);
            return false;
        }

        auto camera = manager->getCameraBySerialNumber(serialNumber);
        if (!camera)
        {
            if (errorMessage)
                *errorMessage = "camera not found: " + serialNumber;
            LOGM_WARN(LOG_TAG, "camera service {} skipped missing camera serial={}", actionName, serialNumber);
            return false;
        }

        std::string localError;
        if (!action(*camera, &localError))
        {
            if (errorMessage)
            {
                *errorMessage = localError.empty() ? std::string(actionName) + " failed for camera: " + serialNumber : localError;
            }
            LOGM_ERROR(LOG_TAG, "camera service {} failed serial={} error={}", actionName, serialNumber, localError);
            return false;
        }
    }

    return true;
}
} // namespace

// ========================================================================
// CameraService::Impl
// ========================================================================

std::shared_ptr<ICameraSdkFactory> CameraService::Impl::createFactory(const CameraServiceConfig &config)
{
    if (config.backend == CameraBackend::CV)
    {
#if defined(SZX_HAS_OPENCV)
        return std::make_shared<CvCameraSdkFactory>(config.cvImageDirectory);
#else
        return nullptr;
#endif
    }
#if defined(AOI_HAS_IKAP)
    if (config.backend == CameraBackend::IKAP)
        return std::make_shared<IkapCameraSdkFactory>();
#endif
#if defined(SZX_HAS_HUARAY)
    if (config.backend == CameraBackend::HUARAY)
        return std::make_shared<HuarayCameraSdkFactory>();
    if (config.backend == CameraBackend::HUARAY_USB)
        return std::make_shared<HuarayUsbCameraSdkFactory>();
#endif
    return nullptr;
}

CameraService::Impl::Impl(const CameraServiceConfig &config) :
    manager(std::make_shared<CameraManager>(createFactory(config))),
    pipeline(std::make_shared<CapturePipeline>())
{
}

void CameraService::Impl::bindFrameHandlers()
{
    for (const auto &camera : manager->listCameras())
    {
        camera->setFrameHandler([pipeline = pipeline](FramePtr frame) { pipeline->publish(frame); });
    }
    LOGM_INFO(LOG_TAG, "camera service bound frame handlers to pipeline");
}

// ========================================================================
// CameraService (PIMPL facade)
// ========================================================================

CameraService::CameraService(CameraBackend backend) :
    CameraService(CameraServiceConfig{backend, {}})
{
}

CameraService::CameraService(const CameraServiceConfig &config) :
    impl_(std::make_unique<Impl>(config))
{
}

CameraService::~CameraService() = default;

bool CameraService::discover(std::string *errorMessage)
{
    LOGM_INFO(LOG_TAG, "camera service discover requested");
    const auto ok = impl_->manager->discover(errorMessage);
    if (ok)
        impl_->bindFrameHandlers();
    return ok;
}

bool CameraService::initialize(std::string *errorMessage)
{
    LOGM_INFO(LOG_TAG, "camera service initialize requested");
    return impl_->manager->openAll(errorMessage);
}

bool CameraService::shutdown(std::string *errorMessage)
{
    LOGM_INFO(LOG_TAG, "camera service shutdown requested");
    return impl_->manager->closeAll(errorMessage);
}

bool CameraService::applyCameraConfigs(const std::unordered_map<std::string, CameraConfig> &configs, std::string *errorMessage)
{
    LOGM_INFO(LOG_TAG, "camera service applying configs for {} cameras", configs.size());
    for (const auto &entry : configs)
    {
        auto camera = impl_->manager->getCameraBySerialNumber(entry.first);
        if (!camera)
        {
            if (errorMessage)
                *errorMessage = "camera not found: " + entry.first;
            LOGM_WARN(LOG_TAG, "camera service skipped missing camera serial={}", entry.first);
            return false;
        }

        std::string localError;
        if (!camera->applyConfig(entry.second, &localError))
        {
            if (errorMessage)
                *errorMessage = localError;
            return false;
        }
    }
    return true;
}

std::vector<CameraInfo> CameraService::cameras() const
{
    return impl_->manager->listDiscovered();
}

std::shared_ptr<ICameraDevice> CameraService::getCameraBySerialNumber(const std::string &serialNumber) const
{
    return impl_->manager->getCameraBySerialNumber(serialNumber);
}

std::unordered_map<std::string, CameraRuntimeState> CameraService::cameraRuntimeStates() const
{
    std::unordered_map<std::string, CameraRuntimeState> result;
    for (const auto &camera : impl_->manager->listCameras())
    {
        if (!camera)
            continue;
        result.emplace(camera->info().serialNumber, camera->runtimeState());
    }
    return result;
}

bool CameraService::startCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers, std::string *errorMessage)
{
    LOGM_INFO(LOG_TAG, "camera service starting {} selected cameras", serialNumbers.size());
    return runOnSelectedCameras(
        impl_->manager, serialNumbers, [](ICameraDevice &camera, std::string *localError) { return camera.start(localError); },
        "start", errorMessage);
}

bool CameraService::stopCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers, std::string *errorMessage)
{
    LOGM_INFO(LOG_TAG, "camera service stopping {} selected cameras", serialNumbers.size());
    return runOnSelectedCameras(
        impl_->manager, serialNumbers, [](ICameraDevice &camera, std::string *localError) { return camera.stop(localError); },
        "stop", errorMessage);
}

bool CameraService::executeSoftwareTriggerBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                                          std::string *errorMessage)
{
    LOGM_INFO(LOG_TAG, "camera service executing software trigger on {} selected cameras", serialNumbers.size());
    return runOnSelectedCameras(
        impl_->manager, serialNumbers, [](ICameraDevice &camera, std::string *localError)
        { return camera.executeSoftwareTrigger(localError); }, "software trigger", errorMessage);
}

void CameraService::addFrameSink(std::shared_ptr<IFrameSink> sink)
{
    impl_->pipeline->addSink(std::move(sink));
}

void CameraService::addFrameSinkWithFilter(std::shared_ptr<IFrameSink> sink, const char *const *cameraNames, int nameCount,
                                           const char *const *cameraSerialNumbers, int serialCount, const char *const *taskIds,
                                           int taskCount, bool highExposureOnly)
{
    FrameSubscription sub;
    sub.onlyHighExposure = highExposureOnly;

    if (cameraNames && nameCount > 0)
    {
        sub.cameraNames.reserve(nameCount);
        for (int i = 0; i < nameCount; ++i)
            sub.cameraNames.emplace_back(cameraNames[i]);
    }
    if (cameraSerialNumbers && serialCount > 0)
    {
        sub.cameraSerialNumbers.reserve(serialCount);
        for (int i = 0; i < serialCount; ++i)
            sub.cameraSerialNumbers.emplace_back(cameraSerialNumbers[i]);
    }
    if (taskIds && taskCount > 0)
    {
        sub.taskIds.reserve(taskCount);
        for (int i = 0; i < taskCount; ++i)
            sub.taskIds.emplace_back(taskIds[i]);
    }

    impl_->pipeline->addSink(std::move(sink), std::move(sub));
}

void CameraService::clearFrameSinks()
{
    impl_->pipeline->removeAllSinks();
}

CapturePipelineStats CameraService::pipelineStats() const
{
    return impl_->pipeline->stats();
}

void CameraService::resetPipelineStats()
{
    impl_->pipeline->resetStats();
}

} // namespace SZXCamera
