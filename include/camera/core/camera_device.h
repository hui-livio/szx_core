#pragma once

#include "camera_sdk_adapter.h"
#include "camera_types.h"

#include <memory>
#include <mutex>
#include <string>

namespace SZXCamera{
    /**
     * CameraDevice 是“单台相机”的统一封装。
     *
     * 它负责：
     * - 管理适配器生命周期
     * - 保存当前配置
     * - 保存当前运行状态
     * - 把 SDK 回调转成统一帧事件
     */
    class CameraDevice{
        public:
            /**
             * FrameHandler 是设备层对外暴露的帧回调类型。
             *
             * 含义：
             * - CameraDevice 收到底层 SDK 的一帧后，会通过这个回调把统一格式帧交给上层。
             * - 上层通常会把它接到 CapturePipeline 或其他业务处理模块。
             */
            using FrameHandler = ICameraSdkAdapter::FrameCallback;

            /**
             * 构造单台相机设备对象。
             *
             * 功能：
             * - 接管底层适配器的所有权。
             * - 从适配器中读取基础信息。
             * - 注册底层帧回调，转发到本对象的 onFrame。
             *
             * @param adapter 该相机对应的 SDK 适配器对象。
             *        一般由 CameraManager 通过工厂创建后传入。
             */
            explicit CameraDevice(std::unique_ptr<ICameraSdkAdapter> adapter);
            ~CameraDevice();


            /**
             * 获取相机基础信息。
             *
             * @return const CameraInfo& 当前设备的基础描述信息引用。
             */
            const CameraInfo &info() const;

            /**
             * 获取相机当前运行态快照。
             *
             * @return CameraRuntimeState 当前设备的运行状态副本。
             */
            CameraRuntimeState runtimeState() const;

            /**
             * 获取当前缓存的配置快照。
             *
             * @return CameraConfig 当前设备已应用或准备应用的配置副本。
             */
            CameraConfig currentConfig() const;

            /**
             * 打开相机。
             *
             * 功能：
             * - 调用底层适配器打开设备。
             * - 成功后更新内部状态为 Opened。
             *
             * @param errorMessage 输出参数。
             *        打开失败时写入可读错误信息。
             *
             * @return true 表示打开成功。
             * @return false 表示打开失败。
             */
            bool open(std::string *errorMessage = nullptr);

            /**
             * 关闭相机。
             *
             * 功能：
             * - 调用底层适配器关闭设备。
             * - 成功后更新内部状态为 Stopped。
             *
             * @param errorMessage 输出参数。
             *        关闭失败时写入可读错误信息。
             *
             * @return true 表示关闭成功。
             * @return false 表示关闭失败。
             */
            bool close(std::string *errorMessage = nullptr);

            /**
             * 应用配置。
             *
             * 功能：
             * - 把 CameraConfig 下发给底层适配器。
             * - 成功后缓存该配置，并将状态更新为 Configured。
             *
             * @param config 目标配置。
             * @param errorMessage 输出参数。
             *        配置失败时写入可读错误信息。
             *
             * @return true 表示配置成功。
             * @return false 表示配置失败。
             */
            bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr);

            /**
             * 启动采集。
             *
             * 功能：
             * - 调用底层适配器开始出图。
             * - 成功后更新状态为 Streaming。
             *
             * @param errorMessage 输出参数。
             *        启动失败时写入可读错误信息。
             *
             * @return true 表示启动成功。
             * @return false 表示启动失败。
             */
            bool start(std::string *errorMessage = nullptr);

            /**
             * 停止采集。
             *
             * 功能：
             * - 调用底层适配器停止出图。
             * - 成功后更新状态为 Stopped。
             *
             * @param errorMessage 输出参数。
             *        停止失败时写入可读错误信息。
             *
             * @return true 表示停止成功。
             * @return false 表示停止失败。
             */
            bool stop(std::string *errorMessage = nullptr);

            /**
            * 执行一次软件触发。
            *
            * 功能：
            * - 调用底层适配器发送一次软件触发命令。
            * - 仅适用于软件触发模式下的相机。
            *
            * @param errorMessage 输出参数。
            *        触发失败时写入可读错误信息。
            *
            * @return true 表示触发成功。
            * @return false 表示触发失败。
            */
            bool executeSoftwareTrigger(std::string *errorMessage = nullptr);


            /**
             * 设置帧回调处理器。
             *
             * @param handler 上层帧处理回调。
             *        当设备收到一帧时，会调用该处理器。
             */
            void setFrameHandler(FrameHandler handler);

        private:
            /**
             * 更新当前设备状态。
             *
             * @param newState 新状态。
             * @param lastError 最近错误信息。
             *        若为空，则表示本次状态切换不附带错误文本。
             */
            void updateState(CameraState newState, const std::string &lastError = "");

            /**
             * 处理底层 SDK 回调上来的帧。
             *
             * 功能：
             * - 更新统计信息，例如总帧数和最后一帧时间。
             * - 把帧继续转发给上层 FrameHandler。
             *
             * @param frame 底层适配器产生的一帧统一格式图像。
             */
            void onFrame(const FramePtr frame);

        private:
            mutable std::mutex mutex_;                    ///< 保护配置、运行态和回调处理器的并发访问。
            std::unique_ptr<ICameraSdkAdapter> adapter_; ///< 当前设备绑定的底层 SDK 适配器。
            CameraInfo info_;                            ///< 当前设备的基础信息快照。
            CameraConfig config_;                        ///< 当前设备最近一次成功应用的配置。
            CameraRuntimeState runtimeState_;            ///< 当前设备的运行态信息。
            FrameHandler frameHandler_;                  ///< 上层注册的帧处理器。
    };
}