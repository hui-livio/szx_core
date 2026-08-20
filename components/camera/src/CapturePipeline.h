/**
 * @file CapturePipeline.h
 * @brief 采集流水线 — 将相机帧分发给订阅者和采集任务。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once


#include "CameraSdkAdapter.h"
#include "CameraTypes.h"
#include "FrameSubscription.h"
#include "IFrameSink.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>


namespace SZXCamera
{

// 内部类，不导出——仅 CameraService::Impl 使用
class CapturePipeline
{
public:
    void addSink(std::shared_ptr<IFrameSink> sink);
    void addSink(std::shared_ptr<IFrameSink> sink, FrameSubscription subscription);
    void removeAllSinks();
    void publish(FramePtr frame);
    CapturePipelineStats stats() const;
    void resetStats();

private:
    struct SinkBinding
    {
        std::shared_ptr<IFrameSink> sink;
        FrameSubscription subscription;
    };

    static bool matchesSubscription(const FramePacket &frame, const FrameSubscription &subscription);
    static bool containsString(const std::vector<std::string> &values, const std::string &target);

    mutable std::mutex mutex_;
    std::vector<SinkBinding> sinks_;

    std::atomic<std::uint64_t> publishedFrames_{0};
    std::atomic<std::uint64_t> deliveredFrames_{0};
    std::atomic<std::uint64_t> emptyPublishCount_{0};
    std::atomic<std::uint64_t> totalPublishNanos_{0};
    std::atomic<std::uint64_t> maxPublishNanos_{0};
    std::atomic<std::uint64_t> slowPublishCount_{0};
    std::atomic<std::uint64_t> lastPublishEpochMs_{0};
};

} // namespace SZXCamera
