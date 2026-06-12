

#pragma once

#include "camera_types.h"
#include "camera/camera_core_api.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace SZXCamera {
    /**
     * 帧数据处理建议通过“管道”统一流转，而不是让 CameraDevice 直接做存图和拼图。
     *
     * 这样做的好处：
     * 1. 设备层只负责“拿到帧”。
     * 2. 处理层只负责“收到帧以后怎么办”。
     * 3. 后续增加存图、拼图、检测、调试订阅时，不需要改设备层。
     */

    class CAMERA_CORE_API IFrameSink
    {
        public:
            virtual ~IFrameSink() = default;

            /**
             * 消费一帧数据。
             *
             * 功能：
             * - 由上游 CapturePipeline 在收到一帧后调用。
             * - 不同 sink 可以把帧写盘、送拼图模块、送检测模块或做调试展示。
             *
             * @param frame 当前收到的一帧图像数据及元信息。
             */
            virtual void consume(const FramePtr& frame) = 0;
        };

        struct CAMERA_CORE_API CapturePipelineStats {
            std::uint64_t publishedFrames{0};          // 累计进入 pipeline 的帧数
            std::uint64_t deliveredFrames{0};          // 累计成功投递给 sink 的次数（frame x sink）
            std::uint64_t emptyPublishCount{0};        // publish 时没有匹配 sink 的次数
            std::size_t sinkCount{0};                  // 当前 sink 数
            std::uint64_t totalPublishNanos{0};        // publish 总耗时（纳秒）
            std::uint64_t maxPublishNanos{0};          // 单次 publish 最大耗时（纳秒）
            std::uint64_t slowPublishCount{0};         // 超过阈值的慢 publish 次数
            std::chrono::system_clock::time_point lastPublishTime{}; // 最近一次 publish 时间
        };

        /**
         * FrameSubscription 描述一个 sink 想要接收哪些帧。
         *
         * 匹配规则：
         * - 每个已填写字段都参与 AND 匹配。
         * - 若所有条件都为空，则视为“接收全部帧”。
         * - cameraSerialNumbers / cameraNames / taskIds 使用精确匹配。
         */
        struct CAMERA_CORE_API FrameSubscription {
            std::vector<std::string> cameraSerialNumbers; ///< 仅接收这些相机序列号对应的帧。
            std::vector<std::string> cameraNames; ///< 仅接收这些逻辑相机名的帧，例如 camera_0。
            std::vector<std::string> taskIds;     ///< 仅接收这些任务 ID 的帧。
            bool onlyHighExposure{false};         ///< 为 true 时，仅接收高曝光帧。

            bool empty() const
            {
                return cameraSerialNumbers.empty() &&
                       cameraNames.empty() &&
                       taskIds.empty() &&
                       !onlyHighExposure;
            }
        };


        /**
         * CapturePipeline 是一条简单的“发布-订阅”型帧管道。
         *
         * 当前职责很克制：
         * - 维护 sink 列表
         * - 把帧广播给所有 sink
         *
         * 后续如果吞吐上来，可以把它升级为：
         * - 工作线程 + 队列
         * - 背压控制
         * - 异步消费
         */
        class CAMERA_CORE_API CapturePipeline
        {
        public:
            /**
             * 注册一个新的帧 sink。
             *
             * @param sink 帧消费者对象。
             *        若为空，则当前调用会被忽略。
             */
            void addSink(std::shared_ptr<IFrameSink> sink);

            /**
             * 注册一个带订阅条件的帧 sink。
             *
             * @param sink 帧消费者对象。
             * @param subscription 该 sink 关注的帧条件。
             *        若 subscription 为空条件，则视为接收全部帧。
             */
            void addSink(std::shared_ptr<IFrameSink> sink, FrameSubscription subscription);

            /**
             * 清空所有已注册的 sink。
             *
             * 功能：
             * - 后续 publish 时将不再有任何下游消费者。
             */
            void removeAllSinks();

            /**
             * 当前 publish() 是同步分发。
             *
             * 注意：
             * - sink->consume() 会在当前调用线程中直接执行。
             * - 因此 sink 必须尽量快速返回。
             * - 耗时逻辑（写盘、编码、网络发送）应由 sink 自己转入异步线程。
             */

            void publish(FramePtr frame);

            // 返回统计快照，便于 API/日志观测
            CapturePipelineStats stats() const;

            // 测试或手动清零统计时可用
            void resetStats();

        private:
            struct SinkBinding {
                std::shared_ptr<IFrameSink> sink;
                FrameSubscription subscription;
            };

            static bool matchesSubscription(const FramePacket &frame, const FrameSubscription &subscription);
            static bool containsString(const std::vector<std::string> &values, const std::string &target);

            mutable std::mutex mutex_;                              ///< 保护 sink 列表的并发修改与读取。
            std::vector<SinkBinding> sinks_;                        ///< 当前已注册的所有帧消费者及其订阅条件。
            std::atomic<std::uint64_t> publishedFrames_{0};
            std::atomic<std::uint64_t> deliveredFrames_{0};
            std::atomic<std::uint64_t> emptyPublishCount_{0};
            std::atomic<std::uint64_t> totalPublishNanos_{0};
            std::atomic<std::uint64_t> maxPublishNanos_{0};
            std::atomic<std::uint64_t> slowPublishCount_{0};
            std::atomic<std::int64_t> lastPublishEpochMs_{0};
    };

}
