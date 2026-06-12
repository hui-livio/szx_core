#include "camera/core/camera_pipeline.h"
#include "common/log/log_macros.h"

namespace SZXCamera {

    namespace {
        constexpr std::uint64_t kSlowPublishThresholdNs = 2'000'000; // 2 ms
    }

    void CapturePipeline::addSink(std::shared_ptr<IFrameSink> sink)
    {
        addSink(std::move(sink), FrameSubscription{});
    }

    void CapturePipeline::addSink(std::shared_ptr<IFrameSink> sink, FrameSubscription subscription)
    {
        if (!sink) {
            LOG_WARN("capture pipeline rejected null sink");
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.push_back(SinkBinding{std::move(sink), std::move(subscription)});
        LOG_INFO("capture pipeline sink added, total_sinks={}", sinks_.size());
    }

    void CapturePipeline::removeAllSinks()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.clear();
        LOG_INFO("capture pipeline sinks cleared");
    }

     void CapturePipeline::publish(FramePtr frame)
    {
        if (!frame) {
            LOG_WARN("capture pipeline received null frame");
            return;
        }

        const auto startedAt = std::chrono::steady_clock::now();
        const auto wallClockNow = std::chrono::system_clock::now();

        std::vector<SinkBinding> sinksCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sinksCopy = sinks_;
        }

        publishedFrames_.fetch_add(1, std::memory_order_relaxed);

        LOG_DEBUG("capture pipeline publishing frame camera={} sink_count={}",
                  frame->meta.cameraName,
                  sinksCopy.size());

        std::size_t matchedSinkCount = 0;
        for (const auto &binding : sinksCopy) {
            if (!binding.sink) {
                continue;
            }

            if (!matchesSubscription(*frame, binding.subscription)) {
                continue;
            }

            binding.sink->consume(frame);
            deliveredFrames_.fetch_add(1, std::memory_order_relaxed);
            matchedSinkCount += 1;
        }

        if (matchedSinkCount == 0) {
            emptyPublishCount_.fetch_add(1, std::memory_order_relaxed);
        }

        const auto elapsedNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - startedAt).count());

        totalPublishNanos_.fetch_add(elapsedNs, std::memory_order_relaxed);

        std::uint64_t oldMax = maxPublishNanos_.load(std::memory_order_relaxed);
        while (elapsedNs > oldMax &&
               !maxPublishNanos_.compare_exchange_weak(
                   oldMax, elapsedNs, std::memory_order_relaxed)) {
        }

        if (elapsedNs >= kSlowPublishThresholdNs) {
            const auto slowCount = slowPublishCount_.fetch_add(1, std::memory_order_relaxed) + 1;

            if (slowCount <= 10 || slowCount % 100 == 0) {
                LOG_WARN("capture pipeline slow publish camera={} sink_count={} matched_sinks={} elapsed_us={} slow_count={}",
                         frame->meta.cameraName,
                         sinksCopy.size(),
                         matchedSinkCount,
                         elapsedNs / 1000,
                         slowCount);
            }
        }

        const auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            wallClockNow.time_since_epoch()).count();
        lastPublishEpochMs_.store(epochMs, std::memory_order_relaxed);
    }

    CapturePipelineStats CapturePipeline::stats() const
    {
        CapturePipelineStats snapshot;
        snapshot.publishedFrames = publishedFrames_.load(std::memory_order_relaxed);
        snapshot.deliveredFrames = deliveredFrames_.load(std::memory_order_relaxed);
        snapshot.emptyPublishCount = emptyPublishCount_.load(std::memory_order_relaxed);
        snapshot.totalPublishNanos = totalPublishNanos_.load(std::memory_order_relaxed);
        snapshot.maxPublishNanos = maxPublishNanos_.load(std::memory_order_relaxed);
        snapshot.slowPublishCount = slowPublishCount_.load(std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot.sinkCount = sinks_.size();
        }

        const auto epochMs = lastPublishEpochMs_.load(std::memory_order_relaxed);
        if (epochMs > 0) {
            snapshot.lastPublishTime =
                std::chrono::system_clock::time_point{std::chrono::milliseconds(epochMs)};
        }

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
        if (subscription.onlyHighExposure && !frame.meta.highExposure) {
            return false;
        }

        if (!subscription.cameraSerialNumbers.empty() &&
            !containsString(subscription.cameraSerialNumbers, frame.meta.cameraSerialNumber)) {
            return false;
        }

        if (!subscription.cameraNames.empty() &&
            !containsString(subscription.cameraNames, frame.meta.cameraName)) {
            return false;
        }

        if (!subscription.taskIds.empty() &&
            !containsString(subscription.taskIds, frame.meta.taskId)) {
            return false;
        }

        return true;
    }

    bool CapturePipeline::containsString(const std::vector<std::string> &values, const std::string &target)
    {
        return std::find(values.begin(), values.end(), target) != values.end();
    }
}   
