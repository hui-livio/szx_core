#include <szx/dmc/DmcController.hpp>

#include "LTDMCE3032Controller.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include <LTDMC.h>

namespace SZX::DMC::v1
{
namespace
{
using SZX::COMMON::Result;

template <typename T>
auto failure(DmcError error, std::string message) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(error), message);
}

template <typename ResultType>
struct ResultTraits;

template <typename T>
struct ResultTraits<Result<T>>
{
    using Value = T;
};

template <typename Function>
auto boundary(Function &&function) noexcept -> decltype(function())
{
    using Return = decltype(function());
    using Value  = typename ResultTraits<Return>::Value;
    try
    {
        return function();
    } catch (const std::bad_alloc &)
    {
        if constexpr (std::is_void_v<Value>)
            return Return::Failure(static_cast<int>(DmcError::ResourceExhausted), "DMC operation exhausted memory.");
        else
            return failure<Value>(DmcError::ResourceExhausted, "DMC operation exhausted memory.");
    } catch (const std::exception &)
    {
        if constexpr (std::is_void_v<Value>)
            return Return::Failure(static_cast<int>(DmcError::UnexpectedFailure), "DMC operation failed.");
        else
            return failure<Value>(DmcError::UnexpectedFailure, "DMC operation failed.");
    } catch (...)
    {
        if constexpr (std::is_void_v<Value>)
            return Return::Failure(static_cast<int>(DmcError::UnexpectedFailure), "DMC operation failed unexpectedly.");
        else
            return failure<Value>(DmcError::UnexpectedFailure, "DMC operation failed unexpectedly.");
    }
}

auto nowNanoseconds() noexcept -> std::int64_t
{
    const auto now = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
    return now.time_since_epoch().count();
}

inline auto isFinite(double value) noexcept -> bool
{
    return std::isfinite(value) != 0;
}

auto validMotionProfile(const MotionProfile &profile) noexcept -> bool
{
    return isFinite(profile.minVelocity) && isFinite(profile.maxVelocity) && isFinite(profile.acceleration) &&
        isFinite(profile.deceleration) && isFinite(profile.stopVelocity) && profile.minVelocity >= 0.0 &&
        profile.maxVelocity >= profile.minVelocity && profile.acceleration >= 0.0 && profile.deceleration >= 0.0 &&
        profile.stopVelocity >= 0.0;
}

auto validHomeProfile(const HomeProfile &profile) noexcept -> bool
{
    return isFinite(profile.lowVelocity) && isFinite(profile.highVelocity) && isFinite(profile.acceleration) &&
        isFinite(profile.deceleration) && isFinite(profile.offset) && profile.lowVelocity >= 0.0 &&
        profile.highVelocity >= profile.lowVelocity && profile.acceleration >= 0.0 && profile.deceleration >= 0.0;
}

auto compareDirection(const std::vector<ComparePoint> &points, std::size_t index) noexcept -> WORD
{
    if (points.size() < 2)
        return 1;
    const auto &point = points[index];
    if (index + 1 < points.size() && points[index + 1].row == point.row)
        return points[index + 1].position >= point.position ? 1 : 0;
    if (index > 0 && points[index - 1].row == point.row)
        return point.position >= points[index - 1].position ? 1 : 0;
    return 1;
}

auto isFirstScanFrame(const std::vector<ComparePoint> &points, std::size_t index, std::size_t columns) noexcept -> bool
{
    if (index == 0)
        return true;
    if (columns != 0)
        return index % columns == 0;
    return points[index].row != points[index - 1].row;
}

std::mutex connectedCardsMutex;
std::unordered_set<std::uint16_t> connectedCards;
// All three board-lifetime values are guarded by connectedCardsMutex.
std::size_t boardUsers = 0;
bool boardInitialized = false;

auto closeBoardIfUnusedLocked() noexcept -> bool
{
    if (boardUsers != 0 || !boardInitialized)
        return true;
    if (dmc_board_close() != 0)
        return false;
    boardInitialized = false;
    return true;
}

} // namespace

struct LTDMCE3032Controller::Impl
{
    explicit Impl(DmcConfig value) :
        config(std::move(value))
    {
        profile.model = config.model;
        profile.id    = config.profileId;
        if (!config.logSink)
        {
            config.logSink = [](const DmcLogLevel level, const std::string &message)
            {
                auto &stream = level == DmcLogLevel::Error || level == DmcLogLevel::Warning ? std::cerr : std::cout;
                stream << "[szx::dmc] " << message << std::endl;
            };
        }
        logSink   = config.logSink;
        inputSink = config.inputEventSink;
        axes.resize(2);
        for (AxisId axis = 0; axis < axes.size(); ++axis)
            axes[axis].axis = axis;
        outputs.resize(config.outputCount);
        status.axes       = axes;
        status.inputValid = config.inputCount <= 64 && config.outputCount <= 64;
        status.model      = config.model;
        status.profileId  = config.profileId;
    }

    DmcConfig config;
    DmcProfile profile;
    mutable std::mutex mutex;
    std::mutex sdkMutex;
    DmcStatus status;
    InputSnapshot inputs;
    std::vector<AxisStatus> axes;
    std::vector<bool> outputs;
    LogSink logSink;
    InputEventSink inputSink;
    CompareConfig compare;
    CaptureRequest request;
    std::jthread inputWorker;
    std::uint64_t inputSampleValidBits    = 0;
    std::uint16_t cardId                  = 0;
    DWORD axisCount                       = 2;
    std::uint16_t localInputCount         = 0;
    std::uint16_t localOutputCount        = 0;
    bool hardwareInitialized              = false;
    bool cardClaimed                      = false;
    std::atomic<bool> motionAbortRequested{false};
    bool captureArmed                     = false;
    std::uint32_t compareExpectedTriggers = 0;
    std::uint32_t manualTriggerFrames     = 0;
    std::array<std::chrono::steady_clock::time_point, 2> homeDeadlines{};
    std::array<double, 2> homeStartPositions{};
    std::array<double, 2> homeLastPositions{};
    std::array<bool, 2> homeOriginSeen{};
    std::array<bool, 2> homeDirectionVerified{};

    auto syncStatus() -> void
    {
        status.axes       = axes;
        status.inputValid = config.inputCount <= 64 && config.outputCount <= 64;
        status.model      = config.model;
        status.profileId  = config.profileId;
    }

    auto connected() const noexcept -> bool
    {
        return status.state != DmcState::Disconnected;
    }

    auto sdkCard() const noexcept -> WORD
    {
        return static_cast<WORD>(cardId);
    }

    auto validateProfile() const -> Result<void>
    {
        if (config.profileId.empty())
            return failure<void>(DmcError::InvalidArgument, "DMC profile id is required.");
        return Result<void>::Success();
    }

    auto activeCapture() const noexcept -> bool
    {
        return captureArmed &&
            (status.state == DmcState::Armed || status.state == DmcState::WaitingForStart ||
             status.state == DmcState::Preparing || status.state == DmcState::Scanning ||
             status.state == DmcState::WaitingForFrames);
    }

    auto clearCompareLocked() -> Result<void>
    {
        std::scoped_lock sdkLock(sdkMutex);
        bool failed = false;
        failed = dmc_compare_set_config(sdkCard(), static_cast<WORD>(compare.axis), 0, static_cast<WORD>(compare.source)) != 0;
        failed = dmc_compare_clear_points(sdkCard(), static_cast<WORD>(compare.axis)) != 0 || failed;
        compare.points.clear();
        compare.queueCapacity        = 0;
        status.compareQueueRemaining = 0;
        status.compareEnabled        = false;
        compareExpectedTriggers      = 0;
        manualTriggerFrames          = 0;
        if (failed)
            return failure<void>(DmcError::CompareUnavailable, "LTDMC compare cleanup failed.");
        return Result<void>::Success();
    }

    auto releaseCardClaimLocked() noexcept -> void
    {
        if (!cardClaimed)
            return;
        connectedCards.erase(cardId);
        cardClaimed = false;
        if (boardUsers != 0)
            --boardUsers;
    }

