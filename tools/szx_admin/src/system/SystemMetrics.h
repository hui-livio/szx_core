#pragma once

#include "Result.hpp"
#include "SystemMetricsDto.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct szx_metrics_service;

namespace SZX::ADMIN::SYSTEM
{
inline namespace v1
{
/** Only server-configured local volumes are monitored; empty means auto-discover (max 32). */
struct MetricsConfig final
{
    std::vector<std::string> diskPaths;
    std::vector<std::string> processNames;
};

struct CpuCounters final
{
    std::uint64_t total{};
    std::uint64_t idle{};
};
struct Metric final
{
    std::string status{"unavailable"};
    std::optional<double> usagePercent;
    std::optional<std::uint64_t> totalBytes;
    std::optional<std::uint64_t> usedBytes;
    std::optional<std::uint64_t> availableBytes;
    std::optional<std::uint64_t> freeBytes;
};
struct DiskMetric final
{
    std::string id;
    std::string path;
    std::int64_t sampledAt{};
    Metric capacity;
};
struct MetricsSample final
{
    std::uint64_t sequence{};
    std::int64_t sampledAt{};
    Metric cpu;
    Metric memory;
};
/** Compatibility helpers backed by szx_metrics' C ABI. */
[[nodiscard]] auto parseCpuCounters(std::string_view text) -> std::optional<CpuCounters>;
[[nodiscard]] auto cpuUsage(CpuCounters previous, CpuCounters current) noexcept -> std::optional<double>;
[[nodiscard]] auto parseMemory(std::string_view text) -> Metric;
[[nodiscard]] auto capacityMetric(std::uint64_t total, std::uint64_t free, std::uint64_t available) -> Metric;

/** Adapts one szx_metrics sampler to the existing admin HTTP contract.
 * The instance must outlive registered handlers; errors use admin-owned codes. */
class SystemMetricsService final
{
public:
    explicit SystemMetricsService(MetricsConfig config = {});
    ~SystemMetricsService();
    SystemMetricsService(const SystemMetricsService &)                     = delete;
    auto operator=(const SystemMetricsService &) -> SystemMetricsService & = delete;
    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    void stop() noexcept;
    [[nodiscard]] auto snapshot() const -> SZX::COMMON::Result<MetricsSnapshotDto>;
    [[nodiscard]] auto history(unsigned windowSeconds = 300) const -> SZX::COMMON::Result<MetricsHistoryDto>;

private:
    struct MetricsServiceDeleter
    {
        void operator()(szx_metrics_service *service) const noexcept;
    };
    MetricsConfig config_;
    mutable std::mutex mutex_;
    std::unique_ptr<szx_metrics_service, MetricsServiceDeleter> service_;
};
} // namespace v1
} // namespace SZX::ADMIN::SYSTEM
