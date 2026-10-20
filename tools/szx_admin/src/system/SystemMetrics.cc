#include "SystemMetrics.h"

#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxMetrics/Metrics.h"

#include <limits>
#include <memory>
#include <utility>

namespace SZX::ADMIN::SYSTEM
{
inline namespace v1
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;

template <typename T>
auto failure(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

auto metricsError(std::int32_t code) noexcept -> ErrorCode
{
    switch (code)
    {
    case SZX_METRICS_OUT_OF_RANGE:
        return ErrorCode::ValidationError;
    case SZX_METRICS_INVALID_CONFIG:
        return ErrorCode::InvalidConfiguration;
    case SZX_METRICS_NOT_RUNNING:
    case SZX_METRICS_PLATFORM_ERROR:
    case SZX_METRICS_OUT_OF_MEMORY:
        return ErrorCode::ServiceUnavailable;
    default:
        return ErrorCode::InternalFailure;
    }
}

auto metricValue(const szx_metrics_metric &raw) -> Metric
{
    Metric result;
    switch (raw.state)
    {
    case SZX_METRICS_METRIC_OK:
        result.status = "ok";
        break;
    case SZX_METRICS_METRIC_WARMING_UP:
        result.status = "warming_up";
        break;
    case SZX_METRICS_METRIC_STOPPED:
        result.status = "stopped";
        break;
    default:
        return result;
    }
    if (raw.present & SZX_METRICS_FIELD_USAGE)
        result.usagePercent = raw.usage_percent;
    if (raw.present & SZX_METRICS_FIELD_TOTAL)
        result.totalBytes = raw.total_bytes;
    if (raw.present & SZX_METRICS_FIELD_USED)
        result.usedBytes = raw.used_bytes;
    if (raw.present & SZX_METRICS_FIELD_AVAILABLE)
        result.availableBytes = raw.available_bytes;
    if (raw.present & SZX_METRICS_FIELD_FREE)
        result.freeBytes = raw.free_bytes;
    return result;
}

auto metricDto(const szx_metrics_metric &raw) -> MetricDto
{
    const auto value     = metricValue(raw);
    const bool available = raw.state == SZX_METRICS_METRIC_OK || raw.state == SZX_METRICS_METRIC_WARMING_UP ||
        raw.state == SZX_METRICS_METRIC_STOPPED;
    const auto error = available ? std::nullopt : std::optional<int>{static_cast<int>(ErrorCode::ServiceUnavailable)};
    return {value.status,
            error,
            value.usagePercent,
            value.totalBytes,
            value.usedBytes,
            value.availableBytes,
            value.freeBytes,
            (raw.present & SZX_METRICS_FIELD_TEMPERATURE) ? std::optional<double>{raw.temperature_celsius} : std::nullopt};
}

auto gpuEngineDto(const szx_metrics_gpu_extra &raw) -> GpuEngineDto
{
    szx_metrics_metric state{};
    state.state       = raw.state;
    const auto metric = metricDto(state);
    return {metric.status, metric.error_code, (raw.present & 1U) ? std::optional<double>{raw.total_percent} : std::nullopt,
            (raw.present & 2U) ? std::optional<double>{raw.cuda_percent} : std::nullopt,
            (raw.present & 4U) ? std::optional<double>{raw.graphics_3d_percent} : std::nullopt};
}

auto sizedMetric() -> szx_metrics_metric
{
    szx_metrics_metric result{};
    result.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_metric);
    return result;
}

using Snapshot = std::unique_ptr<szx_metrics_snapshot, decltype(&szx_metrics_snapshot_release)>;
using History  = std::unique_ptr<szx_metrics_history, decltype(&szx_metrics_history_release)>;
} // namespace

auto parseCpuCounters(std::string_view text) -> std::optional<CpuCounters>
{
    if (text.empty() || text.size() > std::numeric_limits<std::uint32_t>::max())
        return std::nullopt;
    szx_metrics_cpu_counters raw{};
    raw.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_cpu_counters);
    if (szx_metrics_parse_cpu_counters(text.data(), static_cast<std::uint32_t>(text.size()), &raw) != SZX_METRICS_OK)
        return std::nullopt;
    return CpuCounters{raw.total, raw.idle};
}

auto cpuUsage(CpuCounters previous, CpuCounters current) noexcept -> std::optional<double>
{
    szx_metrics_cpu_counters before{SZX_METRICS_SIZE_OF(szx_metrics_cpu_counters), 0, previous.total, previous.idle};
    szx_metrics_cpu_counters after{SZX_METRICS_SIZE_OF(szx_metrics_cpu_counters), 0, current.total, current.idle};
    double percent = 0.0;
    if (szx_metrics_cpu_usage(&before, &after, &percent) != SZX_METRICS_OK)
        return std::nullopt;
    return percent;
}

auto parseMemory(std::string_view text) -> Metric
{
    if (text.empty() || text.size() > std::numeric_limits<std::uint32_t>::max())
        return {};
    auto raw = sizedMetric();
    if (szx_metrics_parse_memory(text.data(), static_cast<std::uint32_t>(text.size()), &raw) != SZX_METRICS_OK)
        return {};
    return metricValue(raw);
}

