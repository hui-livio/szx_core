#pragma once

#include "camera_manager.h"
#include "camera_pipeline.h"
#include "camera/camera_core_api.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace SZXCamera{
    /**
     * CameraService 是相机子系统对外的“服务入口”。
     *
     * VisionController 后续应该优先依赖 CameraService，而不是直接碰 CameraDevice 或 SDK 适配器。
     *
     * 这样做的好处：
     * - HTTP/API 层只面向一个统一服务
     * - 后续替换实现时，影响面更小
     * - 便于在这里集中做批量配置、状态汇总、错误处理
     */
    class CAMERA_CORE_API CameraService
    {
        public:
            /**
             * 构造相机服务对象。
             *
             * @param factory 相机 SDK 工厂。
             *        服务层会据此创建内部的 CameraManager。
             */
            explicit CameraService(std::shared_ptr<ICameraSdkFactory> factory);

            /**
             * 执行设备扫描。
             *
             * 功能：
             * - 扫描当前所有可见相机。
             * - 为每台相机创建 CameraDevice。
             * - 自动把设备输出帧绑定到内部 CapturePipeline。
             *
             * @param errorMessage 输出参数。
             *        扫描失败时写入可读错误信息。
             *
             * @return true 表示扫描成功。
             * @return false 表示扫描失败。
             */
            bool discover(std::string *errorMessage = nullptr);

            /**
             * 初始化相机系统。
             *
             * 功能：
             * - 当前语义主要是“打开全部相机”。
             * - 后续也可以扩展为统一初始化采集缓冲、默认参数等。
             *
             * @param errorMessage 输出参数。
             *        初始化失败时写入可读错误信息。
             *
             * @return true 表示初始化成功。
             * @return false 表示初始化失败。
             */
            bool initialize(std::string *errorMessage = nullptr);

            /**
             * 关闭相机系统。
             *
             * 功能：
             * - 当前语义主要是“关闭全部相机”。
             * - 后续也可扩展为统一停止线程、释放缓存等资源。
             *
             * @param errorMessage 输出参数。
             *        关闭失败时写入可读错误信息。
             *
             * @return true 表示关闭成功。
             * @return false 表示关闭失败。
             */
            bool shutdown(std::string *errorMessage = nullptr);

            /**
             * 批量应用多台相机配置。
             *
             * @param configs 相机配置表。
             *        key 为相机序列号，value 为该相机要下发的配置。
             * @param errorMessage 输出参数。
             *        某台相机配置失败时写入可读错误信息。
             *
             * @return true 表示所有配置均应用成功。
             * @return false 表示存在配置失败。
             */
            bool applyCameraConfigs(const std::unordered_map<std::string, CameraConfig> &configs,
                                    std::string *errorMessage = nullptr);

            /**
             * 获取当前已发现的相机基础信息列表。
             *
             * @return std::vector<CameraInfo> 已发现设备的基础信息副本。
             */
            std::vector<CameraInfo> cameras() const;

            /**
             * 按序列号获取具体相机对象。
             *
             * @param serialNumber 相机序列号。
             *
             * @return std::shared_ptr<CameraDevice> 找到时返回设备对象，否则返回 nullptr。
             */
            std::shared_ptr<CameraDevice> getCameraBySerialNumber(const std::string &serialNumber) const;

            /**
             * 按序列号批量启动指定相机。
             *
             * @param serialNumbers 目标相机序列号列表。
             * @param errorMessage 输出参数。
             *
             * @return true 表示全部启动成功。
             * @return false 表示有任一相机启动失败。
             */
            bool startCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                             std::string *errorMessage = nullptr);

            /**
             * 按序列号批量停止指定相机。
             *
             * @param serialNumbers 目标相机序列号列表。
             * @param errorMessage 输出参数。
             *
             * @return true 表示全部停止成功。
             * @return false 表示有任一相机停止失败。
             */
            bool stopCamerasBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                            std::string *errorMessage = nullptr);

        /**
        * 按序列号对指定相机执行一次软件触发。
        *
        * @param serialNumbers 目标相机序列号列表。
        * @param errorMessage 输出参数。
        *
        * @return true 表示全部触发成功。
        * @return false 表示有任一相机触发失败。
        */
        bool executeSoftwareTriggerBySerialNumbers(const std::vector<std::string> &serialNumbers,
                                                   std::string *errorMessage = nullptr);


            /**
             * 向采集流水线注册一个帧 sink。
             *
             * 功能：
             * - 后续所有通过 CameraService 管理的设备输出帧，都会交给这个 sink 消费。
             *
             * @param sink 帧消费者对象。
             */
            void addFrameSink(std::shared_ptr<IFrameSink> sink);

            /**
             * 向采集流水线注册一个带订阅条件的帧 sink。
             *
             * @param sink 帧消费者对象。
             * @param subscription sink 关注的帧范围。
             */
            void addFrameSink(std::shared_ptr<IFrameSink> sink, FrameSubscription subscription);

            /**
             * 清空所有已注册的帧 sink。
             *
             * 功能：
             * - 后续帧仍会产生，但不会再有下游消费逻辑。
             */
            void clearFrameSinks();

            // 新增：批量查看所有相机运行态
            std::unordered_map<std::string, CameraRuntimeState> cameraRuntimeStates() const;

            // 新增：查看 pipeline 统计
            CapturePipelineStats pipelineStats() const;

            // 可选：清空 pipeline 统计，便于阶段性测试
            void resetPipelineStats();

        private:
            /**
             * 给所有设备绑定统一帧回调。
             *
             * 功能：
             * - 把每台设备的输出帧统一转发到 CapturePipeline。
             * - 是设备层和处理层之间的桥接动作。
             */
            void bindFrameHandlers();

        private:
            std::shared_ptr<CameraManager> manager_;    ///< 多相机管理器，负责扫描、查询和设备生命周期管理。
            std::shared_ptr<CapturePipeline> pipeline_; ///< 帧处理流水线，负责把所有设备输出帧分发给下游 sink。
    };
}
