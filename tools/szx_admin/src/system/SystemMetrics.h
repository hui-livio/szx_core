#pragma once

#include "Result.hpp"
#include "SystemMetricsDto.h"

#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace SZX::ADMIN::SYSTEM
{
inline namespace v1
{
/** Only server-configured local volumes are monitored; empty means auto-discover (max 32). */
struct MetricsConfig final
{
    std::vector<std::string> diskPaths;
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
/** Pure parser/calculation helpers used by the platform collector and deterministic tests. */
[[nodiscard]] auto parseCpuCounters(std::string_view text) -> std::optional<CpuCounters>;
[[nodiscard]] auto cpuUsage(CpuCounters previous, CpuCounters current) noexcept -> std::optional<double>;
[[nodiscard]] auto parseMemory(std::string_view text) -> Metric;
[[nodiscard]] auto capacityMetric(std::uint64_t total, std::uint64_t free, std::uint64_t available) -> Metric;

/** Single bounded sampler shared by all HTTP callers; no database or request-time OS collection.
 * Stable service boundaries contain exceptions and return admin-owned error codes.
 * The instance must outlive registered handlers. stop() wakes and joins the worker. */
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
    class Platform;
    void sample(bool disks);
    MetricsConfig config_;
    std::unique_ptr<Platform> platform_;
    std::string instanceId_;
    mutable std::mutex mutex_;
    std::mutex lifecycleMutex_;
    std::condition_variable_any wake_;
    std::jthread worker_;
    bool running_{};
    unsigned logicalCpus_{};
    std::deque<MetricsSample> samples_;
    std::vector<DiskMetric> disks_;
};
} // namespace v1
} // namespace SZX::ADMIN::SYSTEM
