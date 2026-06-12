#pragma once

#include "camera/camera_core_api.h"
#include "camera/core/camera_sdk_adapter.h"
#include "camera/ikap/ikap_runtime.h"
#include "IKapC.h"
#include "common/log/log_macros.h"

namespace SZXCamera{
    /* @brief：判断 IKapC 函数是否成功调用。
    * @param[in] errc：函数返回值。
    *
    * @brief：Determine whether the IKapC function is called successfully.
    * @param[in] errc：Function return value. */
    inline bool isIkapSuccess(ITKSTATUS errc) {
        return errc == ITKSTATUS_OK;
    }

    class CAMERA_CORE_API CameraIkapSdkAdapter final : public ICameraSdkAdapter
    {
        struct IkapDeviceContext
            {
                uint32_t deviceIndex{static_cast<uint32_t>(-1)};
                ITKDEV_INFO deviceInfo{};

                ITKDEVICE cameraHandle{nullptr};
                ITKSTREAM streamHandle{nullptr};
                ITKBUFFER convertBuffer{nullptr};
                uint32_t grabCount{ITKSTREAM_CONTINUOUS};
        };

        public:
        /**
         * 构造一个空实现相机适配器。
         *
         * @param info 该相机的基础信息。
         *        一般由工厂在枚举设备后传入。
         */
        explicit CameraIkapSdkAdapter(CameraInfo info);
        ~CameraIkapSdkAdapter() override;

        std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
        
        /**
         * 打开空实现相机。
         *
         * 功能：
         * - 仅修改内部 opened 标志，不访问真实硬件。
         */
        bool open(std::string *errorMessage = nullptr) override;

        /**
         * 关闭空实现相机。
         *
         * 功能：
         * - 仅重置内部状态，不访问真实硬件。
         */
        bool close(std::string *errorMessage = nullptr) override;

        /**
         * 应用配置到空实现相机。
         *
         * 功能：
         * - 仅缓存 CameraConfig，方便后续测试和流程验证。
         *
         * @param config 要应用的配置对象。
         */
        bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr) override;

        /**
         * 启动空实现采集。
         *
         * 功能：
         * - 若设备尚未 open，则返回失败。
         * - 否则仅更新内部 grabbing 状态。
         */
        bool startGrabbing(std::string *errorMessage = nullptr) override;

        /**
         * 停止空实现采集。
         *
         * 功能：
         * - 仅重置内部 grabbing 状态。
         */
        bool stopGrabbing(std::string *errorMessage = nullptr) override;

        bool executeSoftwareTrigger(std::string *errorMessage = nullptr) override;

        /**
         * 设置帧回调。
         *
         * @param callback 上层传入的帧处理回调。
         */
        void setFrameCallback(FrameCallback callback) override;

        /**
         * 获取当前空实现适配器对应的设备信息。
         *
         * @return CameraInfo 当前设备的基础信息副本。
         */
        CameraInfo info() const override;

    private:
        struct Impl;

    private:
        CameraInfo info_;
        std::unique_ptr<Impl> impl_;   ///< Pimpl 结构体，后续实现中会用到。
    };

    class CAMERA_CORE_API IkapCameraSdkFactory : public ICameraSdkFactory
    {   
        public:
            IkapCameraSdkFactory() = default;
            ~IkapCameraSdkFactory() override = default;

        /**
         * 枚举设备。
         *
         * 功能：
         * - 调用 IKap SDK 的枚举能力。
         * - 将枚举结果转换成统一的 CameraInfo 列表。
         *
         * @param errorMessage 输出参数。
         *        当枚举失败时，写入可读错误信息；成功时通常为空。
         *
         * @return std::vector<CameraInfo> 枚举到的设备列表。
         *         返回空列表不一定代表失败，也可能表示当前确实没有设备在线。
         */
        std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
        /**
         * 创建适配器实例。
         *
         * 功能：
         * - 根据 CameraInfo 创建对应的一台相机适配器。
         * - 适配器实例后续将交给 CameraDevice 管理。
         *
         * @param info 目标设备的基础信息。
         * @param errorMessage 输出参数。
         *        当创建失败时，写入可读错误信息；成功时通常为空。
         *
         * @return std::unique_ptr<ICameraSdkAdapter> 创建成功时返回适配器对象。
         *         创建失败时返回 nullptr。
         */
        std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info,
                                                                std::string *errorMessage = nullptr) override;

        std::vector<std::unique_ptr<ICameraSdkAdapter>> createAllAdapters(); ///< 获取所有的 Camera。
    };
};
