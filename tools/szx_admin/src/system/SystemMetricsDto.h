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
};
SZX_REFL(MetricDto, status, error_code, usage_percent, total_bytes, used_bytes, available_bytes, free_bytes);

struct DiskMetricDto final
{
    std::string id;
    std::string path;
    std::int64_t sampled_at{};
    MetricDto capacity;
};
SZX_REFL(DiskMetricDto, id, path, sampled_at, capacity);

struct MetricsSampleDto final
{
    std::uint64_t sequence{};
    std::int64_t sampled_at{};
    MetricDto cpu;
    MetricDto memory;
};
SZX_REFL(MetricsSampleDto, sequence, sampled_at, cpu, memory);

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
};
SZX_REFL(MetricsSnapshotDto, sequence, sampled_at, cpu, memory, instance_id, platform, logical_cpus, sample_interval_ms,
         disk_interval_ms, disks);

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
