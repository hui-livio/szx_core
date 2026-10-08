#include <szx/dmc/szx_dmc_c.h>

#include <szx/dmc/DmcController.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_set>

using SZX::DMC::DmcConfig;
using SZX::DMC::DmcController;
using SZX::DMC::DmcControllerFactory;
using SZX::DMC::DmcError;
using SZX::DMC::DmcModel;
using SZX::DMC::DmcStatus;
using SZX::DMC::FrameTriggerSource;
using SZX::DMC::StartSource;
using SZX::DMC::StopMode;

struct szx_dmc_handle
{
    std::unique_ptr<DmcController> controller;
    std::string lastError;
    std::mutex mutex;
    std::condition_variable callsChanged;
    std::size_t activeCalls = 0;
    bool closing            = false;
    std::mutex errorMutex;
};

namespace
{
std::mutex liveHandlesMutex;
std::unordered_set<szx_dmc_handle *> liveHandles;

class HandleGuard
{
public:
    explicit HandleGuard(szx_dmc_handle *value) noexcept :
        handle_(value)
    {
        try
        {
            std::scoped_lock registryLock(liveHandlesMutex);
            if (handle_ == nullptr || !liveHandles.contains(handle_))
            {
                handle_ = nullptr;
                return;
            }
            std::unique_lock lock(handle_->mutex);
            if (handle_->closing)
            {
                handle_ = nullptr;
                return;
            }
            ++handle_->activeCalls;
        } catch (...)
        {
            handle_ = nullptr;
        }
    }

    explicit operator bool() const noexcept
    {
        return handle_ != nullptr;
    }
    auto get() const noexcept -> szx_dmc_handle *
    {
        return handle_;
    }

    ~HandleGuard() noexcept
    {
        if (handle_ == nullptr)
            return;
        std::scoped_lock lock(handle_->mutex);
        if (handle_->activeCalls > 0)
            --handle_->activeCalls;
        handle_->callsChanged.notify_all();
    }

private:
    szx_dmc_handle *handle_ = nullptr;
};

auto resultCode(int code) noexcept -> int32_t
{
    return static_cast<int32_t>(code);
}

auto errorCode(DmcError error) noexcept -> int32_t
{
    return static_cast<int32_t>(error);
}

auto boundedString(const char *value, std::size_t capacity) -> std::string
{
    if (value == nullptr)
        return {};
    std::size_t length = 0;
    while (length < capacity && value[length] != '\0')
        ++length;
    return std::string(value, length);
}

auto invalidArgumentLocked(szx_dmc_handle *handle, const char *message) noexcept -> int32_t
{
    try
    {
        if (handle != nullptr)
        {
            std::scoped_lock lock(handle->errorMutex);
            handle->lastError = message == nullptr ? "Invalid argument." : message;
        }
    } catch (...)
    {
    }
    return errorCode(DmcError::InvalidArgument);
}

auto clearLastErrorLocked(szx_dmc_handle *handle) noexcept -> void
{
    try
    {
        if (handle != nullptr)
        {
            std::scoped_lock lock(handle->errorMutex);
            handle->lastError.clear();
        }
    } catch (...)
    {
    }
}

auto setLastErrorLocked(szx_dmc_handle *handle, const std::string &message) noexcept -> void
{
    try
    {
        if (handle != nullptr)
        {
            std::scoped_lock lock(handle->errorMutex);
            handle->lastError = message;
        }
    } catch (...)
    {
    }
}

auto invalidArgument(szx_dmc_handle *handle, const char *message) noexcept -> int32_t
{
    HandleGuard guard(handle);
    return guard ? invalidArgumentLocked(handle, message) : errorCode(DmcError::InvalidArgument);
}

template <typename Result>
auto consume(szx_dmc_handle *handle, const Result &result) noexcept -> int32_t
{
    if (result.ok)
    {
        clearLastErrorLocked(handle);
        return static_cast<int32_t>(DmcError::Success);
    }
    setLastErrorLocked(handle, result.message);
    return resultCode(result.error);
}

template <typename Function>
auto invoke(szx_dmc_handle *handle, Function &&function) noexcept -> int32_t
{
    HandleGuard guard(handle);
    if (!guard)
        return errorCode(DmcError::InvalidArgument);
    try
    {
        const auto result = function();
        return consume(handle, result);
    } catch (const std::bad_alloc &)
    {
        return errorCode(DmcError::ResourceExhausted);
    } catch (const std::exception &error)
    {
        setLastErrorLocked(handle, error.what());
        return errorCode(DmcError::UnexpectedFailure);
    } catch (...)
    {
        return errorCode(DmcError::UnexpectedFailure);
    }
}

auto copyDiagnostic(const std::string &diagnostic, char *buffer, std::size_t capacity, std::size_t *requiredSize) noexcept
    -> int32_t
{
    const auto required = diagnostic.size() + 1;
    if (requiredSize != nullptr)
        *requiredSize = required;
    if (buffer == nullptr || capacity < required)
        return errorCode(DmcError::BufferTooSmall);
    std::memcpy(buffer, diagnostic.c_str(), required);
    return errorCode(DmcError::Success);
}

auto fillState(const DmcStatus &status, szx_dmc_state_v1 *output) noexcept -> void
{
    const auto size    = output->struct_size;
    const auto version = output->abi_version;
    std::memset(output, 0, sizeof(*output));
    output->struct_size                           = size;
    output->abi_version                           = version;
    output->state                                 = static_cast<std::uint16_t>(status.state);
    output->last_error                            = static_cast<std::uint16_t>(status.lastError);
    output->operation_id                          = status.operationId;
    output->current_row                           = status.currentRow;
    output->total_rows                            = status.totalRows;
    output->compared_triggers                     = status.comparedTriggers;
    output->received_frames                       = status.receivedFrames;
    output->expected_frames                       = status.expectedFrames;
    output->start_edge_sequence                   = 0;
    output->last_start_edge_timestamp_nanoseconds = 0;
    output->compare_queue_remaining               = status.compareQueueRemaining;
    output->safety_latched                        = status.safetyLatched ? 1 : 0;
    output->compare_enabled                       = status.compareEnabled ? 1 : 0;
    output->input_valid                           = status.inputValid ? 1 : 0;
    output->hardware                              = status.hardware ? 1 : 0;
    output->model                                 = static_cast<std::uint16_t>(status.model);
    const auto copyLength                         = std::min(status.diagnostic.size(), sizeof(output->diagnostic) - 1);
    std::memcpy(output->diagnostic, status.diagnostic.data(), copyLength);
    output->diagnostic[copyLength] = '\0';
}

auto requiredSize(std::size_t expected, std::uint32_t actual, szx_dmc_handle *handle) noexcept -> int32_t
{
    if (actual < expected)
        return invalidArgumentLocked(handle, "ABI structure is smaller than the v1 contract.");
    return errorCode(DmcError::Success);
}

} // namespace

