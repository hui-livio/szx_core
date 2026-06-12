#include "common/log/log_macros.h"
#include "camera/core/camera_sdk_adapter.h"

namespace SZXCamera{
    // 这里是一些适配器的公共实现，可以放在基类里，或者直接在具体适配器里实现。
    // 例如 open 和 close 的默认实现可以直接返回 false，表示需要子类重写。
    // 也可以提供一些通用的错误处理、状态管理等功能。

    
    // NullCameraSdkAdapter::NullCameraSdkAdapter(CameraInfo info)
    // : info_(std::move(info))
    // {
    // }

    // bool NullCameraSdkAdapter::open(std::string *errorMessage)
    // {
    //     if (opened_) {
    //         return true;
    //     }

    //     opened_ = true;
    //     LOG_INFO("null camera adapter opened logical_name={}", info_.logicalName);
    //     if (errorMessage) {
    //         errorMessage->clear();
    //     }
    //     return true;
    // }

    // bool NullCameraSdkAdapter::close(std::string *errorMessage)
    // {
    //     grabbing_ = false;
    //     opened_ = false;
    //     LOG_INFO("null camera adapter closed logical_name={}", info_.logicalName);
    //     if (errorMessage) {
    //         errorMessage->clear();
    //     }
    //     return true;
    // }

    // bool NullCameraSdkAdapter::applyConfig(const CameraConfig &config, std::string *errorMessage)
    // {
    //     config_ = config;
    //     LOG_INFO("null camera adapter applied config logical_name={}", info_.logicalName);
    //     if (errorMessage) {
    //         errorMessage->clear();
    //     }
    //     return true;
    // }

    // bool NullCameraSdkAdapter::startGrabbing(std::string *errorMessage)
    // {
    //     if (!opened_) {
    //         if (errorMessage) {
    //             *errorMessage = "camera is not opened";
    //         }
    //         return false;
    //     }

    //     grabbing_ = true;
    //     LOG_INFO("null camera adapter started grabbing logical_name={}", info_.logicalName);
    //     if (errorMessage) {
    //         errorMessage->clear();
    //     }
    //     return true;
    // }

    // bool NullCameraSdkAdapter::stopGrabbing(std::string *errorMessage)
    // {
    //     grabbing_ = false;
    //     LOG_INFO("null camera adapter stopped grabbing logical_name={}", info_.logicalName);
    //     if (errorMessage) {
    //         errorMessage->clear();
    //     }
    //     return true;
    // }

    // void NullCameraSdkAdapter::setFrameCallback(FrameCallback callback)
    // {
    //     callback_ = std::move(callback);
    // }

    // CameraInfo NullCameraSdkAdapter::info() const
    // {
    //     return info_;
    // }

    // std::vector<CameraInfo> NullCameraSdkFactory::enumerateDevices(std::string *errorMessage)
    // {
    //     if (errorMessage) {
    //         errorMessage->clear();
    //     }
    //     LOG_INFO("null camera sdk factory enumerateDevices called, returning empty list");
    //     return {};
    // }

    // std::unique_ptr<ICameraSdkAdapter> NullCameraSdkFactory::createAdapter(const CameraInfo &info,
    //                                                                     std::string *errorMessage)
    // {
    //     if (errorMessage) {
    //         errorMessage->clear();
    //     }
    //     LOG_INFO("null camera sdk factory createAdapter logical_name={}", info.logicalName);
    //     return std::make_unique<NullCameraSdkAdapter>(info);
    // }


}