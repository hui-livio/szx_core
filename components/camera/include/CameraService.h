/**
 * @file CameraService.h
 * @brief 相机服务接口 — 暴露设备枚举、打开和关闭能力。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraExport.h"
#include "CameraTypes.h"
#include "ICameraDevice.h"
#include "IFrameSink.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace SZXCamera
{

class CameraManager; // 前向声明（内部类，仅 Impl 使用）

// ========================================================================
// CameraService — 相机子系统对外的统一服务入口（PIMPL）
// ========================================================================

class AOI_CAMERA_API CameraService
{
public:
    explicit CameraService(CameraBackend backend);
    explicit CameraService(const CameraServiceConfig &config);
    ~CameraService();

    CameraService(const CameraService &)            = delete;
    CameraService &operator=(const CameraService &) = delete;

    // 生命周期
    bool discover(std::string *errorMessage = nullptr);
    bool initialize(std::string *errorMessage = nullptr);
    bool shutdown(std::string *errorMessage = nullptr);

    // 配置
    bool applyCameraConfigs(const std::unordered_map<std::string, CameraConfig> &configs, std::string *errorMessage = nullptr);

    // 查询
    std::vector<CameraInfo> cameras() const;
    std::shared_ptr<ICameraDevice> getCameraBySerialNumber(const std::string &serialNumber) const;
    std::unordered_map<std::string, CameraRuntimeState> cameraRuntimeStates() const;

    // 批量操作
    bool startCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers, std::string *errorMessage = nullptr);
    bool stopCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers, std::string *errorMessage = nullptr);
    bool executeSoftwareTriggerBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                               std::string *errorMessage = nullptr);

    // 帧管道
    void addFrameSink(std::shared_ptr<IFrameSink> sink);
    /**
     * @brief 添加帧管道(可选订阅指定相机或任务ID的帧)
     * @param sink 帧管道
     * @param cameraNames 相机名称数组，可选
     * @param nameCount 相机名称数组元素数量
     * @param cameraSerialNumbers 相机序列号数组，可选
     * @param serialCount 相机序列号数组元素数量
     * @param taskIds 任务ID数组，可选
     * @param taskCount 任务ID数组元素数量
     * @param highExposureOnly 是否仅订阅高曝光相机的帧
     */
    void addFrameSinkWithFilter(std::shared_ptr<IFrameSink> sink, const char *const *cameraNames = nullptr, int nameCount = 0,
                                const char *const *cameraSerialNumbers = nullptr, int serialCount = 0,
                                const char *const *taskIds = nullptr, int taskCount = 0, bool highExposureOnly = false);
    void clearFrameSinks();
    CapturePipelineStats pipelineStats() const;
    void resetPipelineStats();

private:
    struct Impl;

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
    std::unique_ptr<Impl> impl_;
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
};

} // namespace SZXCamera
