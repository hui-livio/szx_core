#pragma once

#include "CameraExport.h"
#include "CameraSdkAdapter.h"

#include "IMVApi.h"
#include "IMVDefines.h"

namespace SZXCamera
{

inline bool isHuarayImvSuccess(int errc)
{
    return errc == IMV_OK;
}

/**
     * 华睿 (HuarayTech) USB / GigE 直连相机适配器（基于 IMV SDK）。
     *
     * 适用范围：
     * - USB3.0 工业相机直连
     * - GigE 工业相机直连
     * - 不通过帧采集卡的所有相机
     *
     * 如果你的相机通过采集卡（CL / CXP / GigE-PCIe）接入，应使用 IMVFG
     * 版本的 CameraHuaraySdkAdapter，而不是这个类。
     *
     * 设计要点：
     * 1. IMV 工作模型只有一个相机句柄 IMV_HANDLE。
     *    CreateHandle 创建句柄 → Open 打开物理设备 → Close 关闭 → DestroyHandle 销毁。
     * 2. IMV SDK 同样是惰性初始化，没有全局 Init/Terminate，因此不需要 Runtime 单例。
     * 3. 帧通过 IMV_AttachGrabbing 回调送达；回调里 pData 的生命周期仅限本次回调，
     *    必须 memcpy 出来再交给上层。
     */
class AOI_CAMERA_API CameraHuarayUsbSdkAdapter final : public ICameraSdkAdapter
{
public:
    explicit CameraHuarayUsbSdkAdapter(CameraInfo info);
    ~CameraHuarayUsbSdkAdapter() override;

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

private:
    CameraInfo info_;
    std::unique_ptr<Impl> impl_;
};

/**
     * 华睿 USB/GigE 直连相机工厂（基于 IMV SDK）。
     *
     * 通过 IMV_EnumDevices(interfaceTypeAll) 完成全局枚举，
     * 然后为每台相机创建 CameraHuarayUsbSdkAdapter。
     */
class AOI_CAMERA_API HuarayUsbCameraSdkFactory : public ICameraSdkFactory
{
public:
    HuarayUsbCameraSdkFactory()           = default;
    ~HuarayUsbCameraSdkFactory() override = default;

    std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
    std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info, std::string *errorMessage = nullptr) override;

    std::vector<std::unique_ptr<ICameraSdkAdapter>> createAllAdapters();
};

} // namespace SZXCamera
