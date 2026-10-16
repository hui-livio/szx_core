#include <gtest/gtest.h>

#include <limits>
#include <memory>

#include <szx/dmc/DmcController.hpp>

namespace
{
using SZX::DMC::CaptureRequest;
using SZX::DMC::DmcConfig;
using SZX::DMC::DmcController;
using SZX::DMC::DmcControllerFactory;
using SZX::DMC::DmcError;
using SZX::DMC::DmcState;
using SZX::DMC::FrameTriggerSource;
using SZX::DMC::StartSource;

auto makeController() -> std::unique_ptr<DmcController>
{
    auto result = DmcControllerFactory::create(DmcConfig{.compareQueueCapacity = 8});
    EXPECT_TRUE(result.ok);
    return std::move(result.value);
}

auto makeCapture(StartSource source = StartSource::SoftwareCommand) -> CaptureRequest
{
    CaptureRequest request;
    request.operationId                = 42;
    request.startSource                = source;
    request.expectedFrames             = 4;
    request.triggerSource              = FrameTriggerSource::PositionCompareOutput;
    request.compareQueueCapacity       = 8;
    request.scanPlan.scanPositions     = {10.0, 20.0};
    request.scanPlan.rowPositions      = {1.0, 2.0};
    request.comparePoints              = {{10.0, 0, 0, 0}, {20.0, 1, 0, 1}, {10.0, 2, 1, 0}, {20.0, 3, 1, 1}};
    request.scanPlan.scanStartPosition = 10.0;
    request.scanPlan.scanEndPosition   = 20.0;
    return request;
}
} // namespace

TEST(DmcControllerTest, SimulationCaptureCompletesOnlyAfterCompareAndFrameProgress)
{
    auto controller = makeController();
    ASSERT_TRUE(controller->connect().ok);

    ASSERT_TRUE(controller->enableAxis(0).ok);
    ASSERT_TRUE(controller->homeAxis(0).ok);
    ASSERT_TRUE(controller->enableAxis(1).ok);
    ASSERT_TRUE(controller->homeAxis(1).ok);
    ASSERT_TRUE(controller->moveAbsolute(0, 100.0).ok);

    const auto request = makeCapture();
    ASSERT_TRUE(controller->armCapture(request).ok);
    ASSERT_EQ(controller->status().value.state, DmcState::Armed);
    ASSERT_TRUE(controller->startCapture(request.operationId).ok);

    EXPECT_EQ(controller->acknowledgeFrameProgress(request.operationId, 1).error,
              static_cast<int>(DmcError::FrameProgressInvalid));
    ASSERT_TRUE(controller->acknowledgeCompareTrigger(request.operationId, 4).ok);
    ASSERT_TRUE(controller->acknowledgeFrameProgress(request.operationId, 4).ok);
    ASSERT_TRUE(controller->completeCamera(request.operationId, 4).ok);

    const auto state = controller->status();
    ASSERT_TRUE(state.ok);
    EXPECT_EQ(state.value.state, DmcState::Completed);
    EXPECT_EQ(state.value.receivedFrames, 4U);
    EXPECT_FALSE(state.value.compareEnabled);
}

TEST(DmcControllerTest, InjectedLogSinkReceivesLifecycleMessages)
{
    auto config    = DmcConfig{.compareQueueCapacity = 8};
    auto logs      = 0;
    config.logSink = [&logs](SZX::DMC::DmcLogLevel, const std::string &)
    {
        ++logs;
    };
    auto result = DmcControllerFactory::create(std::move(config));
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.value->connect().ok);
    EXPECT_GT(logs, 0);
}

TEST(DmcControllerTest, SoftwareTriggerCaptureCompletesWithoutCompareAcknowledgement)
{
    auto controller = makeController();
    ASSERT_TRUE(controller->connect().ok);
    ASSERT_TRUE(controller->enableAxis(0).ok);
    ASSERT_TRUE(controller->homeAxis(0).ok);
    ASSERT_TRUE(controller->enableAxis(1).ok);
    ASSERT_TRUE(controller->homeAxis(1).ok);

    auto request          = makeCapture();
    request.triggerSource = FrameTriggerSource::Software;
    request.comparePoints.clear();
    ASSERT_TRUE(controller->armCapture(request).ok);
    ASSERT_TRUE(controller->startCapture(request.operationId).ok);
    ASSERT_TRUE(controller->acknowledgeFrameProgress(request.operationId, request.expectedFrames).ok);
    ASSERT_TRUE(controller->completeCamera(request.operationId, request.expectedFrames).ok);
    EXPECT_EQ(controller->status().value.state, DmcState::Completed);
}

TEST(DmcControllerTest, RejectsInvalidMotionProfile)
{
    auto controller = makeController();
    ASSERT_TRUE(controller->connect().ok);
    ASSERT_TRUE(controller->enableAxis(0).ok);
    ASSERT_TRUE(controller->homeAxis(0).ok);
    SZX::DMC::MotionProfile profile;
    profile.maxVelocity = -1.0;
    EXPECT_EQ(controller->moveAbsolute(0, 1.0, profile).error, static_cast<int>(DmcError::InvalidArgument));

    profile.maxVelocity = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(controller->moveAbsolute(0, 1.0, profile).error, static_cast<int>(DmcError::InvalidArgument));
}