    auto issueAxisMoveLocked(AxisId axis, double position, const MotionProfile &profile, bool absolute) -> Result<void>
    {
        (void)profile;
        if (axis >= axes.size() || !isFinite(position) || !validMotionProfile(profile))
            return failure<void>(DmcError::InvalidArgument, "Scan axis or position is invalid.");
        if (!axes[axis].enabled || !axes[axis].homed)
            return failure<void>(DmcError::AxisNotReady, "Scan axes must be enabled and homed.");
        WORD axisError = 0;
        WORD busError  = 0;
        if (nmc_get_axis_errcode(sdkCard(), static_cast<WORD>(axis), &axisError) != 0 ||
            nmc_get_errcode(sdkCard(), 2, &busError) != 0 || axisError != 0 || busError != 0)
            return failure<void>(DmcError::AxisNotReady, "LTDMC axis or bus error prevents motion.");
        if (dmc_set_profile_unit(sdkCard(), static_cast<WORD>(axis), profile.minVelocity, profile.maxVelocity,
                                 profile.acceleration, profile.deceleration, profile.stopVelocity) != 0 ||
            dmc_pmove_unit(sdkCard(), static_cast<WORD>(axis), position, absolute ? 1 : 0) != 0)
            return failure<void>(DmcError::AxisNotReady, "LTDMC scan motion failed.");
        axes[axis].targetPosition = absolute ? position : axes[axis].position + position;
        axes[axis].position       = axes[axis].targetPosition;
        axes[axis].velocity       = 0.0;
        axes[axis].moving         = true;
        return Result<void>::Success();
    }

    auto stopAxesLocked(StopMode mode) -> Result<void>
    {
        motionAbortRequested.store(true, std::memory_order_release);
        std::scoped_lock sdkLock(sdkMutex);
        if (mode == StopMode::Emergency)
        {
            if (dmc_emg_stop(sdkCard()) != 0)
                return failure<void>(DmcError::MotionTimeout, "LTDMC emergency stop failed.");
        }
        else
        {
            for (AxisId axis = 0; axis < axes.size(); ++axis)
                if (dmc_stop(sdkCard(), static_cast<WORD>(axis), 0) != 0)
                    return failure<void>(DmcError::MotionTimeout, "LTDMC capture stop failed.");
        }
        for (auto &axis : axes)
        {
            axis.moving   = false;
            axis.velocity = 0.0;
        }
        return Result<void>::Success();
    }

    auto waitAxisDone(std::unique_lock<std::mutex> &stateLock, AxisId axis) -> Result<void>
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (motionAbortRequested.load(std::memory_order_acquire))
                return status.state == DmcState::Failed ?
                    failure<void>(status.lastError, "LTDMC motion wait stopped after a controller failure.") :
                    failure<void>(DmcError::Cancelled, "LTDMC motion wait was cancelled.");

            stateLock.unlock();
            int done       = -1;
            WORD axisError = 0;
            WORD busError  = 0;
            {
                std::scoped_lock sdkLock(sdkMutex);
                done = dmc_check_done(sdkCard(), static_cast<WORD>(axis));
                if (done >= 0)
                {
                    if (nmc_get_axis_errcode(sdkCard(), static_cast<WORD>(axis), &axisError) != 0 ||
                        nmc_get_errcode(sdkCard(), 2, &busError) != 0)
                        done = -1;
                }
            }
            stateLock.lock();
            if (done < 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC axis completion polling failed.");
            if (axisError != 0 || busError != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC axis reported an error while moving.");
            if (done == 1)
            {
                axes[axis].moving   = false;
                axes[axis].velocity = 0.0;
                return Result<void>::Success();
            }
            stateLock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            stateLock.lock();
        }
        return failure<void>(DmcError::MotionTimeout, "LTDMC axis motion timed out.");
    }

    auto issueScanRowLocked(std::unique_lock<std::mutex> &stateLock, std::size_t row) -> Result<void>
    {
        if (request.scanPlan.scanPositions.empty() || request.scanPlan.rowPositions.empty())
            return Result<void>::Success();
        if (row >= request.scanPlan.rowPositions.size())
            return failure<void>(DmcError::InvalidArgument, "Scan row is outside the scan plan.");
        const auto rowMove =
            issueAxisMoveLocked(request.scanPlan.rowAxis, request.scanPlan.rowPositions[row], request.scanPlan.rowProfile, true);
        if (!rowMove.ok)
            return rowMove;
        if (auto done = waitAxisDone(stateLock, request.scanPlan.rowAxis); !done.ok)
            return done;
        if (!captureArmed || motionAbortRequested.load(std::memory_order_acquire))
            return failure<void>(DmcError::Cancelled, "LTDMC capture motion was cancelled.");
        const bool reverse = (row & 1U) != 0;
        const auto start   = reverse ? request.scanPlan.scanEndPosition : request.scanPlan.scanStartPosition;
        const auto scan    = issueAxisMoveLocked(request.scanPlan.scanAxis, start, request.scanPlan.scanProfile, true);
        if (!scan.ok)
            return scan;
        return waitAxisDone(stateLock, request.scanPlan.scanAxis);
    }

    auto issueScanSweepLocked(std::size_t row) -> Result<void>
    {
        const bool reverse = (row & 1U) != 0;
        const auto end     = reverse ? request.scanPlan.scanStartPosition : request.scanPlan.scanEndPosition;
        return issueAxisMoveLocked(request.scanPlan.scanAxis, end, request.scanPlan.scanProfile, true);
    }

    auto pulseOutputLocked(OutputId output, bool activeLevel) -> Result<void>
    {
        if (output >= outputs.size())
            return failure<void>(DmcError::IoUnavailable, "Capture trigger output is outside the configured IO range.");
        const auto active   = static_cast<WORD>(activeLevel ? 1 : 0);
        const auto inactive = static_cast<WORD>(activeLevel ? 0 : 1);
        if (dmc_write_outbit(sdkCard(), static_cast<WORD>(output), active) != 0)
            return failure<void>(DmcError::IoUnavailable, "LTDMC software trigger output failed.");
        std::this_thread::sleep_for(std::chrono::milliseconds(config.softwareTriggerPulseMilliseconds));
        if (dmc_write_outbit(sdkCard(), static_cast<WORD>(output), inactive) != 0)
            return failure<void>(DmcError::IoUnavailable, "LTDMC software trigger release failed.");
        outputs[output] = false;
        return Result<void>::Success();
    }

    auto validateCompareQueueLocked(AxisId axis, std::uint16_t source, bool enabled, long expectedPoints = -1,
                                    long availableBefore = -1) -> Result<void>
    {
        WORD actualEnabled  = 0;
        WORD actualSource   = 0;
        double currentPoint = 0.0;
        long runned         = 0;
        long remained       = 0;
        if (dmc_compare_get_config(sdkCard(), static_cast<WORD>(axis), &actualEnabled, &actualSource) != 0 ||
            dmc_compare_get_current_point_unit(sdkCard(), static_cast<WORD>(axis), &currentPoint) != 0 ||
            dmc_compare_get_points_runned(sdkCard(), static_cast<WORD>(axis), &runned) != 0 ||
            dmc_compare_get_points_remained(sdkCard(), static_cast<WORD>(axis), &remained) != 0 ||
            actualEnabled != static_cast<WORD>(enabled ? 1 : 0) || actualSource != static_cast<WORD>(source) ||
            !isFinite(currentPoint) || runned < 0 || remained < 0 || (expectedPoints >= 0 && runned != 0) ||
            (availableBefore >= 0 &&
             (availableBefore < remained ||
              (availableBefore - remained != expectedPoints && availableBefore - remained != expectedPoints - 1))))
            return failure<void>(DmcError::CompareUnavailable, "LTDMC compare queue readback failed.");
        return Result<void>::Success();
    }

    auto emergencyStopLocked() -> Result<void>
    {
        motionAbortRequested.store(true, std::memory_order_release);
        const auto clear     = clearCompareLocked();
        captureArmed         = false;
        status.safetyLatched = true;
        status.state         = DmcState::SafetyLatched;
        status.lastError     = DmcError::SafetyLatched;
        bool stopFailed      = false;
        std::scoped_lock sdkLock(sdkMutex);
        if (dmc_emg_stop(sdkCard()) != 0)
            stopFailed = true;
        for (AxisId axis = 0; axis < axes.size(); ++axis)
        {
            axes[axis].moving   = false;
            axes[axis].velocity = 0.0;
        }
        if (stopFailed)
        {
            status.lastError = DmcError::MotionTimeout;
            return failure<void>(DmcError::MotionTimeout, "LTDMC emergency stop failed.");
        }
        if (!clear.ok)
            return clear;
        return Result<void>::Success();
    }

