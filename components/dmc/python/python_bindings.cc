#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <szx/dmc/DmcController.hpp>

namespace py = pybind11;
using namespace SZX::DMC::v1;

namespace
{
template <typename Result>
void requireOk(const Result &result)
{
    if (!result.ok)
        throw std::runtime_error(result.message);
}
} // namespace

PYBIND11_MODULE(szx_dmc_py, module)
{
    module.doc() = "SZX DMC motion-control API";

    py::enum_<DmcError>(module, "DmcError")
        .value("Success", DmcError::Success)
        .value("InvalidArgument", DmcError::InvalidArgument)
        .value("DriverUnavailable", DmcError::DriverUnavailable)
        .value("ArchitectureMismatch", DmcError::ArchitectureMismatch)
        .value("SymbolUnavailable", DmcError::SymbolUnavailable)
        .value("BoardNotFound", DmcError::BoardNotFound)
        .value("NotConnected", DmcError::NotConnected)
        .value("Busy", DmcError::Busy)
        .value("SafetyLatched", DmcError::SafetyLatched)
        .value("AxisNotReady", DmcError::AxisNotReady)
        .value("NotHomed", DmcError::NotHomed)
        .value("LimitReached", DmcError::LimitReached)
        .value("MotionTimeout", DmcError::MotionTimeout)
        .value("IoUnavailable", DmcError::IoUnavailable)
        .value("CompareUnavailable", DmcError::CompareUnavailable)
        .value("CompareQueueFull", DmcError::CompareQueueFull)
        .value("TriggerTimeout", DmcError::TriggerTimeout)
        .value("FrameProgressInvalid", DmcError::FrameProgressInvalid)
        .value("Cancelled", DmcError::Cancelled)
        .value("ResourceExhausted", DmcError::ResourceExhausted)
        .value("UnexpectedFailure", DmcError::UnexpectedFailure)
        .value("BufferTooSmall", DmcError::BufferTooSmall)
        .export_values();
    py::enum_<DmcState>(module, "DmcState")
        .value("Disconnected", DmcState::Disconnected)
        .value("Ready", DmcState::Ready)
        .value("Armed", DmcState::Armed)
        .value("WaitingForStart", DmcState::WaitingForStart)
        .value("Preparing", DmcState::Preparing)
        .value("Scanning", DmcState::Scanning)
        .value("WaitingForFrames", DmcState::WaitingForFrames)
        .value("Completed", DmcState::Completed)
        .value("Cancelled", DmcState::Cancelled)
        .value("Failed", DmcState::Failed)
        .value("SafetyLatched", DmcState::SafetyLatched);
    py::enum_<StartSource>(module, "StartSource")
        .value("None", StartSource::None)
        .value("SoftwareCommand", StartSource::SoftwareCommand)
        .value("HardwareTrigger", StartSource::HardwareTrigger)
        .value("Any", StartSource::Any);
    py::enum_<FrameTriggerSource>(module, "FrameTriggerSource")
        .value("Software", FrameTriggerSource::Software)
        .value("PositionCompareOutput", FrameTriggerSource::PositionCompareOutput);
    py::enum_<StopMode>(module, "StopMode").value("Decelerated", StopMode::Decelerated).value("Emergency", StopMode::Emergency);
    py::enum_<DmcModel>(module, "DmcModel")
        .value("GenericLTDMC", DmcModel::GenericLTDMC)
        .value("LeiSaiLTDMC", DmcModel::LeiSaiLTDMC)
        .value("Custom", DmcModel::Custom);
    py::enum_<DmcLogLevel>(module, "DmcLogLevel")
        .value("Debug", DmcLogLevel::Debug)
        .value("Trace", DmcLogLevel::Trace)
        .value("Info", DmcLogLevel::Info)
        .value("Warning", DmcLogLevel::Warning)
        .value("Error", DmcLogLevel::Error);
    py::class_<DmcConfig>(module, "DmcConfig")
        .def(py::init<>())
        .def_readwrite("model", &DmcConfig::model)
        .def_readwrite("profile_id", &DmcConfig::profileId)
        .def_readwrite("card_index", &DmcConfig::cardIndex)
        .def_readwrite("simulation", &DmcConfig::simulation)
        .def_readwrite("allow_simulation_fallback", &DmcConfig::allowSimulationFallback)
        .def_readwrite("compare_queue_capacity", &DmcConfig::compareQueueCapacity)
        .def_readwrite("input_count", &DmcConfig::inputCount)
        .def_readwrite("output_count", &DmcConfig::outputCount)
        .def_readwrite("log_sink", &DmcConfig::logSink);
    py::class_<InputSnapshot>(module, "InputSnapshot")
        .def_readonly("active_bits", &InputSnapshot::activeBits)
        .def_readonly("sampled_bits", &InputSnapshot::sampledBits)
        .def_readonly("timestamp_nanoseconds", &InputSnapshot::timestampNanoseconds)
        .def_readonly("valid", &InputSnapshot::valid);
    py::class_<DmcStatus>(module, "DmcStatus")
        .def_readonly("state", &DmcStatus::state)
        .def_readonly("last_error", &DmcStatus::lastError)
        .def_readonly("operation_id", &DmcStatus::operationId)
        .def_readonly("current_row", &DmcStatus::currentRow)
        .def_readonly("total_rows", &DmcStatus::totalRows)
        .def_readonly("compared_triggers", &DmcStatus::comparedTriggers)
        .def_readonly("received_frames", &DmcStatus::receivedFrames)
        .def_readonly("expected_frames", &DmcStatus::expectedFrames)
        .def_readonly("compare_queue_remaining", &DmcStatus::compareQueueRemaining)
        .def_readonly("safety_latched", &DmcStatus::safetyLatched)
        .def_readonly("compare_enabled", &DmcStatus::compareEnabled)
        .def_readonly("model", &DmcStatus::model)
        .def_readonly("profile_id", &DmcStatus::profileId)
        .def_readonly("diagnostic", &DmcStatus::diagnostic);
    py::class_<ComparePoint>(module, "ComparePoint")
        .def(py::init<>())
        .def_readwrite("position", &ComparePoint::position)
        .def_readwrite("ordinal", &ComparePoint::ordinal)
        .def_readwrite("row", &ComparePoint::row)
        .def_readwrite("column", &ComparePoint::column);
    py::class_<ScanPlan>(module, "ScanPlan")
        .def(py::init<>())
        .def_readwrite("scan_axis", &ScanPlan::scanAxis)
        .def_readwrite("row_axis", &ScanPlan::rowAxis)
        .def_readwrite("scan_positions", &ScanPlan::scanPositions)
        .def_readwrite("row_positions", &ScanPlan::rowPositions)
        .def_readwrite("scan_start_position", &ScanPlan::scanStartPosition)
        .def_readwrite("scan_end_position", &ScanPlan::scanEndPosition)
        .def_readwrite("scan_speed", &ScanPlan::scanSpeed)
        .def_readwrite("row_speed", &ScanPlan::rowSpeed);
    py::class_<CaptureRequest>(module, "CaptureRequest")
        .def(py::init<>())
        .def_readwrite("operation_id", &CaptureRequest::operationId)
        .def_readwrite("start_source", &CaptureRequest::startSource)
        .def_readwrite("trigger_source", &CaptureRequest::triggerSource)
        .def_readwrite("expected_frames", &CaptureRequest::expectedFrames)
        .def_readwrite("compare_output", &CaptureRequest::compareOutput)
        .def_readwrite("compare_output_active_level", &CaptureRequest::compareOutputActiveLevel)
        .def_readwrite("compare_pulse_cycles", &CaptureRequest::comparePulseCycles)
        .def_readwrite("compare_queue_capacity", &CaptureRequest::compareQueueCapacity)
        .def_readwrite("compare_points", &CaptureRequest::comparePoints)
        .def_readwrite("scan_plan", &CaptureRequest::scanPlan);
    py::class_<DmcControllerFactory>(module, "DmcControllerFactory")
        .def_static(
            "create",
            [](const DmcConfig &config)
            {
                auto result = DmcControllerFactory::create(config);
                requireOk(result);
                return std::move(result.value);
            },
            py::arg("config") = DmcConfig{});
    py::class_<DmcController, std::unique_ptr<DmcController>>(module, "DmcController")
        .def("connect", [](DmcController &controller) { requireOk(controller.connect()); })
        .def("disconnect", [](DmcController &controller) { requireOk(controller.disconnect()); })
        .def(
            "set_input_event_sink",
            [](DmcController &controller, py::object sink)
            {
                if (sink.is_none())
                {
                    controller.setInputEventSink({});
                    return;
                }
                auto callback = sink.cast<py::function>();
                controller.setInputEventSink(
                    [callback = std::move(callback)](const InputSnapshot &snapshot)
                    {
                        py::gil_scoped_acquire gil;
                        try
                        {
                            callback(snapshot);
                        } catch (...)
                        {
                            // Python callback failures must not terminate input processing.
                        }
                    });
            },
            py::arg("sink") = py::none())
        .def("status",
             [](DmcController &controller)
             {
                 auto result = controller.status();
                 requireOk(result);
                 return result.value;
             })
        .def("arm_capture",
             [](DmcController &controller, const CaptureRequest &request) { requireOk(controller.armCapture(request)); })
        .def(
            "start_capture", [](DmcController &controller, OperationId operationId, StartSource source)
            { requireOk(controller.startCapture(operationId, source)); }, py::arg("operation_id"),
            py::arg("source") = StartSource::SoftwareCommand)
        .def("acknowledge_compare_trigger", [](DmcController &controller, OperationId operationId, std::uint32_t count)
             { requireOk(controller.acknowledgeCompareTrigger(operationId, count)); })
        .def("acknowledge_frame_progress", [](DmcController &controller, OperationId operationId, FrameOrdinal frames)
             { requireOk(controller.acknowledgeFrameProgress(operationId, frames)); })
        .def("complete_camera", [](DmcController &controller, OperationId operationId, FrameOrdinal frames)
             { requireOk(controller.completeCamera(operationId, frames)); })
        .def(
            "cancel_capture", [](DmcController &controller, OperationId operationId, StopMode mode)
            { requireOk(controller.cancelCapture(operationId, mode)); }, py::arg("operation_id"),
            py::arg("mode") = StopMode::Decelerated)
        .def("signal_digital_input", [](DmcController &controller, InputId input, bool active)
             { requireOk(controller.signalDigitalInput(input, active)); });
}
