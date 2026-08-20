Set-PSReadLineOption: C:\Users\Administrator\Documents\PowerShell\Microsoft.PowerShell_profile.ps1:4
Line |
   4 |  Set-PSReadLineOption -PredictionViewStyle ListView
     |  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     | 句柄无效。
#pragma once

#include "CameraExport.h"
#include "CameraSdkAdapter.h"

#include "IMVFGApi.h"
#include "IMVFGDefines.h"

namespace SZXCamera {

    inline bool isHuaraySuccess(int errc) {
        return errc == IMV_FG_OK;
    }

    /**
     * 华睿 (HuarayTech / IMVFG) 帧采集卡相机适配器。
     *
     * 设计要点：
     * 1. IMVFG SDK 工作模型是 "采集卡(interface) + 相机(device)" 两个独立句柄。
     *    本适配器为每台相机同时管理这两个句柄。
     * 2. IMVFG SDK 是惰性初始化的，没有全局 Init/Terminate，因此不需要 Runtime 单例。
     * 3. 帧通过 IMV_FG_AttachGrabbing 回调送达；回调内的数据生命周期仅限本次回调，
     *    必须 memcpy 到上层 FramePacket 后再交给业务层。
     * 4. CameraLink 接口的触发配置和普通接口不同（部分属性写在采集卡上），
     *    本适配器会按接口类型选择对应路径。
     */
    class AOI_CAMERA_API CameraHuaraySdkAdapter final : public ICameraSdkAdapter
    {
    public:
        explicit CameraHuaraySdkAdapter(CameraInfo info);
        ~CameraHuaraySdkAdapter() override;

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
     * 华睿相机工厂。
     *
     * 通过 IMV_FG_EnumInterface + IMV_FG_EnumDevices 完成全局枚举，
     * 然后为每台相机创建 CameraHuaraySdkAdapter。
     */
    class AOI_CAMERA_API HuarayCameraSdkFactory : public ICameraSdkFactory
    {
    public:
        HuarayCameraSdkFactory() = default;
        ~HuarayCameraSdkFactory() override = default;

        std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
        std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info,
                                                        std::string *errorMessage = nullptr) override;

        std::vector<std::unique_ptr<ICameraSdkAdapter>> createAllAdapters();
    };

}

