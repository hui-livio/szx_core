#pragma once
/** @file MetricsInternal.h
 * @brief 组件内部数据模型与平台采集接口。
 * @details 只在 src/ 内使用，绝不安装、不对外暴露。公共头只认 C ABI；
 *          这里用 STL 组织数据是为了实现方便，二者通过 MetricsService.cc
 *          中的转换函数衔接，STL 类型不会跨越 C ABI 边界。
 */

#include "SzxMetrics/MetricsTypes.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::METRICS::DETAIL
{
inline namespace v1
{
/** 指标单项，对应一个 metric 槽位。present 位描述字段是否存在。 */
struct MetricValue final
{
    std::uint32_t state{SZX_METRICS_METRIC_UNAVAILABLE};
    std::uint32_t present{};
    double usagePercent{};
    double temperatureCelsius{};
    std::uint64_t totalBytes{};
    std::uint64_t usedBytes{};
    std::uint64_t availableBytes{};
    std::uint64_t freeBytes{};
};

struct DiskInfo final
{
    std::string id;
    std::string path;
    std::string type;
    std::int64_t sampledAt{};
    MetricValue capacity;
};

struct NetworkInfo final
{
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> ipv4;
    std::vector<std::string> ipv6;
    std::int64_t sampledAt{};
    std::optional<double> rxBytesPerSec;
    std::optional<double> txBytesPerSec;
    std::optional<double> linkMbps;
};

struct ProcessInfo final
{
    std::string name;
    std::uint32_t state{SZX_METRICS_METRIC_UNAVAILABLE};
    double cpuPercent{};
    std::uint64_t memoryBytes{};
};

struct GpuExtraInfo final
{
    std::uint32_t state{SZX_METRICS_METRIC_UNAVAILABLE};
    std::optional<double> totalPercent;
    std::optional<double> cudaPercent;
    std::optional<double> graphics3dPercent;
    /** 按程序名归类的显存占用（字节）与最忙引擎占用率（百分比）。 */
    std::map<std::string, std::uint64_t> vramByProcess;
    std::map<std::string, double> usageByProcess;
};

struct Sample final
{
    std::uint64_t sequence{};
    std::int64_t sampledAt{};
    MetricValue cpu;
    MetricValue memory;
    MetricValue gpu;
    GpuExtraInfo gpuExtra;
    std::vector<ProcessInfo> processes;
};

/** 卡片开关。 */
struct CardFlags final
{
    bool cpu{true};
    bool memory{true};
    bool disk{true};
    bool network{true};
    bool process{true};
    bool gpu{true};

    [[nodiscard]] auto mask() const noexcept -> std::uint32_t
    {
        return (cpu ? SZX_METRICS_CARD_CPU : 0U) | (memory ? SZX_METRICS_CARD_MEMORY : 0U) | (disk ? SZX_METRICS_CARD_DISK : 0U) |
            (network ? SZX_METRICS_CARD_NETWORK : 0U) | (process ? SZX_METRICS_CARD_PROCESS : 0U) |
            (gpu ? SZX_METRICS_CARD_GPU : 0U);
    }
    static auto fromMask(std::uint32_t mask) noexcept -> CardFlags
    {
        const auto flag = [mask](std::uint32_t bit)
        {
            return (mask & bit) != 0U;
        };
        return {flag(SZX_METRICS_CARD_CPU),     flag(SZX_METRICS_CARD_MEMORY),  flag(SZX_METRICS_CARD_DISK),
                flag(SZX_METRICS_CARD_NETWORK), flag(SZX_METRICS_CARD_PROCESS), flag(SZX_METRICS_CARD_GPU)};
    }
};

/** 平台采集器接口：每个平台一份实现，公共代码只依赖本接口。
 * @details 所有方法允许抛异常，上层逐个捕获并降级为 UNAVAILABLE，
 *          保证一项采集失败不会牵连其余指标。
 */
class Collector
{
public:
    Collector()                                      = default;
    Collector(const Collector &)                     = delete;
    auto operator=(const Collector &) -> Collector & = delete;
    virtual ~Collector()                             = default;

    [[nodiscard]] virtual auto platformName() const -> const char *      = 0;
    [[nodiscard]] virtual auto logicalCpus() const -> std::uint32_t      = 0;
    [[nodiscard]] virtual auto cpuName() const -> std::string            = 0;
    [[nodiscard]] virtual auto memorySpec() const -> std::string         = 0;
    [[nodiscard]] virtual auto cpuTemperature() -> std::optional<double> = 0;

    [[nodiscard]] virtual auto cpu() -> MetricValue    = 0;
    [[nodiscard]] virtual auto memory() -> MetricValue = 0;
    [[nodiscard]] virtual auto disks(const std::vector<std::string> &configured, std::int64_t sampledAt)
        -> std::vector<DiskInfo>                                                            = 0;
    [[nodiscard]] virtual auto networks(std::int64_t sampledAt) -> std::vector<NetworkInfo> = 0;
    [[nodiscard]] virtual auto processes(const std::vector<std::string> &names, std::uint32_t logicalCpus)
        -> std::vector<ProcessInfo>                                          = 0;
    [[nodiscard]] virtual auto gpuCount() -> std::uint32_t                   = 0;
    [[nodiscard]] virtual auto gpuName(std::uint32_t index) -> std::string   = 0;
    [[nodiscard]] virtual auto gpu(std::uint32_t index) -> MetricValue       = 0;
    [[nodiscard]] virtual auto gpuExtra(std::uint32_t index) -> GpuExtraInfo = 0;
};

/** 平台工厂：定义在 Platforms/PlatformWindows.cc 或 PlatformLinux.cc 中。 */
[[nodiscard]] auto createCollector() -> std::unique_ptr<Collector>;

/* ---------------------------------------------------------------------- */
/* 纯计算助手：与平台无关，同时导出为 C 函数供调用方复用与测试            */
/* ---------------------------------------------------------------------- */

struct CpuCountersRaw final
{
    std::uint64_t total{};
    std::uint64_t idle{};
};

[[nodiscard]] auto parseCpuCounters(std::string_view text) -> std::optional<CpuCountersRaw>;
[[nodiscard]] auto cpuUsage(CpuCountersRaw previous, CpuCountersRaw current) noexcept -> std::optional<double>;
[[nodiscard]] auto parseMemory(std::string_view text) -> MetricValue;
[[nodiscard]] auto capacityMetric(std::uint64_t total, std::uint64_t free, std::uint64_t available) -> MetricValue;

/** 字符串驻留区：快照 / 历史对象持有，保证 const char* 有效到对象释放。
 * @details std::deque 的 push_back 不会让已有元素的引用失效，正好满足需要。
 */
class StringArena final
{
public:
    [[nodiscard]] auto intern(std::string_view text) -> const char *
    {
        storage_.emplace_back(text);
        return storage_.back().c_str();
    }

private:
    std::deque<std::string> storage_;
};
} // namespace v1
} // namespace SZX::METRICS::DETAIL
