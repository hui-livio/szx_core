#include <gtest/gtest.h>

#include <memory>

#include <szx/dmc/DmcController.hpp>
#include <szx/dmc/szx_dmc_c.h>

namespace
{
using SZX::DMC::CaptureRequest;
using SZX::DMC::DmcButton;
using SZX::DMC::DmcButtonEvent;
using SZX::DMC::DmcButtonInput;
using SZX::DMC::DmcConfig;
using SZX::DMC::DmcController;
using SZX::DMC::DmcControllerFactory;
using SZX::DMC::DmcError;
using SZX::DMC::DmcState;
using SZX::DMC::FrameTriggerSource;
using SZX::DMC::StartSource;

auto makeController() -> std::unique_ptr<DmcController>
{
    auto result = DmcController::create(DmcConfig{.compareQueueCapacity = 8});
    EXPECT_TRUE(result.ok);
    return std::move(result.value);
}

auto makeCapture(StartSource source = StartSource::SoftwareCommand) -> CaptureRequest
{
    CaptureRequest request;
    request.operationId                = 42;
    request.startSource                = source;
    request.startInput                 = 1;
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
    auto result = DmcController::create(std::move(config));
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

TEST(DmcControllerTest, DigitalInputStartUsesOneActiveEdge)
{
    auto controller = makeController();
    ASSERT_TRUE(controller->connect().ok);
    auto request = makeCapture(StartSource::DigitalInput);
    ASSERT_TRUE(controller->armCapture(request).ok);

    ASSERT_TRUE(controller->signalDigitalInput(request.startInput, false).ok);
    ASSERT_TRUE(controller->signalDigitalInput(request.startInput, true).ok);
    const auto started = controller->status();
    ASSERT_TRUE(started.ok);
    EXPECT_EQ(started.value.state, DmcState::Scanning);
    EXPECT_EQ(started.value.startEdgeSequence, 1U);

    ASSERT_TRUE(controller->signalDigitalInput(request.startInput, true).ok);
    EXPECT_EQ(controller->status().value.startEdgeSequence, 1U);
}

TEST(DmcControllerTest, ButtonPollingReportsStartResetAndEmergencyStop)
{
    auto events                = std::vector<DmcButton>{};
    auto config                = DmcConfig{.compareQueueCapacity = 8};
    config.startButton         = DmcButtonInput{1, true};
    config.resetButton         = DmcButtonInput{2, true};
    config.emergencyStopButton = DmcButtonInput{3, true};
    config.buttonEventSink     = [&events](const DmcButtonEvent &event)
    {
        events.push_back(event.button);
    };
    auto result = DmcController::create(config);
    ASSERT_TRUE(result.ok);
    auto controller = std::move(result.value);
    ASSERT_TRUE(controller->connect().ok);

    ASSERT_TRUE(controller->signalDigitalInput(1, false).ok);
    ASSERT_TRUE(controller->signalDigitalInput(1, true).ok);
    ASSERT_TRUE(controller->signalDigitalInput(2, false).ok);
    ASSERT_TRUE(controller->signalDigitalInput(2, true).ok);
    ASSERT_TRUE(controller->signalDigitalInput(3, false).ok);
    ASSERT_TRUE(controller->signalDigitalInput(3, true).ok);

    ASSERT_EQ(events.size(), 3U);
    EXPECT_EQ(events[0], DmcButton::Start);
    EXPECT_EQ(events[1], DmcButton::Reset);
    EXPECT_EQ(events[2], DmcButton::EmergencyStop);
    ASSERT_TRUE(controller->status().ok);
    EXPECT_EQ(controller->status().value.state, DmcState::SafetyLatched);
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