    auto processInputSampleLocked(InputId input, bool active) -> Result<bool>
    {
        if (input >= config.inputCount || input >= 64)
            return failure<bool>(DmcError::IoUnavailable, "Input is outside the configured IO range.");
        const auto mask            = std::uint64_t{1} << input;
        const bool previouslyValid = (inputSampleValidBits & mask) != 0;
        const bool previousActive  = (inputs.sampledBits & mask) != 0;
        if (active)
            inputs.sampledBits |= mask;
        else
            inputs.sampledBits &= ~mask;
        inputs.activeBits            = inputs.sampledBits;
        inputs.valid                 = true;
        inputs.timestampNanoseconds  = nowNanoseconds();
        inputSampleValidBits        |= mask;
        return Result<bool>::Success(!previouslyValid || previousActive != active);
    }

    auto inputLoop(std::stop_token stopToken) noexcept -> void
    {
        try
        {
            while (!stopToken.stop_requested())
            {
                bool inputFailure = false;
                bool inputChanged = false;
                InputEventSink sink;
                InputSnapshot snapshot;
                {
                    std::scoped_lock lock(mutex);
                    if (!connected())
                        break;
                    for (InputId input = 0; input < config.inputCount; ++input)
                    {
                        int value = -1;
                        {
                            std::scoped_lock sdkLock(sdkMutex);
                            value = dmc_read_inbit(sdkCard(), static_cast<WORD>(input));
                        }
                        if (value < 0)
                        {
                            (void)stopAxesLocked(StopMode::Emergency);
                            (void)clearCompareLocked();
                            captureArmed      = false;
                            status.inputValid = false;
                            status.state      = DmcState::Failed;
                            status.lastError  = DmcError::IoUnavailable;
                            inputFailure      = true;
                            break;
                        }
                        const auto result = processInputSampleLocked(input, value != 0);
                        if (!result.ok)
                        {
                            inputFailure = true;
                            break;
                        }
                        inputChanged = inputChanged || result.value;
                    }
                    if (!inputFailure)
                        for (AxisId axis = 0; axis < axes.size(); ++axis)
                        {
                            double position           = 0.0;
                            WORD axisState            = 0;
                            WORD axisError            = 0;
                            WORD busError             = 0;
                            int positionResult = -1;
                            int done            = -1;
                            {
                                std::scoped_lock sdkLock(sdkMutex);
                                positionResult = dmc_get_position_unit(sdkCard(), static_cast<WORD>(axis), &position);
                                done           = dmc_check_done(sdkCard(), static_cast<WORD>(axis));
                                if (positionResult == 0 && done >= 0 &&
                                    (nmc_get_axis_state_machine(sdkCard(), static_cast<WORD>(axis), &axisState) != 0 ||
                                     nmc_get_axis_errcode(sdkCard(), static_cast<WORD>(axis), &axisError) != 0 ||
                                     nmc_get_errcode(sdkCard(), 2, &busError) != 0))
                                    positionResult = -1;
                            }
                            if (positionResult != 0 || done < 0 ||
                                axisError != 0 || busError != 0 || !isFinite(position))
                            {
                                (void)stopAxesLocked(StopMode::Emergency);
                                (void)clearCompareLocked();
                                captureArmed     = false;
                                status.state     = DmcState::Failed;
                                status.lastError = DmcError::AxisNotReady;
                                inputFailure     = true;
                                break;
                            }
                            axes[axis].position  = position;
                            axes[axis].moving    = done == 0;
                            axes[axis].enabled   = axisState == 4;
                            axes[axis].errorCode = axisError != 0 ? axisError : busError;
                            if (axes[axis].homing)
                            {
                                const bool originActive =
                                    (((inputs.sampledBits >> config.homeOriginInput) & 1U) != 0U) == config.homeOriginActiveLevel;
                                homeOriginSeen[axis] = homeOriginSeen[axis] || originActive;
                                const auto delta     = position - homeLastPositions[axis];
                                if (!homeDirectionVerified[axis] && std::abs(delta) > 1e-9)
                                {
                                    const auto direction = delta > 0.0 ? 1 : -1;
                                    if (direction != config.homeExpectedInitialDirection)
                                    {
                                        {
                                            std::scoped_lock sdkLock(sdkMutex);
                                            (void)dmc_emg_stop(sdkCard());
                                        }
                                        axes[axis].homing = false;
                                        axes[axis].homed  = false;
                                        status.state      = DmcState::Failed;
                                        status.lastError  = DmcError::AxisNotReady;
                                        inputFailure      = true;
                                        break;
                                    }
                                    homeDirectionVerified[axis] = true;
                                }
                                homeLastPositions[axis] = position;
                                if (config.homeMaxTravel > 0.0 &&
                                    std::abs(position - homeStartPositions[axis]) > config.homeMaxTravel)
                                {
                                    {
                                        std::scoped_lock sdkLock(sdkMutex);
                                        (void)dmc_emg_stop(sdkCard());
                                    }
                                    axes[axis].homing = false;
                                    axes[axis].homed  = false;
                                    status.state      = DmcState::Failed;
                                    status.lastError  = DmcError::MotionTimeout;
                                    inputFailure      = true;
                                    break;
                                }
                                if (std::chrono::steady_clock::now() > homeDeadlines[axis])
                                {
                                    {
                                        std::scoped_lock sdkLock(sdkMutex);
                                        (void)dmc_emg_stop(sdkCard());
                                    }
                                    axes[axis].homing = false;
                                    axes[axis].homed  = false;
                                    status.state      = DmcState::Failed;
                                    status.lastError  = DmcError::MotionTimeout;
                                    inputFailure      = true;
                                    break;
                                }
                                WORD homeResult = 0;
                                {
                                    std::scoped_lock sdkLock(sdkMutex);
                                    if (dmc_get_home_result(sdkCard(), static_cast<WORD>(axis), &homeResult) != 0)
                                        homeResult = 0xffff;
                                }
                                if (homeResult == 0xffff)
                                {
                                    status.state     = DmcState::Failed;
                                    status.lastError = DmcError::AxisNotReady;
                                    inputFailure     = true;
                                    break;
                                }
                                if (homeResult == 1 && done == 1 && (!config.homeRequireOriginSeen || homeOriginSeen[axis]))
                                {
                                    bool resetFailed = false;
                                    {
                                        std::scoped_lock sdkLock(sdkMutex);
                                        resetFailed = dmc_set_position_unit(sdkCard(), static_cast<WORD>(axis), 0.0) != 0 ||
                                            dmc_set_encoder_unit(sdkCard(), static_cast<WORD>(axis), 0.0) != 0;
                                    }
                                    if (resetFailed)
                                    {
                                        status.state     = DmcState::Failed;
                                        status.lastError = DmcError::AxisNotReady;
                                        inputFailure     = true;
                                        break;
                                    }
                                    axes[axis].position         = 0.0;
                                    axes[axis].targetPosition   = 0.0;
                                    axes[axis].homing           = false;
                                    axes[axis].homed            = true;
                                    axes[axis].moving           = false;
                                    homeDeadlines[axis]         = {};
                                    homeOriginSeen[axis]        = false;
                                    homeDirectionVerified[axis] = false;
                                }
                                else if (homeResult == 1 && done == 1)
                                {
                                    {
                                        std::scoped_lock sdkLock(sdkMutex);
                                        (void)dmc_emg_stop(sdkCard());
                                    }
                                    axes[axis].homing = false;
                                    axes[axis].homed  = false;
                                    status.state      = DmcState::Failed;
                                    status.lastError  = DmcError::AxisNotReady;
                                    inputFailure      = true;
                                    break;
                                }
                                else if (homeResult == 2)
                                {
                                    axes[axis].homing           = false;
                                    axes[axis].homed            = false;
                                    homeDeadlines[axis]         = {};
                                    homeOriginSeen[axis]        = false;
                                    homeDirectionVerified[axis] = false;
                                    status.state                = DmcState::Failed;
                                    status.lastError            = DmcError::AxisNotReady;
                                    inputFailure                = true;
                                    break;
                                }
                            }
                        }
                    if (!inputFailure && inputChanged)
                    {
                        snapshot = inputs;
                        sink     = inputSink;
                    }
                }
                if (sink)
                    try
                    {
                        sink(snapshot);
                    } catch (...)
                    {
                        // Host callbacks must not terminate the polling worker.
                    }
                if (inputFailure)
                    log(DmcLogLevel::Error, "LTDMC input polling failed.");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (...)
        {
            markInputWorkerFailure();
        }
    }

    auto markInputWorkerFailure() noexcept -> void
    {
        try
        {
            std::scoped_lock lock(mutex);
            status.inputValid = false;
            status.state      = DmcState::Failed;
            status.lastError  = DmcError::UnexpectedFailure;
        } catch (...)
        {
        }
    }

    auto log(DmcLogLevel level, const std::string &message) noexcept -> void
    {
        LogSink sink;
        try
        {
            std::scoped_lock lock(mutex);
            sink = logSink;
        } catch (...)
        {
            return;
        }
        if (!sink)
            return;
        try
        {
            sink(level, message);
        } catch (...)
        {
            // Host logging must never break a device operation boundary.
        }
    }

    auto startCaptureLocked(std::unique_lock<std::mutex> &stateLock) -> Result<void>
    {
        if (!captureArmed || (status.state != DmcState::Armed && status.state != DmcState::WaitingForStart))
            return failure<void>(DmcError::Busy, "DMC capture is not armed.");

        status.state     = DmcState::Preparing;
        status.lastError = DmcError::Success;
        status.compareEnabled =
            request.triggerSource == FrameTriggerSource::PositionCompareOutput && compareExpectedTriggers != 0;
        status.comparedTriggers      = 0;
        status.receivedFrames        = 0;
        status.currentRow            = 0;
        status.compareQueueRemaining = compareExpectedTriggers;
        status.totalRows             = static_cast<std::uint32_t>(request.scanPlan.rowPositions.size());
        if (request.triggerSource == FrameTriggerSource::PositionCompareOutput && compareExpectedTriggers != 0)
        {
            if (dmc_compare_set_config(sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis), 1,
                                       static_cast<WORD>(request.compareSource)) != 0)
            {
                (void)clearCompareLocked();
                status.state     = DmcState::Failed;
                status.lastError = DmcError::CompareUnavailable;
                return failure<void>(DmcError::CompareUnavailable, "LTDMC compare enable failed.");
            }
            if (auto readback = validateCompareQueueLocked(request.scanPlan.scanAxis, request.compareSource, true); !readback.ok)
            {
                (void)clearCompareLocked();
                status.state     = DmcState::Failed;
                status.lastError = DmcError::CompareUnavailable;
                captureArmed     = false;
                return readback;
            }
        }
        motionAbortRequested.store(false, std::memory_order_release);
        const auto scan = issueScanRowLocked(stateLock, 0);
        if (!scan.ok)
        {
            (void)stopAxesLocked(StopMode::Decelerated);
            (void)clearCompareLocked();
            captureArmed     = false;
            status.state     = DmcState::Failed;
            status.lastError = static_cast<DmcError>(scan.error);
            return scan;
        }
        if (auto pulse = pulseOutputLocked(request.compareOutput, request.compareOutputActiveLevel); !pulse.ok)
        {
            (void)stopAxesLocked(StopMode::Emergency);
            captureArmed     = false;
            status.state     = DmcState::Failed;
            status.lastError = static_cast<DmcError>(pulse.error);
            return pulse;
        }
        if (auto sweep = issueScanSweepLocked(0); !sweep.ok)
        {
            (void)stopAxesLocked(StopMode::Decelerated);
            (void)clearCompareLocked();
            captureArmed     = false;
            status.state     = DmcState::Failed;
            status.lastError = static_cast<DmcError>(sweep.error);
            return sweep;
        }
        status.state = DmcState::Scanning;
        return Result<void>::Success();
    }

    auto validateCaptureRequest(const CaptureRequest &capture) const -> Result<void>
    {
        if (!connected())
            return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
        if (capture.operationId == 0 || capture.expectedFrames == 0)
            return failure<void>(DmcError::InvalidArgument, "Capture operation and expected frame count are required.");
        if (!isValidStartSourceMask(capture.startSource))
            return failure<void>(DmcError::InvalidArgument, "Capture start source mask is invalid.");
        const auto capacity = capture.compareQueueCapacity == 0 ? config.compareQueueCapacity : capture.compareQueueCapacity;
        const auto columns  = capture.scanPlan.scanPositions.size();
        std::size_t comparePoints = 0;
        if (capture.triggerSource == FrameTriggerSource::PositionCompareOutput)
            for (std::size_t index = 0; index < capture.comparePoints.size(); ++index)
                comparePoints += !isFirstScanFrame(capture.comparePoints, index, columns);
        if (capacity == 0 || comparePoints > capacity)
            return failure<void>(DmcError::CompareQueueFull, "Capture compare queue capacity is insufficient.");
        if (capture.triggerSource != FrameTriggerSource::Software &&
            capture.triggerSource != FrameTriggerSource::PositionCompareOutput)
            return failure<void>(DmcError::InvalidArgument, "Capture trigger source is invalid.");
        if (!validMotionProfile(capture.scanPlan.scanProfile) || !validMotionProfile(capture.scanPlan.rowProfile))
            return failure<void>(DmcError::InvalidArgument, "Capture motion profile is invalid.");
        if (capture.scanPlan.scanPositions.empty() || capture.scanPlan.rowPositions.empty())
            return failure<void>(DmcError::InvalidArgument, "Capture scan plan must contain scan and row positions.");
        if (capture.triggerSource == FrameTriggerSource::PositionCompareOutput &&
            capture.comparePoints.size() != capture.expectedFrames)
            return failure<void>(DmcError::InvalidArgument, "Compare point count does not match expected frame count.");
        if (capture.scanPlan.scanAxis >= axes.size() || capture.scanPlan.rowAxis >= axes.size())
            return failure<void>(DmcError::InvalidArgument, "Scan plan axis is outside the controller range.");
        if (!capture.scanPlan.scanPositions.empty() && !capture.scanPlan.rowPositions.empty() &&
            capture.scanPlan.scanPositions.size() * capture.scanPlan.rowPositions.size() != capture.expectedFrames)
            return failure<void>(DmcError::InvalidArgument, "Scan plan dimensions do not match expected frame count.");
        if (capture.compareOutput >= config.outputCount)
            return failure<void>(DmcError::IoUnavailable, "Capture compare output is outside the configured IO range.");
        for (const auto &point : capture.comparePoints)
            if (!isFinite(point.position))
                return failure<void>(DmcError::InvalidArgument, "Capture compare point position is invalid.");
        return Result<void>::Success();
    }

    auto validateAxis(AxisId axis, bool allowDuringCapture = false) const -> Result<void>
    {
        if (!connected())
            return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
        if (axis >= axes.size())
            return failure<void>(DmcError::InvalidArgument, "Axis is outside the controller range.");
        if (status.safetyLatched)
            return failure<void>(DmcError::SafetyLatched, "DMC safety state is latched.");
        if (!allowDuringCapture && activeCapture())
            return failure<void>(DmcError::Busy, "Axis operation is unavailable during capture.");
        return Result<void>::Success();
    }
};

LTDMCE3032Controller::LTDMCE3032Controller(DmcConfig config) :
    impl_(std::make_unique<Impl>(std::move(config)))
{
}

LTDMCE3032Controller::~LTDMCE3032Controller() noexcept
{
    if (impl_)
        (void)disconnect();
}

auto LTDMCE3032Controller::connect() -> SZX::COMMON::Result<void>
{
    return boundary(
        [this]() -> Result<void>
        {
            std::unique_lock lock(impl_->mutex);
            if (impl_->connected())
                return Result<void>::Success();
            if (auto profile = impl_->validateProfile(); !profile.ok)
                return profile;

            short boardCount      = 0;
            bool initializedHere = false;
            {
                std::scoped_lock cardLock(connectedCardsMutex);
                if (!boardInitialized)
                {
                    boardCount = dmc_board_init();
                    if (boardCount > 0)
                    {
                        boardInitialized = true;
                        initializedHere  = true;
                    }
                }
                else
                {
                    WORD activeCards = 0;
                    DWORD cardTypes[16]{};
                    WORD cardIds[16]{};
                    boardCount =
                        dmc_get_CardInfList(&activeCards, cardTypes, cardIds) == 0 ? static_cast<short>(activeCards) : -1;
                }
            }
            if (boardCount <= 0)
                return failure<void>(DmcError::DriverUnavailable, "LTDMC board initialization failed.");
            {
                std::scoped_lock cardLock(connectedCardsMutex);
                WORD cardCount = 0;
                DWORD cardTypes[16]{};
                WORD cardIds[16]{};
                if (boardCount > 16 || dmc_get_CardInfList(&cardCount, cardTypes, cardIds) != 0 ||
                    cardCount != static_cast<WORD>(boardCount) || impl_->config.cardIndex >= cardCount)
                {
                    if (initializedHere)
                        (void)closeBoardIfUnusedLocked();
                    return failure<void>(DmcError::BoardNotFound, "LTDMC card enumeration or index validation failed.");
                }
                impl_->cardId = cardIds[impl_->config.cardIndex];
                if (nmc_get_total_axes(impl_->sdkCard(), &impl_->axisCount) != 0 || impl_->axisCount < 2 ||
                    dmc_get_total_ionum(impl_->sdkCard(), &impl_->localInputCount, &impl_->localOutputCount) != 0 ||
                    impl_->config.inputCount > impl_->localInputCount || impl_->config.outputCount > impl_->localOutputCount)
                {
                    if (initializedHere)
                        (void)closeBoardIfUnusedLocked();
                    return failure<void>(DmcError::IoUnavailable, "LTDMC card capabilities do not match the configuration.");
                }
                if (connectedCards.contains(impl_->cardId))
                {
                    if (initializedHere)
                        (void)closeBoardIfUnusedLocked();
                    return failure<void>(DmcError::Busy, "DMC card is already connected by another controller.");
                }
                connectedCards.insert(impl_->cardId);
                impl_->cardClaimed = true;
                ++boardUsers;
                impl_->hardwareInitialized = true;
            }
            impl_->status            = {};
            impl_->status.state      = DmcState::Ready;
            impl_->status.hardware   = true;
            impl_->status.inputValid = impl_->config.inputCount <= 64;
            impl_->status.lastError  = DmcError::Success;
            impl_->syncStatus();
            try
            {
                impl_->inputWorker = std::jthread([impl = impl_.get()](std::stop_token token) { impl->inputLoop(token); });
            } catch (...)
            {
                bool closeFailed = false;
                {
                    std::scoped_lock cardLock(connectedCardsMutex);
                    if (boardUsers <= 1)
                    {
                        closeFailed = !closeBoardIfUnusedLocked();
                        if (!closeFailed)
                            impl_->releaseCardClaimLocked();
                    }
                    else
                    {
                        impl_->releaseCardClaimLocked();
                    }
                }
                if (closeFailed)
                {
                    impl_->hardwareInitialized = true;
                    impl_->status.state        = DmcState::Failed;
                    impl_->status.lastError    = DmcError::DriverUnavailable;
                    impl_->syncStatus();
                }
                else
                {
                    impl_->hardwareInitialized = false;
                    impl_->status               = {};
                }
                throw;
            }
            lock.unlock();
            impl_->log(DmcLogLevel::Info, "DMC board connected.");
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::disconnect() -> SZX::COMMON::Result<void>
{
    return boundary(
        [this]() -> Result<void>
        {
            std::jthread worker;
            auto stopResult  = Result<void>::Success();
            auto clearResult = Result<void>::Success();
            auto closeResult = Result<void>::Success();
            {
                std::scoped_lock lock(impl_->mutex);
                if (!impl_->connected())
                    return Result<void>::Success();
                if (impl_->inputWorker.joinable() && impl_->inputWorker.get_id() == std::this_thread::get_id())
                    return failure<void>(DmcError::Busy, "DMC disconnect cannot join its input worker thread.");
                stopResult = impl_->stopAxesLocked(StopMode::Decelerated);
                impl_->inputWorker.request_stop();
                worker      = std::move(impl_->inputWorker);
                clearResult = impl_->clearCompareLocked();
                for (auto &axis : impl_->axes)
                {
                    axis.enabled  = false;
                    axis.homed    = false;
                    axis.homing   = false;
                    axis.moving   = false;
                    axis.velocity = 0.0;
                }
                impl_->homeDeadlines         = {};
                impl_->homeOriginSeen        = {};
                impl_->homeDirectionVerified = {};
                impl_->captureArmed          = false;
                impl_->status.operationId    = 0;
                impl_->status.lastError      = DmcError::Success;
                impl_->status.compareEnabled = false;
                impl_->syncStatus();
            }
            if (worker.joinable())
                worker.join();
            {
                std::scoped_lock lock(impl_->mutex, connectedCardsMutex);
                if (!stopResult.ok || !clearResult.ok)
                {
                    impl_->hardwareInitialized = true;
                    impl_->status.state        = DmcState::Failed;
                    impl_->status.lastError    = !stopResult.ok ? static_cast<DmcError>(stopResult.error) :
                                                                        static_cast<DmcError>(clearResult.error);
                }
                else if (!impl_->cardClaimed)
                {
                    impl_->hardwareInitialized = false;
                    impl_->status.state        = DmcState::Disconnected;
                }
                else if (boardUsers > 1)
                {
                    impl_->releaseCardClaimLocked();
                    impl_->hardwareInitialized = false;
                    impl_->status.state        = DmcState::Disconnected;
                }
                else if (!closeBoardIfUnusedLocked())
                {
                    closeResult                = failure<void>(DmcError::DriverUnavailable, "LTDMC board close failed.");
                    impl_->hardwareInitialized = true;
                    impl_->status.state        = DmcState::Failed;
                    impl_->status.lastError    = DmcError::DriverUnavailable;
                }
                else
                {
                    impl_->releaseCardClaimLocked();
                    impl_->hardwareInitialized = false;
                    impl_->status.state        = DmcState::Disconnected;
                }
                impl_->syncStatus();
            }
            if (!stopResult.ok)
                return stopResult;
            if (!clearResult.ok)
                return clearResult;
            if (!closeResult.ok)
                return closeResult;
            return Result<void>::Success();
        });
}

void LTDMCE3032Controller::setLogSink(LogSink sink)
{
    std::scoped_lock lock(impl_->mutex);
    impl_->logSink = std::move(sink);
}

void LTDMCE3032Controller::setInputEventSink(InputEventSink sink)
{
    std::scoped_lock lock(impl_->mutex);
    impl_->inputSink = std::move(sink);
}

auto LTDMCE3032Controller::status() const -> SZX::COMMON::Result<DmcStatus>
{
    return boundary(
        [this]() -> Result<DmcStatus>
        {
            std::scoped_lock lock(impl_->mutex);
            impl_->syncStatus();
            return Result<DmcStatus>::Success(impl_->status);
        });
}

auto LTDMCE3032Controller::profile() const -> SZX::COMMON::Result<DmcProfile>
{
    return boundary(
        [this]() -> Result<DmcProfile>
        {
            std::scoped_lock lock(impl_->mutex);
            return Result<DmcProfile>::Success(impl_->profile);
        });
}

auto LTDMCE3032Controller::enableAxis(AxisId axis) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, axis]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (auto result = impl_->validateAxis(axis); !result.ok)
                return result;
            if (nmc_set_axis_enable(impl_->sdkCard(), static_cast<WORD>(axis)) != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC axis enable failed.");
            impl_->axes[axis].enabled = true;
            impl_->syncStatus();
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::disableAxis(AxisId axis) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, axis]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (auto result = impl_->validateAxis(axis); !result.ok)
                return result;
            if (nmc_set_axis_disable(impl_->sdkCard(), static_cast<WORD>(axis)) != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC axis disable failed.");
            impl_->axes[axis].enabled = false;
            impl_->axes[axis].moving  = false;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::homeAxis(AxisId axis, const HomeProfile &profile) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, axis, &profile]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (auto result = impl_->validateAxis(axis); !result.ok)
                return result;
            if (!impl_->axes[axis].enabled)
                return failure<void>(DmcError::AxisNotReady, "Axis must be enabled before homing.");
            if (!validHomeProfile(profile))
                return failure<void>(DmcError::InvalidArgument, "Home profile is invalid.");
            WORD axisError = 0;
            WORD busError  = 0;
            if (nmc_get_axis_errcode(impl_->sdkCard(), static_cast<WORD>(axis), &axisError) != 0 ||
                nmc_get_errcode(impl_->sdkCard(), 2, &busError) != 0 || axisError != 0 || busError != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC axis or bus error prevents homing.");
            const auto sdkResult =
                nmc_set_home_profile_acc(impl_->sdkCard(), static_cast<WORD>(axis), profile.mode, profile.lowVelocity,
                                         profile.highVelocity, profile.acceleration, profile.deceleration, profile.offset);
            if (sdkResult != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC home profile configuration failed.");
            if (dmc_home_move(impl_->sdkCard(), static_cast<WORD>(axis)) != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC home motion failed.");
            impl_->axes[axis].homing   = true;
            impl_->axes[axis].moving   = true;
            impl_->axes[axis].homed    = false;
            impl_->homeDeadlines[axis] = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(static_cast<long long>(impl_->config.homeTimeoutSeconds * 1000.0));
            impl_->homeStartPositions[axis]    = impl_->axes[axis].position;
            impl_->homeLastPositions[axis]     = impl_->axes[axis].position;
            impl_->homeOriginSeen[axis]        = false;
            impl_->homeDirectionVerified[axis] = impl_->config.homeExpectedInitialDirection == 0;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::moveAbsolute(AxisId axis, double position, const MotionProfile &profile) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, axis, position, &profile]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (auto result = impl_->validateAxis(axis); !result.ok)
                return result;
            if (!impl_->axes[axis].enabled || !impl_->axes[axis].homed)
                return failure<void>(DmcError::AxisNotReady, "Axis must be enabled and homed before absolute motion.");
            if (!isFinite(position) || !validMotionProfile(profile))
                return failure<void>(DmcError::InvalidArgument, "Motion target or profile is invalid.");
            WORD axisError = 0;
            WORD busError  = 0;
            if (nmc_get_axis_errcode(impl_->sdkCard(), static_cast<WORD>(axis), &axisError) != 0 ||
                nmc_get_errcode(impl_->sdkCard(), 2, &busError) != 0 || axisError != 0 || busError != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC axis or bus error prevents motion.");
            if (dmc_set_profile_unit(impl_->sdkCard(), static_cast<WORD>(axis), profile.minVelocity, profile.maxVelocity,
                                     profile.acceleration, profile.deceleration, profile.stopVelocity) != 0 ||
                dmc_pmove_unit(impl_->sdkCard(), static_cast<WORD>(axis), position, 1) != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC absolute motion failed.");
            impl_->axes[axis].targetPosition = position;
            impl_->axes[axis].position       = position;
            impl_->axes[axis].velocity       = 0.0;
            impl_->axes[axis].moving         = false;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::moveRelative(AxisId axis, double distance, const MotionProfile &profile) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, axis, distance, &profile]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (auto result = impl_->validateAxis(axis); !result.ok)
                return result;
            if (!impl_->axes[axis].enabled)
                return failure<void>(DmcError::AxisNotReady, "Axis must be enabled before relative motion.");
            if (!isFinite(distance))
                return failure<void>(DmcError::InvalidArgument, "Relative motion distance is invalid.");
            if (!validMotionProfile(profile))
                return failure<void>(DmcError::InvalidArgument, "Motion profile is invalid.");
            const auto target = impl_->axes[axis].position + distance;
            WORD axisError    = 0;
            WORD busError     = 0;
            if (nmc_get_axis_errcode(impl_->sdkCard(), static_cast<WORD>(axis), &axisError) != 0 ||
                nmc_get_errcode(impl_->sdkCard(), 2, &busError) != 0 || axisError != 0 || busError != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC axis or bus error prevents motion.");
            if (dmc_set_profile_unit(impl_->sdkCard(), static_cast<WORD>(axis), profile.minVelocity, profile.maxVelocity,
                                     profile.acceleration, profile.deceleration, profile.stopVelocity) != 0 ||
                dmc_pmove_unit(impl_->sdkCard(), static_cast<WORD>(axis), distance, 0) != 0)
                return failure<void>(DmcError::AxisNotReady, "LTDMC relative motion failed.");
            impl_->axes[axis].targetPosition = target;
            impl_->axes[axis].position       = target;
            impl_->axes[axis].velocity       = 0.0;
            impl_->axes[axis].moving         = false;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::stop(AxisId axis, StopMode mode) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, axis, mode]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (auto result = impl_->validateAxis(axis, true); !result.ok)
                return result;
            impl_->motionAbortRequested.store(true, std::memory_order_release);
            impl_->axes[axis].moving   = false;
            impl_->axes[axis].velocity = 0.0;
            int stopResult = 0;
            {
                std::scoped_lock sdkLock(impl_->sdkMutex);
                stopResult = mode == StopMode::Emergency ? dmc_emg_stop(impl_->sdkCard()) :
                                                             dmc_stop(impl_->sdkCard(), static_cast<WORD>(axis), 0);
            }
            if (stopResult != 0)
                return failure<void>(DmcError::MotionTimeout, "LTDMC axis stop failed.");
            if (mode == StopMode::Emergency)
            {
                impl_->status.safetyLatched = true;
                impl_->status.state         = DmcState::SafetyLatched;
                impl_->status.lastError     = DmcError::SafetyLatched;
            }
            if (impl_->activeCapture())
            {
                const auto clear        = impl_->clearCompareLocked();
                impl_->captureArmed     = false;
                impl_->status.state     = mode == StopMode::Emergency ? DmcState::SafetyLatched : DmcState::Cancelled;
                impl_->status.lastError = mode == StopMode::Emergency ? DmcError::SafetyLatched : DmcError::Cancelled;
                if (!clear.ok)
                {
                    impl_->status.state     = DmcState::Failed;
                    impl_->status.lastError = static_cast<DmcError>(clear.error);
                    return clear;
                }
            }
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::emergencyStop() -> SZX::COMMON::Result<void>
{
    return boundary(
        [this]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
            return impl_->emergencyStopLocked();
        });
}

auto LTDMCE3032Controller::writeOutput(OutputId output, bool active) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, output, active]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
            if (output >= impl_->outputs.size())
                return failure<void>(DmcError::IoUnavailable, "Output is outside the configured IO range.");
            std::scoped_lock sdkLock(impl_->sdkMutex);
            if (dmc_write_outbit(impl_->sdkCard(), static_cast<WORD>(output), active ? 1 : 0) != 0)
                return failure<void>(DmcError::IoUnavailable, "LTDMC output write failed.");
            impl_->outputs[output] = active;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::readInputs() const -> SZX::COMMON::Result<InputSnapshot>
{
    return boundary(
        [this]() -> Result<InputSnapshot>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<InputSnapshot>(DmcError::NotConnected, "DMC controller is not connected.");
            auto input        = impl_->inputs;
            input.sampledBits = 0;
            for (InputId bit = 0; bit < impl_->config.inputCount; ++bit)
            {
                int value = -1;
                {
                    std::scoped_lock sdkLock(impl_->sdkMutex);
                    value = dmc_read_inbit(impl_->sdkCard(), static_cast<WORD>(bit));
                }
                if (value < 0)
                    return failure<InputSnapshot>(DmcError::IoUnavailable, "LTDMC input read failed.");
                if (value != 0)
                    input.sampledBits |= std::uint64_t{1} << bit;
            }
            input.activeBits           = input.sampledBits;
            input.valid                = true;
            input.timestampNanoseconds = nowNanoseconds();
            return Result<InputSnapshot>::Success(input);
        });
}

auto LTDMCE3032Controller::configureCompare(const CompareConfig &config) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, &config]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
            if (impl_->activeCapture())
                return failure<void>(DmcError::Busy, "Cannot reconfigure compare output during capture.");
            const auto capacity = config.queueCapacity == 0 ? impl_->config.compareQueueCapacity : config.queueCapacity;
            if (capacity == 0 || config.points.size() > capacity)
                return failure<void>(DmcError::CompareQueueFull, "Compare queue capacity is insufficient.");
            if (config.output >= impl_->config.outputCount)
                return failure<void>(DmcError::IoUnavailable, "Compare output is outside the configured IO range.");
            if (config.axis >= impl_->axes.size())
                return failure<void>(DmcError::InvalidArgument, "Compare axis is outside the controller range.");
            for (const auto &point : config.points)
                if (!isFinite(point.position))
                    return failure<void>(DmcError::InvalidArgument, "Compare point position is invalid.");
            if (dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(config.axis), 0, static_cast<WORD>(config.source)) !=
                0)
                return failure<void>(DmcError::CompareUnavailable, "LTDMC compare configuration failed.");
            if (dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(config.axis)) != 0)
                return failure<void>(DmcError::CompareUnavailable, "LTDMC compare queue clear failed.");
            long availableBefore = 0;
            if (dmc_compare_get_points_remained(impl_->sdkCard(), static_cast<WORD>(config.axis), &availableBefore) != 0)
                return failure<void>(DmcError::CompareUnavailable, "LTDMC compare queue capacity read failed.");
            for (std::size_t index = 0; index < config.points.size(); ++index)
            {
                const auto &point = config.points[index];
                if (dmc_compare_add_point_cycle_unit(impl_->sdkCard(), static_cast<WORD>(config.axis), point.position,
                                                     compareDirection(config.points, index), static_cast<DWORD>(config.output),
                                                     static_cast<DWORD>(config.pulseCycles), config.activeLevel ? 1 : 0) != 0)
                {
                    (void)dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(config.axis), 0,
                                                 static_cast<WORD>(config.source));
                    (void)dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(config.axis));
                    return failure<void>(DmcError::CompareQueueFull, "LTDMC compare point insertion failed.");
                }
            }
            if (auto readback = impl_->validateCompareQueueLocked(config.axis, config.source, false,
                                                                  static_cast<long>(config.points.size()), availableBefore);
                !readback.ok)
            {
                (void)dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(config.axis), 0,
                                             static_cast<WORD>(config.source));
                (void)dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(config.axis));
                return readback;
            }
            impl_->compare                      = config;
            impl_->compare.queueCapacity        = capacity;
            impl_->status.compareQueueRemaining = static_cast<std::uint32_t>(config.points.size());
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::enableCompare(bool enabled) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, enabled]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
            if (enabled && impl_->compare.queueCapacity == 0)
                return failure<void>(DmcError::CompareUnavailable, "Compare output has not been configured.");
            if (dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(impl_->compare.axis), enabled ? 1 : 0,
                                       static_cast<WORD>(impl_->compare.source)) != 0)
                return failure<void>(DmcError::CompareUnavailable, "LTDMC compare enable failed.");
            if (auto readback = impl_->validateCompareQueueLocked(impl_->compare.axis, impl_->compare.source, enabled);
                !readback.ok)
                return readback;
            impl_->status.compareEnabled = enabled;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::clearCompare() -> SZX::COMMON::Result<void>
{
    return boundary(
        [this]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
            return impl_->clearCompareLocked();
        });
}

