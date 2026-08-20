/**
 * @file CameraDevice.h
 * @brief 相机设备实现 — 封装单个相机的连接、采集和参数访问。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraSdkAdapter.h"
#include "CameraTypes.h"
#include "ICameraDevice.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace SZXCamera
{

// ========================================================================
// CameraDevice — 内部实现类（不导出，继承 ICameraDevice）
// ========================================================================

class CameraDevice final : public ICameraDevice
{
public:
    explicit CameraDevice(std::unique_ptr<ICameraSdkAdapter> adapter);
    ~CameraDevice() override;

    CameraDevice(const CameraDevice &)            = delete;
    CameraDevice &operator=(const CameraDevice &) = delete;

    const CameraInfo &info() const override;
    CameraRuntimeState runtimeState() const override;
    CameraConfig currentConfig() const override;

    bool open(std::string *errorMessage = nullptr) override;
    bool close(std::string *errorMessage = nullptr) override;
    bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr) override;
    bool start(std::string *errorMessage = nullptr) override;
    bool stop(std::string *errorMessage = nullptr) override;
    bool executeSoftwareTrigger(std::string *errorMessage = nullptr) override;

    void setFrameHandler(FrameHandler handler) override;

private:
    void updateState(CameraState newState, const std::string &lastError = "");
    void onFrame(FramePtr frame);

    std::unique_ptr<ICameraSdkAdapter> adapter_;
    CameraInfo info_;
    CameraConfig config_;
    CameraRuntimeState runtimeState_;
    FrameHandler frameHandler_;
    std::string lastErrorBuf_;
    mutable std::mutex mutex_;
};

} // namespace SZXCamera
