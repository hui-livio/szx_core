/** @file MetricsParsers.cc
 * @brief 与平台无关的解析与计算助手。
 * @details 这些函数既被 Linux/Windows 采集器内部复用，也通过 C ABI 导出，
 *          让调用方能在自己的单元测试里直接验证解析口径，而不必依赖真实机器。
 *          解析口径由公共测试固定，平台采集器与调用方共用。
 */

#include "MetricsAbi.h"
#include "MetricsInternal.h"
#include "SzxMetrics/Metrics.h"

#include <array>
#include <limits>
#include <sstream>
#include <string>

namespace SZX::METRICS::DETAIL
{
inline namespace v1
{
auto parseCpuCounters(std::string_view text) -> std::optional<CpuCountersRaw>
{
    const auto line = text.substr(0, text.find('\n'));
    std::istringstream input{std::string(line)};
    std::string label;
    std::array<std::uint64_t, 8> ticks{};
    if (!(input >> label) || label != "cpu")
        return std::nullopt;
    for (auto &tick : ticks)
        if (!(input >> tick))
            return std::nullopt;

    CpuCountersRaw result;
    for (const auto tick : ticks)
    {
        if (tick > std::numeric_limits<std::uint64_t>::max() - result.total)
            return std::nullopt;
        result.total += tick;
    }
    // idle + iowait；guest 已经计入 user/nice，不能重复相加
    result.idle = ticks[3] + ticks[4];
    return result;
}
auto cpuUsage(CpuCountersRaw previous, CpuCountersRaw current) noexcept -> std::optional<double>
{
    if (current.total <= previous.total || current.idle < previous.idle)
        return std::nullopt;
    const auto total = current.total - previous.total;
    const auto idle  = current.idle - previous.idle;
    if (idle > total)
        return std::nullopt;
    return 100.0 * static_cast<double>(total - idle) / static_cast<double>(total);
}
auto capacityMetric(std::uint64_t total, std::uint64_t free, std::uint64_t available) -> MetricValue
{
    if (!total || free > total || available > free)
        return {};
    MetricValue result;
    result.state   = SZX_METRICS_METRIC_OK;
    result.present = SZX_METRICS_FIELD_USAGE | SZX_METRICS_FIELD_TOTAL | SZX_METRICS_FIELD_USED | SZX_METRICS_FIELD_AVAILABLE |
        SZX_METRICS_FIELD_FREE;
    result.totalBytes     = total;
    result.usedBytes      = total - free;
    result.availableBytes = available;
    result.freeBytes      = free;
    result.usagePercent   = 100.0 * static_cast<double>(total - free) / static_cast<double>(total);
    return result;
}
auto parseMemory(std::string_view text) -> MetricValue
{
    std::istringstream lines{std::string(text)};
    std::string line;
    std::optional<std::uint64_t> total, available;
    while (std::getline(lines, line))
    {
        std::istringstream input{line};
        std::string key, unit;
        std::uint64_t value{};
        if (!(input >> key) || (key != "MemTotal:" && key != "MemAvailable:"))
            continue;
        if (!(input >> value >> unit) || unit != "kB" || value > std::numeric_limits<std::uint64_t>::max() / 1024)
            return {};
        (key == "MemTotal:" ? total : available) = value * 1024;
    }
    if (!total || !available)
        return {};
    auto result = capacityMetric(*total, *available, *available);
    // MemAvailable 含可回收缓存，不等于「原始空闲」，因此不置 FREE 位
    result.present   &= ~static_cast<std::uint32_t>(SZX_METRICS_FIELD_FREE);
    result.freeBytes  = 0;
    return result;
}
} // namespace v1
} // namespace SZX::METRICS::DETAIL

namespace
{
using SZX::METRICS::ABI::fill;
using SZX::METRICS::DETAIL::capacityMetric;
using SZX::METRICS::DETAIL::CpuCountersRaw;
using SZX::METRICS::DETAIL::cpuUsage;
using SZX::METRICS::DETAIL::parseCpuCounters;
using SZX::METRICS::DETAIL::parseMemory;

/** 把内部 MetricValue 转成 C 结构体。嵌套结构体的 struct_size 由库填写。 */
auto metricOf(const SZX::METRICS::DETAIL::MetricValue &value) -> szx_metrics_metric
{
    szx_metrics_metric result{};
    result.struct_size         = SZX_METRICS_SIZE_OF(szx_metrics_metric);
    result.state               = value.state;
    result.present             = value.present;
    result.usage_percent       = value.usagePercent;
    result.temperature_celsius = value.temperatureCelsius;
    result.total_bytes         = value.totalBytes;
    result.used_bytes          = value.usedBytes;
    result.available_bytes     = value.availableBytes;
    result.free_bytes          = value.freeBytes;
    return result;
}
auto textView(const char *text, std::uint32_t length) -> std::string_view
{
    if (text == nullptr)
        return {};
    return length ? std::string_view(text, length) : std::string_view(text);
}
} // namespace

extern "C" {
SZX_METRICS_API std::int32_t szx_metrics_parse_cpu_counters(const char *text, std::uint32_t length, szx_metrics_cpu_counters *out)
{
    try
    {
        if (out == nullptr)
            return SZX_METRICS_INVALID_ARGUMENT;
        const auto parsed = parseCpuCounters(textView(text, length));
        if (!parsed)
            return SZX_METRICS_INVALID_ARGUMENT;
        szx_metrics_cpu_counters result{};
        result.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_cpu_counters);
        result.total       = parsed->total;
        result.idle        = parsed->idle;
        return fill(out, SZX::METRICS::ABI::kCpuCountersSize, result);
    } catch (...)
    {
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
SZX_METRICS_API std::int32_t szx_metrics_cpu_usage(const szx_metrics_cpu_counters *previous,
                                                   const szx_metrics_cpu_counters *current, double *outPercent)
{
    try
    {
        if (previous == nullptr || current == nullptr || outPercent == nullptr)
            return SZX_METRICS_INVALID_ARGUMENT;
        const auto usage =
            cpuUsage(CpuCountersRaw{previous->total, previous->idle}, CpuCountersRaw{current->total, current->idle});
        if (!usage)
            return SZX_METRICS_OUT_OF_RANGE;
        *outPercent = *usage;
        return SZX_METRICS_OK;
    } catch (...)
    {
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
SZX_METRICS_API std::int32_t szx_metrics_parse_memory(const char *text, std::uint32_t length, szx_metrics_metric *out)
{
    try
    {
        if (out == nullptr)
            return SZX_METRICS_INVALID_ARGUMENT;
        return fill(out, SZX::METRICS::ABI::kMetricPrefixSize, metricOf(parseMemory(textView(text, length))));
    } catch (...)
    {
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
SZX_METRICS_API std::int32_t szx_metrics_capacity_metric(std::uint64_t total, std::uint64_t free, std::uint64_t available,
                                                         szx_metrics_metric *out)
{
    try
    {
        if (out == nullptr)
            return SZX_METRICS_INVALID_ARGUMENT;
        return fill(out, SZX::METRICS::ABI::kMetricPrefixSize, metricOf(capacityMetric(total, free, available)));
    } catch (...)
    {
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
} // extern "C"
