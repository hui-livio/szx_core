#include "SimulatedDmcController.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <utility>

namespace SZX::DMC::v1
{
namespace
{
using SZX::COMMON::Result;

template <typename T>
auto fail(DmcError error, const char *message) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(error), message);
}

auto timestamp() -> std::int64_t
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

class SimulatedDmcController final : public DmcController
{
public:
    explicit SimulatedDmcController(DmcConfig config) :
        config_(std::move(config))
    {
        profile_.model = config_.model;
        profile_.id    = config_.profileId;
        axes_.resize(profile_.axisCount);
        for (AxisId i = 0; i < axes_.size(); ++i)
            axes_[i].axis = i;
        outputs_.resize(config_.outputCount);
        sink_ = config_.logSink;
        sync();
    }

    auto connect() -> Result<void> override
    {
        LogSink sink;
        {
            std::scoped_lock lock(mutex_);
            if (status_.state == DmcState::Disconnected)
            {
                status_.state     = DmcState::Ready;
                status_.hardware  = false;
                status_.lastError = DmcError::Success;
            }
            sink = sink_;
        }
        log(sink, DmcLogLevel::Info, "DMC connected to the simulation controller.");
        return Result<void>::Success();
    }

    auto disconnect() -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        compare_ = {};
        request_ = {};
        armed_   = false;
        status_  = {};
        sync();
        return Result<void>::Success();
    }

    auto status() const -> Result<DmcStatus> override
    {
        std::scoped_lock lock(mutex_);
        auto snapshot = status_;
        snapshot.axes = axes_;
        return Result<DmcStatus>::Success(std::move(snapshot));
    }

    auto profile() const -> Result<DmcProfile> override
    {
        return Result<DmcProfile>::Success(profile_);
    }

    void setLogSink(LogSink sink) override
    {
        std::scoped_lock lock(mutex_);
        sink_ = std::move(sink);
    }

    auto enableAxis(AxisId axis) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkAxis(axis); !check.ok)
            return check;
        axes_[axis].enabled = true;
        return Result<void>::Success();
    }

    auto disableAxis(AxisId axis) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkAxis(axis); !check.ok)
            return check;
        axes_[axis].enabled = false;
        axes_[axis].moving  = false;
        return Result<void>::Success();
    }

    auto homeAxis(AxisId axis, const HomeProfile &profile) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkAxis(axis); !check.ok)
            return check;
        if (!axes_[axis].enabled)
            return fail<void>(DmcError::AxisNotReady, "The axis is not enabled.");
        if (!std::isfinite(profile.offset))
            return fail<void>(DmcError::InvalidArgument, "The homing offset is invalid.");
        axes_[axis].position       = profile.offset;
        axes_[axis].targetPosition = profile.offset;
        axes_[axis].homed          = true;
        return Result<void>::Success();
    }

    auto moveAbsolute(AxisId axis, double position, const MotionProfile &) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkAxis(axis); !check.ok)
            return check;
        if (!axes_[axis].enabled || !axes_[axis].homed)
            return fail<void>(DmcError::AxisNotReady, "The axis is not enabled or homed.");
        if (!std::isfinite(position))
            return fail<void>(DmcError::InvalidArgument, "The target position is invalid.");
        axes_[axis].position = axes_[axis].targetPosition = position;
        return Result<void>::Success();
    }

    auto moveRelative(AxisId axis, double distance, const MotionProfile &) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkAxis(axis); !check.ok)
            return check;
        if (!axes_[axis].enabled || !std::isfinite(distance))
            return fail<void>(DmcError::AxisNotReady, "The axis is not enabled or the distance is invalid.");
        axes_[axis].position       += distance;
        axes_[axis].targetPosition  = axes_[axis].position;
        return Result<void>::Success();
    }

    auto stop(AxisId axis, StopMode mode) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkAxis(axis); !check.ok)
            return check;
        axes_[axis].moving   = false;
        axes_[axis].velocity = 0.0;
        if (mode == StopMode::Emergency)
            latchSafety();
        return Result<void>::Success();
    }

    auto emergencyStop() -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return check;
        for (auto &axis : axes_)
        {
            axis.moving   = false;
            axis.velocity = 0.0;
        }
        latchSafety();
        return Result<void>::Success();
    }

    auto writeOutput(OutputId output, bool active) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return check;
        if (output >= outputs_.size())
            return fail<void>(DmcError::IoUnavailable, "The output index is out of range.");
        outputs_[output] = active;
        return Result<void>::Success();
    }

    auto readInputs() const -> Result<InputSnapshot> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return fail<InputSnapshot>(DmcError::NotConnected, "The controller is not connected.");
        return Result<InputSnapshot>::Success(inputs_);
    }

    auto configureCompare(const CompareConfig &config) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return check;
        if (armed_)
            return fail<void>(DmcError::Busy, "Position compare cannot be configured during capture.");
        const auto capacity = config.queueCapacity == 0 ? config_.compareQueueCapacity : config.queueCapacity;
        if (config.points.size() > capacity)
            return fail<void>(DmcError::CompareQueueFull, "The position compare queue has insufficient capacity.");
        if (config.axis >= axes_.size() || config.output >= outputs_.size())
            return fail<void>(DmcError::InvalidArgument, "The position compare axis or output is out of range.");
        compare_                      = config;
        compare_.queueCapacity        = capacity;
        status_.compareQueueRemaining = static_cast<std::uint32_t>(config.points.size());
        return Result<void>::Success();
    }

    auto enableCompare(bool enabled) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return check;
        if (enabled && compare_.queueCapacity == 0)
            return fail<void>(DmcError::CompareUnavailable, "Position compare is not configured.");
        status_.compareEnabled = enabled;
        return Result<void>::Success();
    }

    auto clearCompare() -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return check;
        clearCompareLocked();
        return Result<void>::Success();
    }

    auto compareTriggerCount() const -> Result<std::uint32_t> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return fail<std::uint32_t>(DmcError::NotConnected, "The controller is not connected.");
        return Result<std::uint32_t>::Success(status_.comparedTriggers);
    }

    auto armCapture(const CaptureRequest &request) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return check;
        if (status_.safetyLatched)
            return fail<void>(DmcError::SafetyLatched, "The safety stop is latched.");
        if (armed_)
            return fail<void>(DmcError::Busy, "A capture operation is already active.");
        const auto capacity = request.compareQueueCapacity == 0 ? config_.compareQueueCapacity : request.compareQueueCapacity;
        if (request.operationId == 0 || request.expectedFrames == 0)
            return fail<void>(DmcError::InvalidArgument, "The capture ID and frame count must be non-zero.");
        if (request.expectedFrames > capacity)
            return fail<void>(DmcError::CompareQueueFull, "The position compare queue has insufficient capacity.");
        if (request.triggerSource == FrameTriggerSource::PositionCompareOutput &&
            request.comparePoints.size() != request.expectedFrames)
            return fail<void>(DmcError::InvalidArgument, "The compare point count does not match the frame count.");
        if (request.scanPlan.scanAxis >= axes_.size() || request.scanPlan.rowAxis >= axes_.size() ||
            request.compareOutput >= outputs_.size() ||
            (request.startSource == StartSource::DigitalInput && request.startInput >= config_.inputCount))
            return fail<void>(DmcError::InvalidArgument, "A capture axis or IO index is out of range.");
        const auto columns = request.scanPlan.scanPositions.size();
        const auto rows    = request.scanPlan.rowPositions.size();
        if (columns && rows && columns * rows != request.expectedFrames)
            return fail<void>(DmcError::InvalidArgument, "The scan dimensions do not match the frame count.");
        request_                      = request;
        armed_                        = true;
        status_.operationId           = request.operationId;
        status_.expectedFrames        = request.expectedFrames;
        status_.comparedTriggers      = 0;
        status_.receivedFrames        = 0;
        status_.currentRow            = 0;
        status_.totalRows             = static_cast<std::uint32_t>(rows);
        status_.compareQueueRemaining = request.expectedFrames;
        status_.lastError             = DmcError::Success;
        status_.state = request.startSource == StartSource::DigitalInput ? DmcState::WaitingForStart : DmcState::Armed;
        return Result<void>::Success();
    }

    auto startCapture(OperationId operationId) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkOperation(operationId); !check.ok)
            return check;
        if (request_.startSource != StartSource::SoftwareCommand)
            return fail<void>(DmcError::InvalidArgument, "The capture is waiting for a digital input start.");
        return startLocked();
    }

    auto acknowledgeCompareTrigger(OperationId operationId, std::uint32_t count) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkOperation(operationId); !check.ok)
            return check;
        if (status_.state != DmcState::Scanning && status_.state != DmcState::WaitingForFrames)
            return fail<void>(DmcError::Busy, "The capture is not running.");
        if (count < status_.comparedTriggers || count > status_.expectedFrames)
            return fail<void>(DmcError::CompareQueueFull, "The trigger count is out of range.");
        status_.comparedTriggers      = count;
        status_.compareQueueRemaining = status_.expectedFrames - count;
        if (count == status_.expectedFrames && status_.receivedFrames < count)
            status_.state = DmcState::WaitingForFrames;
        return Result<void>::Success();
    }

    auto acknowledgeFrameProgress(OperationId operationId, FrameOrdinal savedFrames) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkOperation(operationId); !check.ok)
            return check;
        if (savedFrames < status_.receivedFrames || savedFrames > status_.expectedFrames ||
            (request_.triggerSource == FrameTriggerSource::PositionCompareOutput && savedFrames > status_.comparedTriggers))
            return fail<void>(DmcError::FrameProgressInvalid, "The camera frame progress is invalid.");
        status_.receivedFrames = savedFrames;
        const auto columns     = request_.scanPlan.scanPositions.size();
        const auto rows        = request_.scanPlan.rowPositions.size();
        if (columns && rows)
        {
            status_.currentRow = std::min<std::uint32_t>(
                static_cast<std::uint32_t>((savedFrames ? savedFrames - 1 : 0) / columns), static_cast<std::uint32_t>(rows - 1));
            if (savedFrames && savedFrames % columns == 0 && savedFrames < status_.expectedFrames)
                issueRowLocked(savedFrames / columns);
        }
        if (savedFrames == status_.expectedFrames && status_.comparedTriggers >= savedFrames)
            status_.state = DmcState::WaitingForFrames;
        return Result<void>::Success();
    }

    auto completeCamera(OperationId operationId, FrameOrdinal savedFrames) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkOperation(operationId); !check.ok)
            return check;
        if (savedFrames != status_.expectedFrames || status_.comparedTriggers < savedFrames)
            return fail<void>(DmcError::FrameProgressInvalid, "The camera frame count does not match the capture plan.");
        status_.receivedFrames = savedFrames;
        clearCompareLocked();
        armed_            = false;
        status_.state     = DmcState::Completed;
        status_.lastError = DmcError::Success;
        return Result<void>::Success();
    }

    auto failCapture(OperationId operationId, DmcError reason) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkOperation(operationId); !check.ok)
            return check;
        if (reason == DmcError::Success)
            return fail<void>(DmcError::InvalidArgument, "A capture failure requires a non-success reason.");
        clearCompareLocked();
        armed_            = false;
        status_.state     = reason == DmcError::SafetyLatched ? DmcState::SafetyLatched : DmcState::Failed;
        status_.lastError = reason;
        return Result<void>::Success();
    }

    auto cancelCapture(OperationId operationId, StopMode mode) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkOperation(operationId); !check.ok)
            return check;
        for (auto &axis : axes_)
        {
            axis.moving   = false;
            axis.velocity = 0.0;
        }
        clearCompareLocked();
        armed_ = false;
        if (mode == StopMode::Emergency)
            latchSafety();
        else
        {
            status_.state     = DmcState::Cancelled;
            status_.lastError = DmcError::Cancelled;
        }
        return Result<void>::Success();
    }

    auto pollCapture(OperationId operationId) const -> Result<DmcStatus> override
    {
        std::scoped_lock lock(mutex_);
        if (operationId && operationId != status_.operationId)
            return fail<DmcStatus>(DmcError::InvalidArgument, "The capture ID is stale or unknown.");
        auto snapshot = status_;
        snapshot.axes = axes_;
        return Result<DmcStatus>::Success(std::move(snapshot));
    }

    auto signalDigitalInput(InputId input, bool active) -> Result<void> override
    {
        std::scoped_lock lock(mutex_);
        if (auto check = checkConnected(); !check.ok)
            return check;
        if (input >= config_.inputCount || input >= 64)
            return fail<void>(DmcError::IoUnavailable, "The input index is out of range.");
        const auto mask     = std::uint64_t{1} << input;
        const bool valid    = (validInputBits_ & mask) != 0;
        const bool previous = (inputs_.sampledBits & mask) != 0;
        if (active)
            inputs_.sampledBits |= mask;
        else
            inputs_.sampledBits &= ~mask;
        inputs_.activeBits            = inputs_.sampledBits;
        inputs_.timestampNanoseconds  = timestamp();
        inputs_.valid                 = true;
        validInputBits_              |= mask;
        if (valid && previous != active && armed_ && status_.state == DmcState::WaitingForStart && request_.startInput == input &&
            active == request_.startInputActiveLevel)
        {
            ++status_.startEdgeSequence;
            status_.lastStartEdgeTimestampNanoseconds = inputs_.timestampNanoseconds;
            return startLocked();
        }
        return Result<void>::Success();
    }