extern "C" {

auto szx_dmc_abi_version_v1() noexcept -> int32_t
{
    return SZX_DMC_ABI_VERSION_V1;
}

auto szx_dmc_create_v1(const szx_dmc_config_v1 *config, szx_dmc_handle **outHandle) noexcept -> int32_t
{
    try
    {
        if (outHandle == nullptr)
            return errorCode(DmcError::InvalidArgument);
        *outHandle = nullptr;
        if (config == nullptr || config->abi_version != SZX_DMC_ABI_VERSION_V1 || config->struct_size < sizeof(szx_dmc_config_v1))
            return errorCode(DmcError::InvalidArgument);
        if (config->compare_queue_capacity == 0 || config->input_count > 64 || config->output_count > 64)
            return errorCode(DmcError::InvalidArgument);

        DmcConfig cppConfig;
        if (config->model != SZX_DMC_MODEL_LEISAI_LTDMC && config->model != SZX_DMC_MODEL_CUSTOM)
            return errorCode(DmcError::InvalidArgument);
        cppConfig.model     = config->model == SZX_DMC_MODEL_CUSTOM ? DmcModel::Custom : DmcModel::LeiSaiLTDMC;
        cppConfig.profileId = boundedString(config->profile_id, sizeof(config->profile_id));
        if (cppConfig.profileId.empty())
            cppConfig.profileId = "E3032";
        cppConfig.cardIndex               = config->card_index;
        cppConfig.simulation              = config->simulation != 0;
        cppConfig.allowSimulationFallback = config->allow_simulation_fallback != 0;
        cppConfig.compareQueueCapacity    = config->compare_queue_capacity;
        cppConfig.inputCount              = config->input_count;
        cppConfig.outputCount             = config->output_count;
        if (config->log_callback != nullptr)
        {
            const auto callback = config->log_callback;
            const auto userData = config->log_user_data;
            cppConfig.logSink   = [callback, userData](const SZX::DMC::DmcLogLevel level, const std::string &message)
            {
                callback(static_cast<uint8_t>(level), message.c_str(), userData);
            };
        }
        auto result = DmcControllerFactory::create(cppConfig);
        if (!result.ok)
            return resultCode(result.error);

        auto handle        = std::make_unique<szx_dmc_handle>();
        handle->controller = std::move(result.value);
        auto *rawHandle    = handle.get();
        {
            std::scoped_lock lock(liveHandlesMutex);
            liveHandles.insert(rawHandle);
        }
        *outHandle = handle.release();
        return errorCode(DmcError::Success);
    } catch (const std::bad_alloc &)
    {
        return errorCode(DmcError::ResourceExhausted);
    } catch (...)
    {
        return errorCode(DmcError::UnexpectedFailure);
    }
}

auto szx_dmc_destroy_v1(szx_dmc_handle *handle) noexcept -> int32_t
{
    try
    {
        {
            std::scoped_lock registryLock(liveHandlesMutex);
            if (handle == nullptr || !liveHandles.contains(handle))
                return errorCode(DmcError::InvalidArgument);
            std::unique_lock operationLock(handle->mutex);
            if (handle->activeCalls != 0)
                return errorCode(DmcError::Busy);
            handle->closing = true;
            liveHandles.erase(handle);
        }
        delete handle;
        return errorCode(DmcError::Success);
    } catch (...)
    {
        return errorCode(DmcError::UnexpectedFailure);
    }
}

auto szx_dmc_connect_v1(szx_dmc_handle *handle) noexcept -> int32_t
{
    return invoke(handle, [&] { return handle->controller->connect(); });
}

auto szx_dmc_disconnect_v1(szx_dmc_handle *handle) noexcept -> int32_t
{
    return invoke(handle, [&] { return handle->controller->disconnect(); });
}

auto szx_dmc_poll_state_v1(szx_dmc_handle *handle, szx_dmc_state_v1 *state) noexcept -> int32_t
{
    HandleGuard guard(handle);
    if (!guard)
        return invalidArgument(handle, "Handle and state output are required.");
    if (state == nullptr)
        return invalidArgumentLocked(handle, "Handle and state output are required.");
    if (requiredSize(sizeof(szx_dmc_state_v1), state->struct_size, handle) != 0 || state->abi_version != SZX_DMC_ABI_VERSION_V1)
        return invalidArgumentLocked(handle, "State ABI version or size is invalid.");
    SZX::COMMON::Result<DmcStatus> result;
    try
    {
        result = handle->controller->status();
    } catch (...)
    {
        return errorCode(DmcError::UnexpectedFailure);
    }
    if (!result.ok)
        return consume(handle, result);
    fillState(result.value, state);
    clearLastErrorLocked(handle);
    return errorCode(DmcError::Success);
}

auto szx_dmc_execute_v1(szx_dmc_handle *handle, const szx_dmc_command_v1 *command) noexcept -> int32_t
{
    if (command == nullptr)
        return invalidArgument(handle, "Handle and command are required.");
    if (command->struct_size < sizeof(szx_dmc_command_v1) || command->abi_version != SZX_DMC_ABI_VERSION_V1)
        return invalidArgument(handle, "Command ABI version or size is invalid.");
    const auto profile =
        SZX::DMC::MotionProfile{command->min_velocity, command->max_velocity, command->acceleration, command->deceleration, 0.0};
    switch (command->action)
    {
    case 0:
        return invoke(handle, [&] { return handle->controller->enableAxis(command->axis); });
    case 1:
        return invoke(handle, [&] { return handle->controller->disableAxis(command->axis); });
    case 2:
        return invoke(handle, [&] { return handle->controller->homeAxis(command->axis); });
    case 3:
        return invoke(handle, [&] { return handle->controller->moveAbsolute(command->axis, command->value, profile); });
    case 4:
        return invoke(handle, [&] { return handle->controller->moveRelative(command->axis, command->value, profile); });
    case 5:
        return invoke(handle, [&] { return handle->controller->stop(command->axis, StopMode::Decelerated); });
    case 6:
        return invoke(handle, [&] { return handle->controller->emergencyStop(); });
    default:
        return invalidArgument(handle, "Unknown DMC command action.");
    }
}

auto szx_dmc_arm_capture_v1(szx_dmc_handle *handle, const szx_dmc_capture_v1 *capture) noexcept -> int32_t
{
    if (capture == nullptr)
        return invalidArgument(handle, "Handle and capture request are required.");
    if (capture->struct_size < sizeof(szx_dmc_capture_v1) || capture->abi_version != SZX_DMC_ABI_VERSION_V1)
        return invalidArgument(handle, "Capture ABI version or size is invalid.");
    SZX::DMC::CaptureRequest request;
    request.operationId = capture->operation_id;
    request.startSource = static_cast<StartSource>(capture->start_source);
    if (capture->trigger_source > SZX_DMC_FRAME_TRIGGER_POSITION_COMPARE_OUTPUT_V1)
        return invalidArgument(handle, "Capture trigger source is invalid.");
    request.triggerSource                      = static_cast<FrameTriggerSource>(capture->trigger_source);
    request.compareOutput                      = capture->compare_output;
    request.compareOutputActiveLevel           = capture->compare_output_active_level != 0;
    request.expectedFrames                     = capture->expected_frames;
    request.comparePulseCycles                 = capture->compare_pulse_cycles;
    request.compareQueueCapacity               = capture->compare_queue_capacity;
    request.scanPlan.scanAxis                  = capture->scan_axis;
    request.scanPlan.rowAxis                   = capture->row_axis;
    request.scanPlan.scanStartPosition         = capture->scan_start_position;
    request.scanPlan.scanEndPosition           = capture->scan_end_position;
    request.scanPlan.scanSpeed                 = capture->scan_speed;
    request.scanPlan.rowSpeed                  = capture->row_speed;
    constexpr std::uint32_t maxCaptureElements = 1U << 20;
    if (capture->compare_point_count > maxCaptureElements || capture->scan_position_count > maxCaptureElements ||
        capture->row_position_count > maxCaptureElements)
        return invalidArgument(handle, "Capture array is too large.");
    if (capture->compare_point_count > capture->expected_frames ||
        capture->scan_position_count != 0 && capture->row_position_count != 0 &&
            capture->scan_position_count > capture->expected_frames / capture->row_position_count)
        return invalidArgument(handle, "Capture array dimensions are invalid.");
    if (capture->compare_point_count > 0 && capture->compare_points == nullptr)
        return invalidArgument(handle, "Compare point buffer is required.");
    if (capture->scan_position_count > 0 && capture->scan_positions == nullptr)
        return invalidArgument(handle, "Scan position buffer is required.");
    if (capture->row_position_count > 0 && capture->row_positions == nullptr)
        return invalidArgument(handle, "Row position buffer is required.");
    return invoke(handle,
                  [&]
                  {
                      request.comparePoints.reserve(capture->compare_point_count);
                      for (std::uint32_t index = 0; index < capture->compare_point_count; ++index)
                      {
                          const auto &point = capture->compare_points[index];
                          request.comparePoints.push_back({point.position, point.ordinal, point.row, point.column});
                      }
                      if (capture->scan_position_count != 0)
                          request.scanPlan.scanPositions.assign(capture->scan_positions,
                                                                capture->scan_positions + capture->scan_position_count);
                      if (capture->row_position_count != 0)
                          request.scanPlan.rowPositions.assign(capture->row_positions,
                                                               capture->row_positions + capture->row_position_count);
                      return handle->controller->armCapture(request);
                  });
}

auto szx_dmc_start_capture_v1(szx_dmc_handle *handle, uint64_t operationId) noexcept -> int32_t
{
    return invoke(handle, [&] { return handle->controller->startCapture(operationId); });
}

auto szx_dmc_start_capture_source_v1(szx_dmc_handle *handle, uint64_t operationId, uint16_t source) noexcept -> int32_t
{
    return invoke(handle, [&] { return handle->controller->startCapture(operationId, static_cast<StartSource>(source)); });
}

auto szx_dmc_cancel_capture_v1(szx_dmc_handle *handle, uint8_t emergency) noexcept -> int32_t
{
    HandleGuard guard(handle);
    if (!guard)
        return errorCode(DmcError::InvalidArgument);
    SZX::COMMON::Result<DmcStatus> state;
    try
    {
        state = handle->controller->status();
    } catch (...)
    {
        return errorCode(DmcError::UnexpectedFailure);
    }
    if (!state.ok)
        return consume(handle, state);
    try
    {
        return consume(handle,
                       handle->controller->cancelCapture(state.value.operationId,
                                                         emergency != 0 ? StopMode::Emergency : StopMode::Decelerated));
    } catch (const std::bad_alloc &)
    {
        return errorCode(DmcError::ResourceExhausted);
    } catch (const std::exception &error)
    {
        setLastErrorLocked(handle, error.what());
        return errorCode(DmcError::UnexpectedFailure);
    } catch (...)
    {
        return errorCode(DmcError::UnexpectedFailure);
    }
}

auto szx_dmc_last_error_v1(szx_dmc_handle *handle, char *buffer, size_t capacity, size_t *required) noexcept -> int32_t
{
    HandleGuard guard(handle);
    if (!guard)
        return errorCode(DmcError::InvalidArgument);
    std::scoped_lock lock(handle->errorMutex);
    return copyDiagnostic(handle->lastError, buffer, capacity, required);
}

} // extern "C"