auto LTDMCE3032Controller::compareTriggerCount() const -> SZX::COMMON::Result<std::uint32_t>
{
    return boundary(
        [this]() -> Result<std::uint32_t>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<std::uint32_t>(DmcError::NotConnected, "DMC controller is not connected.");
            long count = 0;
            if (dmc_compare_get_points_runned(impl_->sdkCard(), static_cast<WORD>(impl_->compare.axis), &count) != 0 || count < 0)
                return failure<std::uint32_t>(DmcError::CompareUnavailable, "LTDMC compare count read failed.");
            return Result<std::uint32_t>::Success(static_cast<std::uint32_t>(count));
        });
}

auto LTDMCE3032Controller::armCapture(const CaptureRequest &request) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, &request]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (impl_->status.safetyLatched)
                return failure<void>(DmcError::SafetyLatched, "DMC safety state is latched.");
            if (impl_->activeCapture())
                return failure<void>(DmcError::Busy, "Another DMC capture is active.");
            if (auto result = impl_->validateCaptureRequest(request); !result.ok)
                return result;
            impl_->request                      = request;
            impl_->captureArmed                 = true;
            impl_->status.operationId           = request.operationId;
            impl_->status.expectedFrames        = request.expectedFrames;
            impl_->status.comparedTriggers      = 0;
            impl_->status.receivedFrames        = 0;
            impl_->status.totalRows             = static_cast<std::uint32_t>(request.scanPlan.rowPositions.size());
            impl_->status.currentRow            = 0;
            impl_->status.lastError             = DmcError::Success;
            impl_->manualTriggerFrames          = request.triggerSource == FrameTriggerSource::PositionCompareOutput ?
                         static_cast<std::uint32_t>(request.scanPlan.rowPositions.size()) :
                         0;
            impl_->compareExpectedTriggers      = 0;
            impl_->status.compareQueueRemaining = 0;
            impl_->status.state                 = DmcState::Armed;
            if (request.triggerSource == FrameTriggerSource::PositionCompareOutput)
            {
                const auto columns = request.scanPlan.scanPositions.size();
                if (dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis), 0,
                                           static_cast<WORD>(request.compareSource)) != 0 ||
                    dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis)) != 0)
                {
                    impl_->captureArmed = false;
                    (void)dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis), 0,
                                                 static_cast<WORD>(request.compareSource));
                    (void)dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis));
                    impl_->status.state     = DmcState::Failed;
                    impl_->status.lastError = DmcError::CompareUnavailable;
                    return failure<void>(DmcError::CompareUnavailable, "LTDMC compare queue setup failed.");
                }
                long availableBefore = 0;
                if (dmc_compare_get_points_remained(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis),
                                                    &availableBefore) != 0)
                {
                    impl_->captureArmed = false;
                    (void)dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis));
                    impl_->status.state     = DmcState::Failed;
                    impl_->status.lastError = DmcError::CompareUnavailable;
                    return failure<void>(DmcError::CompareUnavailable, "LTDMC compare queue capacity read failed.");
                }
                for (std::size_t index = 0; index < request.comparePoints.size(); ++index)
                {
                    if (isFirstScanFrame(request.comparePoints, index, columns))
                        continue;
                    const auto &point = request.comparePoints[index];
                    ++impl_->compareExpectedTriggers;
                    if (dmc_compare_add_point_cycle_unit(
                            impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis), point.position,
                            compareDirection(request.comparePoints, index), static_cast<DWORD>(request.compareOutput),
                            static_cast<DWORD>(request.comparePulseCycles), request.compareOutputActiveLevel ? 1 : 0) != 0)
                    {
                        impl_->captureArmed = false;
                        (void)dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis), 0,
                                                     static_cast<WORD>(request.compareSource));
                        (void)dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis));
                        impl_->status.state     = DmcState::Failed;
                        impl_->status.lastError = DmcError::CompareQueueFull;
                        return failure<void>(DmcError::CompareQueueFull, "LTDMC compare point insertion failed.");
                    }
                }
                if (auto readback =
                        impl_->validateCompareQueueLocked(request.scanPlan.scanAxis, request.compareSource, false,
                                                          static_cast<long>(impl_->compareExpectedTriggers), availableBefore);
                    !readback.ok)
                {
                    impl_->captureArmed = false;
                    (void)dmc_compare_set_config(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis), 0,
                                                 static_cast<WORD>(request.compareSource));
                    (void)dmc_compare_clear_points(impl_->sdkCard(), static_cast<WORD>(request.scanPlan.scanAxis));
                    impl_->status.state     = DmcState::Failed;
                    impl_->status.lastError = DmcError::CompareUnavailable;
                    return readback;
                }
                CompareConfig compare;
                compare.axis        = request.scanPlan.scanAxis;
                compare.output      = request.compareOutput;
                compare.activeLevel = request.compareOutputActiveLevel;
                compare.source      = request.compareSource;
                compare.pulseCycles = request.comparePulseCycles;
                compare.queueCapacity =
                    request.compareQueueCapacity == 0 ? impl_->config.compareQueueCapacity : request.compareQueueCapacity;
                compare.points.reserve(impl_->compareExpectedTriggers);
                for (std::size_t index = 0; index < request.comparePoints.size(); ++index)
                    if (!isFirstScanFrame(request.comparePoints, index, columns))
                        compare.points.push_back(request.comparePoints[index]);
                impl_->compare                      = std::move(compare);
                impl_->status.compareQueueRemaining = impl_->compareExpectedTriggers;
            }
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::startCapture(OperationId operationId, StartSource source) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, operationId, source]() -> Result<void>
        {
            std::unique_lock lock(impl_->mutex);
            if (!impl_->captureArmed || impl_->status.operationId != operationId)
                return failure<void>(DmcError::InvalidArgument, "Capture operation is stale or unknown.");
            if (!hasStartSource(impl_->request.startSource, source))
                return failure<void>(DmcError::InvalidArgument, "The requested capture start source is not enabled.");
            return impl_->startCaptureLocked(lock);
        });
}

