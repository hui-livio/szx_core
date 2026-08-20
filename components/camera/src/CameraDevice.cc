/**
 * @file CameraDevice.cc
 * @brief 相机设备实现 — 转发底层 SDK 操作并管理帧分发。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "CameraDevice.h"

#include "Logger.h"

#include <chrono>

#include <utility>

using namespace SZX::LOGGER;

namespace
{
constexpr const char LOG_TAG[] = "Camera";
}

namespace SZXCamera
{

// ========================================================================
// CameraDevice
// ========================================================================

CameraDevice::CameraDevice(std::unique_ptr<ICameraSdkAdapter> a) :
    adapter_(std::move(a))
{
    if (adapter_)
    {
        info_ = adapter_->info();
        adapter_->setFrameCallback([this](FramePtr frame) { onFrame(std::move(frame)); });
        runtimeState_.state = CameraState::Discovered;
        LOGM_INFO(LOG_TAG, "camera device created logical_name={} serial={}", info_.logicalName, info_.serialNumber);
    }
    else
    {
        runtimeState_.state     = CameraState::Error;
        lastErrorBuf_           = "adapter is null";
        runtimeState_.lastError = lastErrorBuf_.c_str();
        LOGM_ERROR(LOG_TAG, "camera device created with null adapter");
    }
}

CameraDevice::~CameraDevice()
{
    if (adapter_)
    {
        adapter_->setFrameCallback(nullptr);
        std::string ignored;
        stop(&ignored);
        close(&ignored);
    }
}

const CameraInfo &CameraDevice::info() const
{
    return info_;
}

CameraRuntimeState CameraDevice::runtimeState() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeState_;
}

CameraConfig CameraDevice::currentConfig() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool CameraDevice::open(std::string *errorMessage)
{
    if (!adapter_)
    {
        if (errorMessage)
            *errorMessage = "camera adapter is null";
        updateState(CameraState::Error, "camera adapter is null");
        return false;
    }

    LOGM_INFO(LOG_TAG, "opening camera logical_name={}", info_.logicalName);
    std::string localError;
    if (!adapter_->open(&localError))
    {
        if (errorMessage)
            *errorMessage = localError;
        updateState(CameraState::Error, localError);
        LOGM_ERROR(LOG_TAG, "failed to open camera logical_name={} error={}", info_.logicalName, localError);
        return false;
    }

    updateState(CameraState::Opened);
    return true;
}

bool CameraDevice::close(std::string *errorMessage)
{
    if (!adapter_)
        return true;

    LOGM_INFO(LOG_TAG, "closing camera logical_name={}", info_.logicalName);

    {
        const auto state = runtimeState();
        if (state.state == CameraState::Streaming)
        {
            std::string stopError;
            if (!stop(&stopError))
            {
                if (errorMessage)
                    *errorMessage = stopError;
                return false;
            }
        }
    }

    std::string localError;
    if (!adapter_->close(&localError))
    {
        if (errorMessage)
            *errorMessage = localError;
        updateState(CameraState::Error, localError);
        LOGM_ERROR(LOG_TAG, "failed to close camera logical_name={} error={}", info_.logicalName, localError);
        return false;
    }

    updateState(CameraState::Stopped);
    return true;
}

bool CameraDevice::applyConfig(const CameraConfig &config, std::string *errorMessage)
{
    if (!adapter_)
    {
        if (errorMessage)
            *errorMessage = "camera adapter is null";
        updateState(CameraState::Error, "camera adapter is null");
        return false;
    }

    LOGM_INFO(LOG_TAG, "applying config to camera logical_name={}", info_.logicalName);
    std::string localError;
    if (!adapter_->applyConfig(config, &localError))
    {
        if (errorMessage)
            *errorMessage = localError;
        updateState(CameraState::Error, localError);
        LOGM_ERROR(LOG_TAG, "failed to apply config camera={} error={}", info_.logicalName, localError);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }
    updateState(CameraState::Configured);
    return true;
}

bool CameraDevice::start(std::string *errorMessage)
{
    if (!adapter_)
    {
        if (errorMessage)
            *errorMessage = "camera adapter is null";
        updateState(CameraState::Error, "camera adapter is null");
        return false;
    }

    LOGM_INFO(LOG_TAG, "starting grab logical_name={}", info_.logicalName);
    std::string localError;
    if (!adapter_->startGrabbing(&localError))
    {
        if (errorMessage)
            *errorMessage = localError;
        updateState(CameraState::Error, localError);
        LOGM_ERROR(LOG_TAG, "failed to start grab camera={} error={}", info_.logicalName, localError);
        return false;
    }

    updateState(CameraState::Streaming);
    return true;
}

bool CameraDevice::stop(std::string *errorMessage)
{
    if (!adapter_)
        return true;

    LOGM_INFO(LOG_TAG, "stopping grab logical_name={}", info_.logicalName);
    std::string localError;
    if (!adapter_->stopGrabbing(&localError))
    {
        if (errorMessage)
            *errorMessage = localError;
        updateState(CameraState::Error, localError);
        LOGM_ERROR(LOG_TAG, "failed to stop grab camera={} error={}", info_.logicalName, localError);
        return false;
    }

    updateState(CameraState::Stopped);
    return true;
}

bool CameraDevice::executeSoftwareTrigger(std::string *errorMessage)
{
    if (!adapter_)
    {
        if (errorMessage)
            *errorMessage = "camera adapter is null";
        updateState(CameraState::Error, "camera adapter is null");
        return false;
    }

    LOGM_INFO(LOG_TAG, "executing software trigger logical_name={}", info_.logicalName);
    std::string localError;
    if (!adapter_->executeSoftwareTrigger(&localError))
    {
        if (errorMessage)
            *errorMessage = localError;
        updateState(CameraState::Error, localError);
        LOGM_ERROR(LOG_TAG, "failed to execute software trigger camera={} error={}", info_.logicalName, localError);
        return false;
    }

    return true;
}

void CameraDevice::setFrameHandler(FrameHandler handler)
{
    std::lock_guard<std::mutex> lock(mutex_);
    frameHandler_ = std::move(handler);
}

void CameraDevice::updateState(CameraState newState, const std::string &lastError)
{
    std::lock_guard<std::mutex> lock(mutex_);
    runtimeState_.state     = newState;
    lastErrorBuf_           = lastError;
    runtimeState_.lastError = lastErrorBuf_.c_str();
}

void CameraDevice::onFrame(FramePtr frame)
{
    if (!frame)
        return;

    FrameHandler handlerCopy;
    {
        const auto now          = std::chrono::system_clock::now();
        std::uint64_t latencyUs = 0;

        if (frame->meta.timestampMs > 0)
        {
            auto frameTime = std::chrono::system_clock::time_point{std::chrono::milliseconds(frame->meta.timestampMs)};
            latencyUs =
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - frameTime).count());
        }

        std::lock_guard<std::mutex> lock(mutex_);
        runtimeState_.totalFrames        += 1;
        runtimeState_.lastFrameTimeMs     = frame->meta.timestampMs;
        runtimeState_.lastFrameLatencyUs  = latencyUs;

        if (latencyUs > runtimeState_.maxFrameLatencyUs)
        {
            runtimeState_.maxFrameLatencyUs = latencyUs;
        }

        handlerCopy = frameHandler_;
    }

    if (handlerCopy)
    {
        handlerCopy(frame);
    }
}

} // namespace SZXCamera
