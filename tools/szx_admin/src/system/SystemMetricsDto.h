#pragma once

#include "Serializer.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SZX::ADMIN::SYSTEM
{
inline namespace v1
{
/** Metrics wire DTOs. Field names are the stable HTTP contract; iguana writes all
 * optionals as a value or null. Keep domain/OS sampling independent of serialization. */
struct MetricDto final
{
    std::string status;
    std::optional<int> error_code;
    std::optional<double> usage_percent;
    std::optional<std::uint64_t> total_bytes;
    std::optional<std::uint64_t> used_bytes;
    std::optional<std::uint64_t> available_bytes;
    std::optional<std::uint64_t> free_bytes;
    std::optional<double> temperature_celsius;
};
SZX_REFL(MetricDto, status, error_code, usage_percent, total_bytes, used_bytes, available_bytes, free_bytes, temperature_celsius);

struct GpuEngineDto final
{
    std::string status;
    std::optional<int> error_code;
    std::optional<double> total_percent;
    std::optional<double> cuda_percent;
    std::optional<double> graphics_3d_percent;
};
SZX_REFL(GpuEngineDto, status, error_code, total_percent, cuda_percent, graphics_3d_percent);

struct NetworkMetricDto final
{
    std::string id;
    std::string name;
    std::string description;
    std::string ipv4;
    std::string ipv6;
    std::int64_t sampled_at{};
    std::optional<double> rx_bytes_per_sec;
    std::optional<double> tx_bytes_per_sec;
    std::optional<double> link_mbps;
};
SZX_REFL(NetworkMetricDto, id, name, description, ipv4, ipv6, sampled_at, rx_bytes_per_sec, tx_bytes_per_sec, link_mbps);

struct ProcessMetricDto final
{
    std::string name;
    std::string status;
    std::optional<int> error_code;
    std::optional<double> cpu_percent;
    std::optional<std::uint64_t> memory_bytes;
};
SZX_REFL(ProcessMetricDto, name, status, error_code, cpu_percent, memory_bytes);

struct DiskMetricDto final
{
    std::string id;
    std::string path;
    std::int64_t sampled_at{};
    MetricDto capacity;
    std::string type;
};
SZX_REFL(DiskMetricDto, id, path, sampled_at, capacity, type);

struct MetricsSampleDto final
{
    std::uint64_t sequence{};
    std::int64_t sampled_at{};
    MetricDto cpu;
    MetricDto memory;
    MetricDto gpu;
};
SZX_REFL(MetricsSampleDto, sequence, sampled_at, cpu, memory, gpu);

struct MetricsSnapshotDto final
{
    std::uint64_t sequence{};
    std::int64_t sampled_at{};
    MetricDto cpu;
    MetricDto memory;
    std::string instance_id;
    std::string platform;
    unsigned logical_cpus{};
    unsigned sample_interval_ms{2000};
    unsigned disk_interval_ms{30000};
    std::vector<DiskMetricDto> disks;
    std::string cpu_name;
    std::string memory_spec;
    MetricDto gpu;
    GpuEngineDto gpu_engine;
    unsigned gpu_count{};
    std::string gpu_name;
    std::vector<NetworkMetricDto> networks;
    std::vector<ProcessMetricDto> processes;
};
SZX_REFL(MetricsSnapshotDto, sequence, sampled_at, cpu, memory, instance_id, platform, logical_cpus, sample_interval_ms,
         disk_interval_ms, disks, cpu_name, memory_spec, gpu, gpu_engine, gpu_count, gpu_name, networks, processes);

struct MetricsHistoryDto final
{
    std::string instance_id;
    unsigned sample_interval_ms{2000};
    unsigned window_seconds{300};
    std::vector<MetricsSampleDto> samples;
};
SZX_REFL(MetricsHistoryDto, instance_id, sample_interval_ms, window_seconds, samples);

struct MetricsSnapshotResponse final
{
    MetricsSnapshotDto data;
    std::string request_id;
};
SZX_REFL(MetricsSnapshotResponse, data, request_id);

struct MetricsHistoryResponse final
{
    MetricsHistoryDto data;
    std::string request_id;
};
SZX_REFL(MetricsHistoryResponse, data, request_id);
} // namespace v1
} // namespace SZX::ADMIN::SYSTEM