auto capacityMetric(std::uint64_t total, std::uint64_t free, std::uint64_t available) -> Metric
{
    auto raw = sizedMetric();
    if (szx_metrics_capacity_metric(total, free, available, &raw) != SZX_METRICS_OK)
        return {};
    return metricValue(raw);
}

void SystemMetricsService::MetricsServiceDeleter::operator()(szx_metrics_service *service) const noexcept
{
    szx_metrics_service_destroy(service);
}

SystemMetricsService::SystemMetricsService(MetricsConfig config) :
    config_(std::move(config))
{
}

SystemMetricsService::~SystemMetricsService()
{
    stop();
}

auto SystemMetricsService::start() -> Result<void>
{
    try
    {
        std::lock_guard lock{mutex_};
        if (service_)
            return failure<void>(ErrorCode::InvalidConfiguration);

        szx_metrics_config config{};
        szx_metrics_config_init(&config);
        config.flags = SZX_METRICS_CARD_CPU | SZX_METRICS_CARD_MEMORY | SZX_METRICS_CARD_DISK | SZX_METRICS_CARD_NETWORK |
            SZX_METRICS_CARD_GPU;
        std::vector<const char *> paths;
        paths.reserve(config_.diskPaths.size());
        for (const auto &path : config_.diskPaths)
            paths.push_back(path.c_str());
        if (paths.size() > SZX_METRICS_MAX_DISK_PATHS)
            return failure<void>(ErrorCode::InvalidConfiguration);
        config.disk_paths      = paths.data();
        config.disk_path_count = static_cast<std::uint32_t>(paths.size());
        std::vector<const char *> names;
        names.reserve(config_.processNames.size());
        for (const auto &name : config_.processNames)
            names.push_back(name.c_str());
        if (names.size() > SZX_METRICS_MAX_PROCESS_NAMES)
            return failure<void>(ErrorCode::InvalidConfiguration);
        config.process_names      = names.data();
        config.process_name_count = static_cast<std::uint32_t>(names.size());
        if (!names.empty())
            config.flags |= SZX_METRICS_CARD_PROCESS;

        szx_metrics_service *created = nullptr;
        const auto createdCode       = szx_metrics_service_create(&config, &created);
        if (createdCode != SZX_METRICS_OK)
            return failure<void>(createdCode == SZX_METRICS_INVALID_ARGUMENT || createdCode == SZX_METRICS_INVALID_CONFIG ?
                                     ErrorCode::InvalidConfiguration :
                                     metricsError(createdCode));
        std::unique_ptr<szx_metrics_service, MetricsServiceDeleter> pending{created};
        const auto startedCode = szx_metrics_service_start(pending.get());
        if (startedCode != SZX_METRICS_OK)
            return failure<void>(startedCode == SZX_METRICS_INVALID_CONFIG ? ErrorCode::InvalidConfiguration :
                                                                             metricsError(startedCode));
        service_ = std::move(pending);
        return Result<void>::Success();
    } catch (...)
    {
        return failure<void>(ErrorCode::InternalFailure);
    }
}

void SystemMetricsService::stop() noexcept
{
    try
    {
        std::lock_guard lock{mutex_};
        service_.reset();
    } catch (...)
    {
    }
}

