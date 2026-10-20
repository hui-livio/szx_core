#pragma once
/** @file MetricsAbi.h
 * @brief ABI v1 布局常量与 struct_size 安全的出参写入助手。
 * @details 完整布局大小用于编译期检查，最小前缀大小用于接受旧调用方。
 *          fill 仅写入调用方声明大小与库内布局的交集，不写越界缓冲区。
 *          新字段只能追加在结构体末尾，并同步更新布局检查及前缀约定。
 */

#include "SzxMetrics/Metrics.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace SZX::METRICS::ABI
{
inline namespace v1
{
constexpr std::uint32_t kMetricSize       = 64;
constexpr std::uint32_t kMetricPrefixSize = 32;
constexpr std::uint32_t kDiskSize         = 104;
constexpr std::uint32_t kNetworkSize      = 88;
constexpr std::uint32_t kProcessSize      = 40;
constexpr std::uint32_t kGpuExtraSize     = 40;
constexpr std::uint32_t kNamedValueSize   = 24;
constexpr std::uint32_t kSampleSize       = 216;
constexpr std::uint32_t kConfigSize       = 64;
constexpr std::uint32_t kCpuCountersSize  = 24;

static_assert(sizeof(szx_metrics_metric) == kMetricSize, "szx_metrics_metric layout changed: append fields only");
static_assert(sizeof(szx_metrics_disk) == kDiskSize, "szx_metrics_disk layout changed: append fields only");
static_assert(sizeof(szx_metrics_network) == kNetworkSize, "szx_metrics_network layout changed: append fields only");
static_assert(sizeof(szx_metrics_process) == kProcessSize, "szx_metrics_process layout changed: append fields only");
static_assert(sizeof(szx_metrics_gpu_extra) == kGpuExtraSize, "szx_metrics_gpu_extra layout changed: append fields only");
static_assert(sizeof(szx_metrics_named_value) == kNamedValueSize, "szx_metrics_named_value layout changed: append fields only");
static_assert(sizeof(szx_metrics_sample) == kSampleSize, "szx_metrics_sample layout changed: append fields only");
static_assert(sizeof(szx_metrics_config) == kConfigSize, "szx_metrics_config layout changed: append fields only");
static_assert(sizeof(szx_metrics_cpu_counters) == kCpuCountersSize, "szx_metrics_cpu_counters layout changed");

/** 写入出参的统一入口。
 * @param out     调用方结构体，首字段必须是 struct_size
 * @param minSize 当前 ABI 版本要求调用方至少声明的大小
 * @param source  库内完整结构体
 * @return OK / INVALID_ARGUMENT
 */
template <typename T>
auto fill(T *out, std::uint32_t minSize, const T &source) -> std::int32_t
{
    if (out == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    const std::uint32_t declared = out->struct_size;
    if (declared < minSize)
        return SZX_METRICS_INVALID_ARGUMENT;
    const std::uint32_t bytes = std::min(declared, static_cast<std::uint32_t>(sizeof(T)));
    std::memset(out, 0, bytes);
    std::memcpy(out, &source, bytes);
    out->struct_size = bytes;
    return SZX_METRICS_OK;
}
} // namespace v1
} // namespace SZX::METRICS::ABI
