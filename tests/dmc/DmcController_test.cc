#include <gtest/gtest.h>

#include <memory>

#include <szx/dmc/DmcController.hpp>
#include <szx/dmc/szx_dmc_c.h>

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

TEST(DmcCAbiTest, CreatesPollsAndRejectsInvalidAbi)
{
    szx_dmc_config_v1 config{};
    config.struct_size            = sizeof(config);
    config.abi_version            = SZX_DMC_ABI_VERSION_V1;
    config.simulation             = 1;
    config.compare_queue_capacity = 8;
    config.input_count            = 8;
    config.output_count           = 8;
    szx_dmc_handle *handle        = nullptr;

    ASSERT_EQ(szx_dmc_create_v1(&config, &handle), SZX_DMC_SUCCESS);
    ASSERT_NE(handle, nullptr);
    ASSERT_EQ(szx_dmc_connect_v1(handle), SZX_DMC_SUCCESS);

    szx_dmc_state_v1 state{};
    state.struct_size = sizeof(state);
    state.abi_version = SZX_DMC_ABI_VERSION_V1;
    ASSERT_EQ(szx_dmc_poll_state_v1(handle, &state), SZX_DMC_SUCCESS);
    EXPECT_EQ(state.state, 1U); // Ready

    state.abi_version = 99;
    EXPECT_EQ(szx_dmc_poll_state_v1(handle, &state), SZX_DMC_INVALID_ARGUMENT);
    EXPECT_EQ(szx_dmc_destroy_v1(handle), SZX_DMC_SUCCESS);
    EXPECT_EQ(szx_dmc_destroy_v1(handle), SZX_DMC_INVALID_ARGUMENT);
}

TEST(DmcCAbiTest, CancelsArmedCaptureWithoutLockReentry)
{
    szx_dmc_config_v1 config{};
    config.struct_size            = sizeof(config);
    config.abi_version            = SZX_DMC_ABI_VERSION_V1;
    config.simulation             = 1;
    config.compare_queue_capacity = 8;
    config.input_count            = 8;
    config.output_count           = 8;
    szx_dmc_handle *handle        = nullptr;
    ASSERT_EQ(szx_dmc_create_v1(&config, &handle), SZX_DMC_SUCCESS);
    ASSERT_EQ(szx_dmc_connect_v1(handle), SZX_DMC_SUCCESS);

    szx_dmc_capture_v1 capture{};
    capture.struct_size            = sizeof(capture);
    capture.abi_version            = SZX_DMC_ABI_VERSION_V1;
    capture.operation_id           = 7;
    capture.start_source           = SZX_DMC_START_SOURCE_SOFTWARE_COMMAND_V1;
    capture.trigger_source         = 0;
    capture.expected_frames        = 1;
    capture.compare_queue_capacity = 8;
    ASSERT_EQ(szx_dmc_arm_capture_v1(handle, &capture), SZX_DMC_SUCCESS);
    ASSERT_EQ(szx_dmc_cancel_capture_v1(handle, 0), SZX_DMC_SUCCESS);
    EXPECT_EQ(szx_dmc_destroy_v1(handle), SZX_DMC_SUCCESS);
}