auto SystemMetricsService::snapshot() const -> Result<MetricsSnapshotDto>
{
    try
    {
        std::lock_guard lock{mutex_};
        if (!service_)
            return failure<MetricsSnapshotDto>(ErrorCode::ServiceUnavailable);
        szx_metrics_snapshot *created = nullptr;
        const auto code               = szx_metrics_service_snapshot(service_.get(), &created);
        if (code != SZX_METRICS_OK)
            return failure<MetricsSnapshotDto>(metricsError(code));
        Snapshot snapshot{created, &szx_metrics_snapshot_release};

        auto cpu    = sizedMetric();
        auto memory = sizedMetric();
        auto gpu    = sizedMetric();
        if (szx_metrics_snapshot_cpu(snapshot.get(), &cpu) != SZX_METRICS_OK ||
            szx_metrics_snapshot_memory(snapshot.get(), &memory) != SZX_METRICS_OK ||
            szx_metrics_snapshot_gpu(snapshot.get(), &gpu) != SZX_METRICS_OK)
            return failure<MetricsSnapshotDto>(ErrorCode::InternalFailure);
        szx_metrics_gpu_extra gpuExtra{};
        gpuExtra.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_gpu_extra);
        if (szx_metrics_snapshot_gpu_extra(snapshot.get(), &gpuExtra) != SZX_METRICS_OK)
            return failure<MetricsSnapshotDto>(ErrorCode::InternalFailure);

        MetricsSnapshotDto result;
        result.sequence           = szx_metrics_snapshot_sequence(snapshot.get());
        result.sampled_at         = szx_metrics_snapshot_sampled_at(snapshot.get());
        result.cpu                = metricDto(cpu);
        result.memory             = metricDto(memory);
        result.instance_id        = szx_metrics_snapshot_instance_id(snapshot.get());
        result.platform           = szx_metrics_snapshot_platform(snapshot.get());
        result.logical_cpus       = szx_metrics_snapshot_logical_cpus(snapshot.get());
        result.sample_interval_ms = szx_metrics_snapshot_sample_interval_ms(snapshot.get());
        result.disk_interval_ms   = szx_metrics_snapshot_disk_interval_ms(snapshot.get());
        result.cpu_name           = szx_metrics_snapshot_cpu_name(snapshot.get());
        result.memory_spec        = szx_metrics_snapshot_memory_spec(snapshot.get());
        result.gpu                = metricDto(gpu);
        result.gpu_engine         = gpuEngineDto(gpuExtra);
        result.gpu_count          = szx_metrics_snapshot_gpu_count(snapshot.get());
        result.gpu_name           = szx_metrics_snapshot_gpu_name(snapshot.get());
        const auto diskCount      = szx_metrics_snapshot_disk_count(snapshot.get());
        result.disks.reserve(diskCount);
        for (std::uint32_t index = 0; index < diskCount; ++index)
        {
            szx_metrics_disk disk{};
            disk.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_disk);
            if (szx_metrics_snapshot_disk_at(snapshot.get(), index, &disk) != SZX_METRICS_OK)
                return failure<MetricsSnapshotDto>(ErrorCode::InternalFailure);
            result.disks.push_back({disk.id, disk.path, disk.sampled_at, metricDto(disk.capacity), disk.type});
        }
        const auto networkCount = szx_metrics_snapshot_network_count(snapshot.get());
        result.networks.reserve(networkCount);
        for (std::uint32_t index = 0; index < networkCount; ++index)
        {
            szx_metrics_network network{};
            network.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_network);
            if (szx_metrics_snapshot_network_at(snapshot.get(), index, &network) != SZX_METRICS_OK)
                return failure<MetricsSnapshotDto>(ErrorCode::InternalFailure);
            result.networks.push_back({network.id, network.name, network.description, network.ipv4, network.ipv6,
                                       network.sampled_at,
                                       (network.present & 1U) ? std::optional<double>{network.rx_bytes_per_sec} : std::nullopt,
                                       (network.present & 2U) ? std::optional<double>{network.tx_bytes_per_sec} : std::nullopt,
                                       (network.present & 4U) ? std::optional<double>{network.link_mbps} : std::nullopt});
        }
        const auto processCount = szx_metrics_snapshot_process_count(snapshot.get());
        result.processes.reserve(processCount);
        for (std::uint32_t index = 0; index < processCount; ++index)
        {
            szx_metrics_process process{};
            process.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_process);
            if (szx_metrics_snapshot_process_at(snapshot.get(), index, &process) != SZX_METRICS_OK)
                return failure<MetricsSnapshotDto>(ErrorCode::InternalFailure);
            szx_metrics_metric state{};
            state.state       = process.state;
            const auto status = metricDto(state);
            result.processes.push_back(
                {process.name, status.status, status.error_code,
                 process.state == SZX_METRICS_METRIC_OK ? std::optional<double>{process.cpu_percent} : std::nullopt,
                 process.state == SZX_METRICS_METRIC_OK ? std::optional<std::uint64_t>{process.memory_bytes} : std::nullopt});
        }
        return Result<MetricsSnapshotDto>::Success(std::move(result));
    } catch (...)
    {
        return failure<MetricsSnapshotDto>(ErrorCode::InternalFailure);
    }
}

auto SystemMetricsService::history(unsigned windowSeconds) const -> Result<MetricsHistoryDto>
{
    if (!windowSeconds || windowSeconds > SZX_METRICS_MAX_WINDOW_SECONDS)
        return failure<MetricsHistoryDto>(ErrorCode::ValidationError);
    try
    {
        std::lock_guard lock{mutex_};
        if (!service_)
            return failure<MetricsHistoryDto>(ErrorCode::ServiceUnavailable);
        szx_metrics_history *created = nullptr;
        const auto code              = szx_metrics_service_history(service_.get(), windowSeconds, &created);
        if (code != SZX_METRICS_OK)
            return failure<MetricsHistoryDto>(metricsError(code));
        History history{created, &szx_metrics_history_release};

        MetricsHistoryDto result;
        result.instance_id        = szx_metrics_history_instance_id(history.get());
        result.sample_interval_ms = szx_metrics_history_sample_interval_ms(history.get());
        result.window_seconds     = szx_metrics_history_window_seconds(history.get());
        const auto count          = szx_metrics_history_count(history.get());
        result.samples.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            szx_metrics_sample sample{};
            sample.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_sample);
            if (szx_metrics_history_sample_at(history.get(), index, &sample) != SZX_METRICS_OK)
                return failure<MetricsHistoryDto>(ErrorCode::InternalFailure);
            result.samples.push_back(
                {sample.sequence, sample.sampled_at, metricDto(sample.cpu), metricDto(sample.memory), metricDto(sample.gpu)});
        }
        return Result<MetricsHistoryDto>::Success(std::move(result));
    } catch (...)
    {
        return failure<MetricsHistoryDto>(ErrorCode::InternalFailure);
    }
}
} // namespace v1
} // namespace SZX::ADMIN::SYSTEM