auto LTDMCE3032Controller::acknowledgeCompareTrigger(OperationId operationId, std::uint32_t count) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, operationId, count]() -> Result<void>
        {
            std::unique_lock lock(impl_->mutex);
            if (!impl_->captureArmed || impl_->status.operationId != operationId)
                return failure<void>(DmcError::InvalidArgument, "Capture operation is stale or unknown.");
            if (impl_->status.state != DmcState::Scanning && impl_->status.state != DmcState::WaitingForFrames)
                return failure<void>(DmcError::Busy, "Capture is not running.");
            if (count < impl_->status.comparedTriggers || count > impl_->compareExpectedTriggers)
                return failure<void>(DmcError::CompareQueueFull, "Compare trigger count is outside the capture range.");
            impl_->status.comparedTriggers      = count;
            impl_->status.compareQueueRemaining = impl_->compareExpectedTriggers - count;
            if (count == impl_->compareExpectedTriggers &&
                impl_->status.receivedFrames >= impl_->status.expectedFrames - impl_->manualTriggerFrames)
                impl_->status.state = DmcState::WaitingForFrames;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::acknowledgeFrameProgress(OperationId operationId, FrameOrdinal savedFrames)
    -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, operationId, savedFrames]() -> Result<void>
        {
            std::unique_lock lock(impl_->mutex);
            if (!impl_->captureArmed || impl_->status.operationId != operationId)
                return failure<void>(DmcError::InvalidArgument, "Capture operation is stale or unknown.");
            if (impl_->status.state != DmcState::Scanning && impl_->status.state != DmcState::WaitingForFrames)
                return failure<void>(DmcError::Busy, "Capture is not running.");
            if (savedFrames < impl_->status.receivedFrames || savedFrames > impl_->status.expectedFrames)
                return failure<void>(DmcError::FrameProgressInvalid, "Saved frame progress is outside the capture range.");
            if (impl_->request.triggerSource == FrameTriggerSource::PositionCompareOutput &&
                savedFrames > impl_->manualTriggerFrames + impl_->status.comparedTriggers)
                return failure<void>(DmcError::FrameProgressInvalid, "Camera progress exceeds confirmed compare triggers.");
            if (impl_->request.triggerSource == FrameTriggerSource::Software && savedFrames > impl_->status.receivedFrames)
                for (FrameOrdinal frame = impl_->status.receivedFrames; frame < savedFrames; ++frame)
                {
                    if (auto pulse =
                            impl_->pulseOutputLocked(impl_->request.compareOutput, impl_->request.compareOutputActiveLevel);
                        !pulse.ok)
                        return pulse;
                }
            impl_->status.receivedFrames = savedFrames;
            const auto columns           = impl_->request.scanPlan.scanPositions.size();
            const auto rows              = impl_->request.scanPlan.rowPositions.size();
            if (columns != 0 && rows != 0)
                impl_->status.currentRow =
                    std::min<std::uint32_t>(static_cast<std::uint32_t>((savedFrames == 0 ? 0 : savedFrames - 1) / columns),
                                            static_cast<std::uint32_t>(rows - 1));
            if (columns != 0 && rows != 0 && savedFrames != 0 && savedFrames % columns == 0 &&
                savedFrames < impl_->status.expectedFrames)
            {
                const auto nextRow = static_cast<std::size_t>(savedFrames / columns);
                if (auto done = impl_->waitAxisDone(lock, impl_->request.scanPlan.scanAxis); !done.ok)
                {
                    impl_->status.state     = DmcState::Failed;
                    impl_->status.lastError = static_cast<DmcError>(done.error);
                    return done;
                }
                if (auto motion = impl_->issueScanRowLocked(lock, nextRow); !motion.ok)
                {
                    impl_->status.state     = DmcState::Failed;
                    impl_->status.lastError = static_cast<DmcError>(motion.error);
                    return motion;
                }
                if (auto pulse = impl_->pulseOutputLocked(impl_->request.compareOutput, impl_->request.compareOutputActiveLevel);
                    !pulse.ok)
                    return pulse;
                if (auto sweep = impl_->issueScanSweepLocked(nextRow); !sweep.ok)
                {
                    impl_->status.state     = DmcState::Failed;
                    impl_->status.lastError = static_cast<DmcError>(sweep.error);
                    return sweep;
                }
            }
            if (savedFrames == impl_->status.expectedFrames &&
                (impl_->request.triggerSource == FrameTriggerSource::Software ||
                 impl_->manualTriggerFrames + impl_->status.comparedTriggers >= savedFrames))
                impl_->status.state = DmcState::WaitingForFrames;
            return Result<void>::Success();
        });
}

