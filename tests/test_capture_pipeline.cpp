// Smoke test: CapturePipeline publish/subscribe and FrameSubscription filtering.

#include "camera/core/camera_pipeline.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>

using namespace SZXCamera;

namespace {

class CountingSink final : public IFrameSink
{
public:
    void consume(const FramePtr &) override { ++count_; }
    std::uint64_t value() const { return count_.load(); }
private:
    std::atomic<std::uint64_t> count_{0};
};

FramePtr makeFrame(const std::string &name, const std::string &sn,
                   const std::string &taskId = {}, bool highExposure = false)
{
    FramePacket p;
    p.meta.cameraName = name;
    p.meta.cameraSerialNumber = sn;
    p.meta.taskId = taskId;
    p.meta.highExposure = highExposure;
    p.width = 10;
    p.height = 10;
    p.channels = 1;
    p.pixelFormat = PixelFormat::Mono8;
    p.data.assign(100, 0);
    return std::make_shared<const FramePacket>(std::move(p));
}

} // namespace

int main()
{
    CapturePipeline pipeline;

    auto sinkAll  = std::make_shared<CountingSink>();
    auto sinkCam0 = std::make_shared<CountingSink>();
    auto sinkHigh = std::make_shared<CountingSink>();

    pipeline.addSink(sinkAll);

    FrameSubscription sub0;
    sub0.cameraNames = {"camera_0"};
    pipeline.addSink(sinkCam0, sub0);

    FrameSubscription subHigh;
    subHigh.onlyHighExposure = true;
    pipeline.addSink(sinkHigh, subHigh);

    pipeline.publish(makeFrame("camera_0", "SN0"));
    pipeline.publish(makeFrame("camera_1", "SN1"));
    pipeline.publish(makeFrame("camera_0", "SN0", "task-1", /*highExposure=*/true));
    pipeline.publish(makeFrame("camera_1", "SN1", "task-1", /*highExposure=*/true));

    assert(sinkAll->value()  == 4);
    assert(sinkCam0->value() == 2);
    assert(sinkHigh->value() == 2);

    const auto stats = pipeline.stats();
    assert(stats.publishedFrames == 4);
    assert(stats.deliveredFrames == sinkAll->value() + sinkCam0->value() + sinkHigh->value());
    assert(stats.sinkCount == 3);

    std::cout << "[OK] test_capture_pipeline (published=" << stats.publishedFrames
              << ", delivered=" << stats.deliveredFrames << ")" << std::endl;
    return 0;
}
