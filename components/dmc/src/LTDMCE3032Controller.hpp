#pragma once

/**
 * @file LTDMCE3032Controller.hpp
 * @brief Internal LeiSai LTDMC controller declaration.
 *
 * This header is private to the module implementation and is not installed.
 */

#include <szx/dmc/DmcController.hpp>

namespace SZX::DMC::v1
{

class LTDMCE3032Controller final : public DmcController
{
public:
    explicit LTDMCE3032Controller(DmcConfig config);
    ~LTDMCE3032Controller() noexcept override;

    SZX::COMMON::Result<void> connect() override;
    SZX::COMMON::Result<void> disconnect() override;
    SZX::COMMON::Result<DmcStatus> status() const override;
    SZX::COMMON::Result<DmcProfile> profile() const override;
    void setLogSink(LogSink sink) override;
    void setInputEventSink(InputEventSink sink) override;
    SZX::COMMON::Result<void> enableAxis(AxisId axis) override;
    SZX::COMMON::Result<void> disableAxis(AxisId axis) override;
    SZX::COMMON::Result<void> homeAxis(AxisId axis, const HomeProfile &profile = {}) override;
    SZX::COMMON::Result<void> moveAbsolute(AxisId axis, double position, const MotionProfile &profile = {}) override;
    SZX::COMMON::Result<void> moveRelative(AxisId axis, double distance, const MotionProfile &profile = {}) override;
    SZX::COMMON::Result<void> stop(AxisId axis, StopMode mode = StopMode::Decelerated) override;
    SZX::COMMON::Result<void> emergencyStop() override;
    SZX::COMMON::Result<void> writeOutput(OutputId output, bool active) override;
    SZX::COMMON::Result<InputSnapshot> readInputs() const override;
    SZX::COMMON::Result<void> configureCompare(const CompareConfig &config) override;
    SZX::COMMON::Result<void> enableCompare(bool enabled) override;
    SZX::COMMON::Result<void> clearCompare() override;
    SZX::COMMON::Result<std::uint32_t> compareTriggerCount() const override;
    SZX::COMMON::Result<void> armCapture(const CaptureRequest &request) override;
    SZX::COMMON::Result<void> startCapture(OperationId operationId, StartSource source = StartSource::SoftwareCommand) override;
    SZX::COMMON::Result<void> acknowledgeCompareTrigger(OperationId operationId, std::uint32_t count) override;
    SZX::COMMON::Result<void> acknowledgeFrameProgress(OperationId operationId, FrameOrdinal savedFrames) override;
    SZX::COMMON::Result<void> completeCamera(OperationId operationId, FrameOrdinal savedFrames) override;
    SZX::COMMON::Result<void> failCapture(OperationId operationId, DmcError reason) override;
    SZX::COMMON::Result<void> cancelCapture(OperationId operationId, StopMode mode = StopMode::Decelerated) override;
    SZX::COMMON::Result<DmcStatus> pollCapture(OperationId operationId) const override;
    SZX::COMMON::Result<void> signalDigitalInput(InputId input, bool active) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SZX::DMC::v1