auto LTDMCE3032Controller::completeCamera(OperationId operationId, FrameOrdinal savedFrames) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, operationId, savedFrames]() -> Result<void>
        {
            std::unique_lock lock(impl_->mutex);
            if (!impl_->captureArmed || impl_->status.operationId != operationId)
                return failure<void>(DmcError::InvalidArgument, "Capture operation is stale or unknown.");
            if (impl_->status.state != DmcState::Scanning && impl_->status.state != DmcState::WaitingForFrames)
                return failure<void>(DmcError::Busy, "Capture is not running.");
            if (savedFrames != impl_->status.expectedFrames ||
                (impl_->request.triggerSource == FrameTriggerSource::PositionCompareOutput &&
                 impl_->manualTriggerFrames + impl_->status.comparedTriggers < savedFrames))
                return failure<void>(DmcError::FrameProgressInvalid, "Camera completion does not match the capture plan.");
            if (auto done = impl_->waitAxisDone(lock, impl_->request.scanPlan.scanAxis); !done.ok)
                return done;
            if (!impl_->captureArmed || impl_->status.operationId != operationId)
                return failure<void>(DmcError::Cancelled, "Capture completion was cancelled.");
            impl_->status.receivedFrames = savedFrames;
            const auto clear             = impl_->clearCompareLocked();
            impl_->captureArmed          = false;
            impl_->status.state          = DmcState::Completed;
            impl_->status.lastError      = DmcError::Success;
            return clear;
        });
}

