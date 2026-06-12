#include "camera/core/camera_device.h"
#include "common/log/log_macros.h"
#include <utility>

namespace SZXCamera{

    
    CameraDevice::CameraDevice(std::unique_ptr<ICameraSdkAdapter> adapter)
    : adapter_(std::move(adapter)){
        if (adapter_) {
            info_ = adapter_->info();
            adapter_->setFrameCallback([this](FramePtr frame) {
                onFrame(std::move(frame));
            });
            runtimeState_.state = CameraState::Discovered;
            LOG_INFO("camera device created logical_name={} serial={}", info_.logicalName, info_.serialNumber);
        } else {
            runtimeState_.state = CameraState::Error;
            runtimeState_.lastError = "adapter is null";
            LOG_ERROR("camera device created with null adapter");
        }
    }
    CameraDevice::~CameraDevice(){
         if (adapter_) {
            adapter_->setFrameCallback(nullptr);

            std::string ignored;
            stop(&ignored);
            close(&ignored);
        }
    }
    const CameraInfo &CameraDevice::info() const{
        return info_;
    }

    CameraRuntimeState CameraDevice::runtimeState() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return runtimeState_;
    }

    CameraConfig CameraDevice::currentConfig() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    bool CameraDevice::open(std::string *errorMessage){
        if (!adapter_) {
            if (errorMessage) {
                *errorMessage = "camera adapter is null";
            }
            updateState(CameraState::Error, "camera adapter is null");
            return false;
        }

        LOG_INFO("opening camera logical_name={}", info_.logicalName);
        std::string localError;
        if (!adapter_->open(&localError)) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            updateState(CameraState::Error, localError);
            LOG_ERROR("failed to open camera logical_name={} error={}", info_.logicalName, localError);
            return false;
        }

        updateState(CameraState::Opened);
        return true;
    }

    bool CameraDevice::close(std::string *errorMessage){
        if (!adapter_) {
            return true;
        }

        LOG_INFO("closing camera logical_name={}", info_.logicalName);

        {
            const auto state = runtimeState();
            if (state.state == CameraState::Streaming) {
                std::string stopError;
                if (!stop(&stopError)) {
                    if (errorMessage) {
                        *errorMessage = stopError;
                    }
                    return false;
                }
            }
        }

        std::string localError;
        if (!adapter_->close(&localError)) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            updateState(CameraState::Error, localError);
            LOG_ERROR("failed to close camera logical_name={} error={}", info_.logicalName, localError);
            return false;
        }

        updateState(CameraState::Stopped);
        return true;

    }

    bool CameraDevice::applyConfig(const CameraConfig &config, std::string *errorMessage){
        if (!adapter_) {
            if (errorMessage) {
                *errorMessage = "camera adapter is null";
            }
            updateState(CameraState::Error, "camera adapter is null");
            return false;
        }

        LOG_INFO("applying config to camera logical_name={}", info_.logicalName);
        std::string localError;
        if (!adapter_->applyConfig(config, &localError)) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            updateState(CameraState::Error, localError);
            LOG_ERROR("failed to apply config camera={} error={}", info_.logicalName, localError);
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
        if (!adapter_) {
            if (errorMessage) {
                *errorMessage = "camera adapter is null";
            }
            updateState(CameraState::Error, "camera adapter is null");
            return false;
        }

        LOG_INFO("starting grab logical_name={}", info_.logicalName);
        std::string localError;
        if (!adapter_->startGrabbing(&localError)) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            updateState(CameraState::Error, localError);
            LOG_ERROR("failed to start grab camera={} error={}", info_.logicalName, localError);
            return false;
        }

        updateState(CameraState::Streaming);
        return true;
    }

    bool CameraDevice::stop(std::string *errorMessage)
    {
        if (!adapter_) {
            return true;
        }

        LOG_INFO("stopping grab logical_name={}", info_.logicalName);
        std::string localError;
        if (!adapter_->stopGrabbing(&localError)) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            updateState(CameraState::Error, localError);
            LOG_ERROR("failed to stop grab camera={} error={}", info_.logicalName, localError);
            return false;
        }

        updateState(CameraState::Stopped);
        return true;
    }

    bool CameraDevice::executeSoftwareTrigger(std::string *errorMessage)
    {
        if (!adapter_) {
            if (errorMessage) {
                *errorMessage = "camera adapter is null";
            }
            updateState(CameraState::Error, "camera adapter is null");
            return false;
        }

        LOG_INFO("executing software trigger logical_name={}", info_.logicalName);

        std::string localError;
        if (!adapter_->executeSoftwareTrigger(&localError)) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            updateState(CameraState::Error, localError);
            LOG_ERROR("failed to execute software trigger camera={} error={}",
                      info_.logicalName,
                      localError);
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
        runtimeState_.state = newState;
        runtimeState_.lastError = lastError;
    }

    void CameraDevice::onFrame(const FramePtr frame)
    {

        if (!frame) {
            return;
        }

        FrameHandler handlerCopy;
        {
            const auto now = std::chrono::system_clock::now();
            std::uint64_t latencyUs = 0;

            if (frame->meta.timestamp.time_since_epoch().count() > 0) {
                latencyUs = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        now - frame->meta.timestamp).count());
            }

            std::lock_guard<std::mutex> lock(mutex_);
            runtimeState_.totalFrames += 1;
            runtimeState_.lastFrameTime = frame->meta.timestamp;
            runtimeState_.lastFrameLatencyUs = latencyUs;

            if (latencyUs > runtimeState_.maxFrameLatencyUs) {
                runtimeState_.maxFrameLatencyUs = latencyUs;
            }

            handlerCopy = frameHandler_;
        }

        LOG_DEBUG("camera device received frame logical_name={} frame_id={}",
                info_.logicalName,
                 frame->meta.frameId);

        if (handlerCopy) {
            handlerCopy(std::move(frame));
        }
    }

}