private:
    static void log(const LogSink &sink, DmcLogLevel level, const std::string &message) noexcept
    {
        try
        {
            if (sink)
                sink(level, message);
            else
                std::cout << "[szx::dmc] " << message << std::endl;
        } catch (...)
        {
        }
    }

    auto checkConnected() const -> Result<void>
    {
        if (status_.state == DmcState::Disconnected)
            return fail<void>(DmcError::NotConnected, "The controller is not connected.");
        return Result<void>::Success();
    }

    auto checkAxis(AxisId axis) const -> Result<void>
    {
        if (auto check = checkConnected(); !check.ok)
            return check;
        if (axis >= axes_.size())
            return fail<void>(DmcError::InvalidArgument, "The axis index is out of range.");
        if (status_.safetyLatched)
            return fail<void>(DmcError::SafetyLatched, "The safety stop is latched.");
        return Result<void>::Success();
    }

    auto checkOperation(OperationId id) const -> Result<void>
    {
        if (!armed_ || id != status_.operationId)
            return fail<void>(DmcError::InvalidArgument, "The capture ID is stale or unknown.");
        return Result<void>::Success();
    }

    void clearCompareLocked()
    {
        compare_                      = {};
        status_.compareQueueRemaining = 0;
        status_.compareEnabled        = false;
    }

    void latchSafety()
    {
        clearCompareLocked();
        armed_                = false;
        status_.safetyLatched = true;
        status_.state         = DmcState::SafetyLatched;
        status_.lastError     = DmcError::SafetyLatched;
    }

    void sync()
    {
        status_.model      = config_.model;
        status_.profileId  = config_.profileId;
        status_.axes       = axes_;
        status_.inputValid = config_.inputCount <= 64;
    }

    void issueRowLocked(std::size_t row)
    {
        const auto &plan = request_.scanPlan;
        if (plan.scanPositions.empty() || plan.rowPositions.empty() || row >= plan.rowPositions.size())
            return;
        auto &rowAxis    = axes_[plan.rowAxis];
        rowAxis.position = rowAxis.targetPosition = plan.rowPositions[row];
        auto &scanAxis                            = axes_[plan.scanAxis];
        scanAxis.position = scanAxis.targetPosition = row % 2 ? plan.scanStartPosition : plan.scanEndPosition;
    }

    auto startLocked() -> Result<void>
    {
        if (status_.state != DmcState::Armed && status_.state != DmcState::WaitingForStart)
            return fail<void>(DmcError::Busy, "The capture is not armed.");
        status_.state          = DmcState::Scanning;
        status_.compareEnabled = request_.triggerSource == FrameTriggerSource::PositionCompareOutput;
        issueRowLocked(0);
        return Result<void>::Success();
    }

    DmcConfig config_;
    DmcProfile profile_;
    mutable std::mutex mutex_;
    DmcStatus status_;
    InputSnapshot inputs_;
    std::uint64_t validInputBits_ = 0;
    std::vector<AxisStatus> axes_;
    std::vector<bool> outputs_;
    CompareConfig compare_;
    CaptureRequest request_;
    LogSink sink_;
    bool armed_ = false;
};
} // namespace

auto createSimulatedController(DmcConfig config) -> std::unique_ptr<DmcController>
{
    return std::make_unique<SimulatedDmcController>(std::move(config));
}

} // namespace SZX::DMC::v1