auto LTDMCE3032Controller::failCapture(OperationId operationId, DmcError reason) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, operationId, reason]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->captureArmed || impl_->status.operationId != operationId)
                return failure<void>(DmcError::InvalidArgument, "Capture operation is stale or unknown.");
            if (reason == DmcError::Success)
                return failure<void>(DmcError::InvalidArgument, "Capture failure requires a non-success error.");
            const auto clear            = impl_->clearCompareLocked();
            impl_->captureArmed         = false;
            impl_->status.state         = reason == DmcError::SafetyLatched ? DmcState::SafetyLatched : DmcState::Failed;
            impl_->status.lastError     = reason;
            impl_->status.safetyLatched = reason == DmcError::SafetyLatched;
            return clear;
        });
}

auto LTDMCE3032Controller::cancelCapture(OperationId operationId, StopMode mode) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, operationId, mode]() -> Result<void>
        {
            std::scoped_lock lock(impl_->mutex);
            if (!impl_->captureArmed || impl_->status.operationId != operationId)
                return failure<void>(DmcError::InvalidArgument, "Capture operation is stale or unknown.");
            const auto stop             = impl_->stopAxesLocked(mode);
            const auto clear            = impl_->clearCompareLocked();
            impl_->captureArmed         = false;
            impl_->status.state         = !stop.ok || !clear.ok ?
                        DmcState::Failed :
                        (mode == StopMode::Emergency ? DmcState::SafetyLatched : DmcState::Cancelled);
            impl_->status.lastError     = !stop.ok ? static_cast<DmcError>(stop.error) :
                    !clear.ok                      ? static_cast<DmcError>(clear.error) :
                                                     (mode == StopMode::Emergency ? DmcError::SafetyLatched : DmcError::Cancelled);
            impl_->status.safetyLatched = mode == StopMode::Emergency;
            return !stop.ok ? stop : clear;
        });
}

