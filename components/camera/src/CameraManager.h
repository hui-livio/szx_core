/**
 * @file CameraManager.h
 * @brief 相机管理器 — 统一管理相机设备发现、打开和生命周期。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraDevice.h"
#include "CameraSdkAdapter.h"
#include "CameraTypes.h"
#include "ICameraDevice.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


namespace SZXCamera
{

// ========================================================================
// CameraManager — 多相机统一管理（内部类，不导出）
// ========================================================================

class CameraManager
{
public:
    explicit CameraManager(std::shared_ptr<ICameraSdkFactory> factory);
    ~CameraManager() = default;

    CameraManager(const CameraManager &)            = delete;
    CameraManager &operator=(const CameraManager &) = delete;

    bool discover(std::string *errorMessage = nullptr);
    std::vector<CameraInfo> listDiscovered() const;
    std::shared_ptr<ICameraDevice> getCameraBySerialNumber(const std::string &serialNumber) const;
    std::vector<std::shared_ptr<ICameraDevice>> listCameras() const;
    bool openAll(std::string *errorMessage = nullptr);
    bool closeAll(std::string *errorMessage = nullptr);

private:
    std::shared_ptr<ICameraSdkFactory> factory_;
    mutable std::mutex mutex_;
    std::vector<CameraInfo> discovered_;
    std::unordered_map<std::string, std::shared_ptr<CameraDevice>> cameras_;
};

} // namespace SZXCamera
