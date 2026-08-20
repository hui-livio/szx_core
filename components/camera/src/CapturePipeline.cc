/**
 * @file CapturePipeline.cc
 * @brief 采集流水线实现 — 管理帧订阅、分发和缓存。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "CapturePipeline.h"

#include "Logger.h"
#include "TimeHelper.hpp"

using namespace SZX::LOGGER;
using namespace SZX::COMMON;

namespace
{
constexpr const char LOG_TAG[] = "Camera";
// 发布会同步调用 sink；CV 模拟后端的正常图像投递约为数毫秒，2 ms 会产生大量误告警。
constexpr std::uint64_t kSlowPublishThresholdNs = 20'000'000; // 20 ms
} // namespace

namespace SZXCamera
{

void CapturePipeline::addSink(std::shared_ptr<IFrameSink> sink)
{
    addSink(std::move(sink), FrameSubscription{});
}

void CapturePipeline::addSink(std::shared_ptr<IFrameSink> sink, FrameSubscription subscription)
{
    if (!sink)
    {
        LOGM_WARN(LOG_TAG, "capture pipeline rejected null sink");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(SinkBinding{std::move(sink), std::move(subscription)});
    LOGM_INFO(LOG_TAG, "capture pipeline sink added, total_sinks={}", sinks_.size());
}

void CapturePipeline::removeAllSinks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
    LOGM_INFO(LOG_TAG, "capture pipeline sinks cleared");
}

void CapturePipeline::publish(FramePtr frame)
{
    if (!frame)
    {
        LOGM_WARN(LOG_TAG, "capture pipeline received null frame");
        return;
    }

    const auto startedAt      = std::chrono::steady_clock::now();
    const auto publishEpochMs = TimeHelper::NowTimestamp();

    std::vector<SinkBinding> sinksCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sinksCopy = sinks_;
    }

    publishedFrames_.fetch_add(1, std::memory_order_relaxed);

    LOGM_TRACE(LOG_TAG, "capture pipeline publishing frame camera={} sink_count={}", frame->meta.cameraName, sinksCopy.size());

    std::size_t matchedSinkCount = 0;
    for (const auto &binding : sinksCopy)
    {
        if (!binding.sink)
            continue;
        if (!matchesSubscription(*frame, binding.subscription))
            continue;

        binding.sink->consume(frame);
        deliveredFrames_.fetch_add(1, std::memory_order_relaxed);
        matchedSinkCount += 1;
    }

    if (matchedSinkCount == 0)
    {
        emptyPublishCount_.fetch_add(1, std::memory_order_relaxed);
    }

    const auto elapsedNs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - startedAt).count());

    totalPublishNanos_.fetch_add(elapsedNs, std::memory_order_relaxed);

    std::uint64_t oldMax = maxPublishNanos_.load(std::memory_order_relaxed);
    while (elapsedNs > oldMax && !maxPublishNanos_.compare_exchange_weak(oldMax, elapsedNs, std::memory_order_relaxed))
    {
    }

    if (elapsedNs >= kSlowPublishThresholdNs)
    {
        const auto slowCount = slowPublishCount_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (slowCount <= 10 || slowCount % 100 == 0)
        {
            LOGM_WARN(LOG_TAG,
                      "capture pipeline slow publish camera={} sink_count={} matched_sinks={} elapsed_us={} "
                      "slow_count={}",
                      frame->meta.cameraName, sinksCopy.size(), matchedSinkCount, elapsedNs / 1000, slowCount);
        }
    }

    lastPublishEpochMs_.store(publishEpochMs, std::memory_order_relaxed);
}

CapturePipelineStats CapturePipeline::stats() const
{
    CapturePipelineStats snapshot;
    snapshot.publishedFrames   = publishedFrames_.load(std::memory_order_relaxed);
    snapshot.deliveredFrames   = deliveredFrames_.load(std::memory_order_relaxed);
    snapshot.emptyPublishCount = emptyPublishCount_.load(std::memory_order_relaxed);
    snapshot.totalPublishNanos = totalPublishNanos_.load(std::memory_order_relaxed);
    snapshot.maxPublishNanos   = maxPublishNanos_.load(std::memory_order_relaxed);
    snapshot.slowPublishCount  = slowPublishCount_.load(std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.sinkCount = sinks_.size();
    }

    snapshot.lastPublishTimeMs = lastPublishEpochMs_.load(std::memory_order_relaxed);

    return snapshot;
}

void CapturePipeline::resetStats()
{
    publishedFrames_.store(0, std::memory_order_relaxed);
    deliveredFrames_.store(0, std::memory_order_relaxed);
    emptyPublishCount_.store(0, std::memory_order_relaxed);
    totalPublishNanos_.store(0, std::memory_order_relaxed);
    maxPublishNanos_.store(0, std::memory_order_relaxed);
    slowPublishCount_.store(0, std::memory_order_relaxed);
    lastPublishEpochMs_.store(0, std::memory_order_relaxed);
}

bool CapturePipeline::matchesSubscription(const FramePacket &frame, const FrameSubscription &subscription)
{
    if (subscription.onlyHighExposure && !frame.meta.highExposure)
        return false;

    if (!subscription.cameraSerialNumbers.empty() &&
        !containsString(subscription.cameraSerialNumbers, frame.meta.cameraSerialNumber))
        return false;

    if (!subscription.cameraNames.empty() && !containsString(subscription.cameraNames, frame.meta.cameraName))
        return false;

    if (!subscription.taskIds.empty() && !containsString(subscription.taskIds, frame.meta.taskId))
        return false;

    return true;
}

bool CapturePipeline::containsString(const std::vector<std::string> &values, const std::string &target)
{
    return std::find(values.begin(), values.end(), target) != values.end();
}

} // namespace SZXCamera