auto LTDMCE3032Controller::pollCapture(OperationId operationId) const -> SZX::COMMON::Result<DmcStatus>
{
    return boundary(
        [this, operationId]() -> Result<DmcStatus>
        {
            std::scoped_lock lock(impl_->mutex);
            if (operationId != 0 && impl_->status.operationId != operationId)
                return failure<DmcStatus>(DmcError::InvalidArgument, "Capture operation is stale or unknown.");
            impl_->syncStatus();
            return Result<DmcStatus>::Success(impl_->status);
        });
}

auto LTDMCE3032Controller::signalDigitalInput(InputId input, bool active) -> SZX::COMMON::Result<void>
{
    return boundary(
        [this, input, active]() -> Result<void>
        {
            std::unique_lock lock(impl_->mutex);
            if (!impl_->connected())
                return failure<void>(DmcError::NotConnected, "DMC controller is not connected.");
            auto result = impl_->processInputSampleLocked(input, active);
            if (!result.ok)
                return failure<void>(static_cast<DmcError>(result.error), result.message);
            auto sink     = result.value ? impl_->inputSink : InputEventSink{};
            auto snapshot = impl_->inputs;
            lock.unlock();
            if (sink)
                try
                {
                    sink(snapshot);
                } catch (...)
                {
                    // Host callbacks must not cross the controller boundary.
                }
            return Result<void>::Success();
        });
}

} // namespace SZX::DMC::v1
