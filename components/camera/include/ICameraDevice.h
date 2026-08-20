/**
 * @file ICameraDevice.h
 * @brief 相机设备接口 — 定义采集、参数和帧订阅 API。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraExport.h"
#include "CameraTypes.h"

#include <functional>
#include <memory>
#include <string>


namespace SZXCamera
{

// ========================================================================
// ICameraDevice — 单台相机的对外公开接口（纯虚）
// ========================================================================

class ICameraDevice
{
public:
    using FrameHandler = std::function<void(std::shared_ptr<const FramePacket>)>;

    virtual ~ICameraDevice()                                                                  = default;
    virtual const CameraInfo &info() const                                                    = 0;
    virtual CameraRuntimeState runtimeState() const                                           = 0;
    virtual CameraConfig currentConfig() const                                                = 0;
    virtual bool open(std::string *errorMessage = nullptr)                                    = 0;
    virtual bool close(std::string *errorMessage = nullptr)                                   = 0;
    virtual bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr) = 0;
    virtual bool start(std::string *errorMessage = nullptr)                                   = 0;
    virtual bool stop(std::string *errorMessage = nullptr)                                    = 0;
    virtual bool executeSoftwareTrigger(std::string *errorMessage = nullptr)                  = 0;
    virtual void setFrameHandler(FrameHandler handler)                                        = 0;
};

} // namespace SZXCamera