TEST(DmcControllerTest, FactoryRoutesLeiSaiProfileByProfileId)
{
    auto config      = DmcConfig{.compareQueueCapacity = 8};
    config.profileId = "E3032";
    auto result      = DmcControllerFactory::create(config);
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.value->profile().ok);
    EXPECT_EQ(result.value->profile().value.id, "E3032");

    config.profileId = "UNKNOWN";
    auto unsupported = DmcControllerFactory::create(config);
    EXPECT_FALSE(unsupported.ok);
    EXPECT_EQ(unsupported.error, static_cast<int>(DmcError::InvalidArgument));
}

TEST(DmcControllerTest, DigitalInputDoesNotStartCapture)
{
    auto controller = makeController();
    ASSERT_TRUE(controller->connect().ok);
    ASSERT_TRUE(controller->enableAxis(0).ok);
    ASSERT_TRUE(controller->homeAxis(0).ok);
    ASSERT_TRUE(controller->enableAxis(1).ok);
    ASSERT_TRUE(controller->homeAxis(1).ok);
    auto request = makeCapture(StartSource::HardwareTrigger);
    ASSERT_TRUE(controller->armCapture(request).ok);

    EXPECT_EQ(controller->startCapture(request.operationId, StartSource::None).error,
              static_cast<int>(DmcError::InvalidArgument));
    ASSERT_TRUE(controller->signalDigitalInput(1, false).ok);
    ASSERT_TRUE(controller->signalDigitalInput(1, true).ok);
    const auto armed = controller->status();
    ASSERT_TRUE(armed.ok);
    EXPECT_EQ(armed.value.state, DmcState::Armed);

    ASSERT_TRUE(controller->signalDigitalInput(1, true).ok);
    EXPECT_EQ(controller->status().value.state, DmcState::Armed);
    ASSERT_TRUE(controller->startCapture(request.operationId, StartSource::HardwareTrigger).ok);
    EXPECT_EQ(controller->status().value.state, DmcState::Scanning);
}

TEST(DmcControllerTest, RejectsInvalidCaptureStartSourceMask)
{
    auto controller = makeController();
    ASSERT_TRUE(controller->connect().ok);
    ASSERT_TRUE(controller->enableAxis(0).ok);
    ASSERT_TRUE(controller->homeAxis(0).ok);
    ASSERT_TRUE(controller->enableAxis(1).ok);
    ASSERT_TRUE(controller->homeAxis(1).ok);
    auto request        = makeCapture();
    request.startSource = static_cast<StartSource>(4);
    EXPECT_EQ(controller->armCapture(request).error, static_cast<int>(DmcError::InvalidArgument));
}

TEST(DmcControllerTest, InputSnapshotCallbackReceivesRawInputState)
{
    auto snapshots        = std::vector<SZX::DMC::InputSnapshot>{};
    auto config           = DmcConfig{.compareQueueCapacity = 8};
    config.inputEventSink = [&snapshots](const SZX::DMC::InputSnapshot &snapshot)
    {
        snapshots.push_back(snapshot);
    };
    auto result = DmcControllerFactory::create(config);
    ASSERT_TRUE(result.ok);
    auto controller = std::move(result.value);
    ASSERT_TRUE(controller->connect().ok);

    ASSERT_TRUE(controller->signalDigitalInput(1, false).ok);
    ASSERT_TRUE(controller->signalDigitalInput(1, true).ok);
    ASSERT_TRUE(controller->signalDigitalInput(1, true).ok);
    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_EQ(snapshots[0].sampledBits, 0U);
    EXPECT_EQ(snapshots[1].sampledBits, 2U);
    EXPECT_TRUE(snapshots[1].valid);
}

TEST(DmcControllerTest, CancellationClearsCompareQueueAndPreservesSafetyState)
{
    auto controller = makeController();
    ASSERT_TRUE(controller->connect().ok);
    ASSERT_TRUE(controller->enableAxis(0).ok);
    ASSERT_TRUE(controller->homeAxis(0).ok);
    ASSERT_TRUE(controller->enableAxis(1).ok);
    ASSERT_TRUE(controller->homeAxis(1).ok);
    const auto request = makeCapture();
    ASSERT_TRUE(controller->armCapture(request).ok);
    ASSERT_TRUE(controller->startCapture(request.operationId).ok);
    ASSERT_TRUE(controller->cancelCapture(request.operationId).ok);

    const auto state = controller->status();
    ASSERT_TRUE(state.ok);
    EXPECT_EQ(state.value.state, DmcState::Cancelled);
    EXPECT_EQ(state.value.lastError, DmcError::Cancelled);
    EXPECT_EQ(state.value.compareQueueRemaining, 0U);
    EXPECT_FALSE(state.value.compareEnabled);
}
