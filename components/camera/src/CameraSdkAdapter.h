/**
 * @file CameraSdkAdapter.h
 * @brief 相机 SDK 适配接口 — 屏蔽不同厂商 SDK 的差异。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraTypes.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>


namespace SZXCamera
{

using FramePtr      = std::shared_ptr<const FramePacket>;
using FrameCallback = std::function<void(FramePtr)>;

// ========================================================================
// ICameraSdkAdapter — 单台相机的 SDK 适配接口（纯虚）
// ========================================================================

class ICameraSdkAdapter
{
public:
    virtual ~ICameraSdkAdapter() = default;

    virtual std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr)     = 0;
    virtual bool open(std::string *errorMessage = nullptr)                                    = 0;
    virtual bool close(std::string *errorMessage = nullptr)                                   = 0;
    virtual bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr) = 0;
    virtual bool startGrabbing(std::string *errorMessage = nullptr)                           = 0;
    virtual bool stopGrabbing(std::string *errorMessage = nullptr)                            = 0;
    virtual bool executeSoftwareTrigger(std::string *errorMessage = nullptr)                  = 0;
    virtual void setFrameCallback(FrameCallback callback)                                     = 0;
    virtual CameraInfo info() const                                                           = 0;
};

// ========================================================================
// ICameraSdkFactory — SDK 工厂接口（纯虚）
// ========================================================================

class ICameraSdkFactory
{
public:
    virtual ~ICameraSdkFactory() = default;

    virtual std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr)                                 = 0;
    virtual std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info, std::string *errorMessage = nullptr) = 0;
};

} // namespace SZXCamera
