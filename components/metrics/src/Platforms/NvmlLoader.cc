/** @file NvmlLoader.cc
 * @brief NVML 按需加载的实现。Windows 与 Linux 仅符号来源不同，其余共用。
 */

#include "NvmlLoader.h"

#include "TimeHelper.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace SZX::METRICS::DETAIL
{
inline namespace v1
{
NvmlLoader::~NvmlLoader()
{
    std::lock_guard lock{mutex_};
    if (initialized_ && shutdown_ != nullptr)
        shutdown_();
#ifdef _WIN32
    if (module_ != nullptr)
        ::FreeLibrary(static_cast<HMODULE>(module_));
#else
    if (module_ != nullptr)
        ::dlclose(module_);
#endif
}
auto NvmlLoader::ready() -> bool
{
    std::lock_guard lock{mutex_};
    if (initialized_)
        return true;
    const auto now = SZX::COMMON::TimeHelper::NowTimestamp();
    if (now < nextAttemptAt_)
        return false;
    nextAttemptAt_ = now + kRetryIntervalMs;
    return load();
}
auto NvmlLoader::load() -> bool
{
#ifdef _WIN32
    // 不写绝对路径：nvml.dll 随驱动装在 System32，走标准搜索顺序即可命中
    module_ = static_cast<void *>(::LoadLibraryW(L"nvml.dll"));
#else
    // 先试版本化的 soname，再退回无版本名；两者都由驱动安装
    for (const char *name : {"libnvidia-ml.so.1", "libnvidia-ml.so"})
    {
        module_ = ::dlopen(name, RTLD_NOW | RTLD_LOCAL);
        if (module_ != nullptr)
            break;
    }
#endif
    if (module_ == nullptr)
        return false;

    // 每个符号都先试 _v2 再退到无后缀名：头文件里的别名只存在于头文件
    init_           = resolve<Init>("nvmlInit_v2", "nvmlInit");
    shutdown_       = resolve<Shutdown>("nvmlShutdown");
    getCount_       = resolve<GetCount>("nvmlDeviceGetCount_v2", "nvmlDeviceGetCount");
    getHandle_      = resolve<GetHandle>("nvmlDeviceGetHandleByIndex_v2", "nvmlDeviceGetHandleByIndex");
    getName_        = resolve<GetName>("nvmlDeviceGetName");
    getUtilization_ = resolve<GetUtilization>("nvmlDeviceGetUtilizationRates");
    getMemory_      = resolve<GetMemory>("nvmlDeviceGetMemoryInfo");
    getTemperature_ = resolve<GetTemperature>("nvmlDeviceGetTemperature");
    // 进程显存只用 _v3：v1/v2 的 nvmlProcessInfo_t 少两个字段
    getComputeProcesses_  = resolve<GetProcesses>("nvmlDeviceGetComputeRunningProcesses_v3");
    getGraphicsProcesses_ = resolve<GetProcesses>("nvmlDeviceGetGraphicsRunningProcesses_v3");

    if (init_ == nullptr || init_() != kNvmlSuccess)
    {
#ifdef _WIN32
        ::FreeLibrary(static_cast<HMODULE>(module_));
#else
        ::dlclose(module_);
#endif
        module_ = nullptr;
        return false;
    }
    initialized_ = true;
    return true;
}
auto NvmlLoader::deviceCount() -> unsigned
{
    unsigned count = 0;
    if (!ready() || getCount_ == nullptr)
        return 0;
    // 用 _v2：老版 nvmlDeviceGetCount 收 int*，混用会把 4 字节写成 8 字节读
    if (getCount_(&count) != kNvmlSuccess)
        return 0;
    return count;
}
auto NvmlLoader::handleAt(unsigned index) -> NvmlDevice
{
    if (!ready() || getHandle_ == nullptr || index >= deviceCount())
        return nullptr;
    NvmlDevice device = nullptr;
    if (getHandle_(index, &device) != kNvmlSuccess)
        return nullptr;
    return device;
}
auto NvmlLoader::deviceName(unsigned index) -> std::string
{
    const auto device = handleAt(index);
    if (device == nullptr || getName_ == nullptr)
        return {};
    // 官方头文件建议的缓冲区容量就是 96 字节（含结尾 NUL）
    std::array<char, 96> name{};
    if (getName_(device, name.data(), static_cast<unsigned>(name.size())) != kNvmlSuccess)
        return {};
    name.back() = '\0';
    return std::string(name.data());
}
auto NvmlLoader::sample(unsigned index) -> NvmlSample
{
    NvmlSample result;
    const auto device = handleAt(index);
    if (device == nullptr)
        return result;

    unsigned temperature = 0;
    if (getTemperature_ != nullptr && getTemperature_(device, 0, &temperature) == kNvmlSuccess && temperature <= 150)
        result.temperatureCelsius = static_cast<double>(temperature);

    if (getUtilization_ != nullptr)
    {
        NvmlUtilization utilization{};
        if (getUtilization_(device, &utilization) == kNvmlSuccess && utilization.gpu <= 100U)
        {
            result.usageValid   = true;
            result.usagePercent = static_cast<double>(utilization.gpu);
        }
    }
    if (getMemory_ != nullptr)
    {
        NvmlMemory memory{};
        if (getMemory_(device, &memory) == kNvmlSuccess && memory.total > 0 && memory.free <= memory.total)
        {
            result.memoryValid = true;
            result.totalBytes  = memory.total;
            result.freeBytes   = memory.free;
            // 显存没有「可回收缓存」，核占用率缺失时用显存占用率兜底
            if (!result.usageValid)
            {
                result.usageValid   = true;
                result.usagePercent = 100.0 * static_cast<double>(memory.total - memory.free) / static_cast<double>(memory.total);
            }
        }
    }
    return result;
}
auto NvmlLoader::processMemory(unsigned index) -> std::map<unsigned, unsigned long long>
{
    std::map<unsigned, unsigned long long> result;
    const auto device = handleAt(index);
    if (device == nullptr)
        return result;
    for (const auto function : {getComputeProcesses_, getGraphicsProcesses_})
    {
        if (function == nullptr)
            continue;
        // 一次给足 256 槽位，避免「先问容量再取值」两趟调用
        std::array<NvmlProcessInfo, 256> infos{};
        unsigned count = static_cast<unsigned>(infos.size());
        if (function(device, infos.data(), &count) != kNvmlSuccess)
            continue;
        for (unsigned i = 0; i < count && i < infos.size(); ++i)
        {
            if (infos[i].usedGpuMemory == kNvmlValueNotAvailable)
                continue;
            result[infos[i].pid] += infos[i].usedGpuMemory;
        }
    }
    return result;
}
} // namespace v1
} // namespace SZX::METRICS::DETAIL
