/**
 * @file camera_ikapsdk_adapter.h
 * @brief IKap 相机适配器 — 将 IKap SDK 能力适配为 CameraSdkAdapter 接口。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraSdkAdapter.h"
#include "ikap_runtime.h"

#ifdef _MSC_VER
    #pragma pack(push)
    #pragma warning(push)
    #pragma warning(disable : 4103) // IKapC.h 内部 #pragma pack 未配对关闭
#endif
#include "IKapC.h"
#ifdef _MSC_VER
    #pragma warning(pop)
    #pragma pack(pop)
#endif

#include <atomic>
#include <memory>
#include <string>


namespace SZXCamera
{

inline bool isIkapSuccess(ITKSTATUS errc)
{
    return errc == ITKSTATUS_OK;
}

// ========================================================================
// CameraIkapSdkAdapter — IKap SDK 相机适配器（内部类，不导出）
// ========================================================================

class CameraIkapSdkAdapter final : public ICameraSdkAdapter
{
public:
    explicit CameraIkapSdkAdapter(CameraInfo info);
    ~CameraIkapSdkAdapter() override;

    std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
    bool open(std::string *errorMessage = nullptr) override;
    bool close(std::string *errorMessage = nullptr) override;
    bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr) override;
    bool startGrabbing(std::string *errorMessage = nullptr) override;
    bool stopGrabbing(std::string *errorMessage = nullptr) override;
    bool executeSoftwareTrigger(std::string *errorMessage = nullptr) override;
    void setFrameCallback(FrameCallback callback) override;
    CameraInfo info() const override;

private:
    struct Impl;

    CameraInfo info_;
    std::unique_ptr<Impl> impl_;
};

// ========================================================================
// IkapCameraSdkFactory — IKap SDK 工厂
// ========================================================================

class IkapCameraSdkFactory final : public ICameraSdkFactory
{
public:
    IkapCameraSdkFactory()           = default;
    ~IkapCameraSdkFactory() override = default;

    std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
    std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info, std::string *errorMessage = nullptr) override;

private:
    // 缓存枚举结果字符串，CameraInfo 的 const char* 指针指向此处
    std::vector<std::string> deviceIds_;
    std::vector<std::string> serialNumbers_;
    std::vector<std::string> logicalNames_;
    std::vector<std::string> vendors_;
    std::vector<std::string> models_;
};

} // namespace SZXCamera
