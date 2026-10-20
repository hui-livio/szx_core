#pragma once
/** @file NvmlLoader.h
 * @brief NVML（NVIDIA Management Library）按需加载器，Windows / Linux 共用。
 * @details 为什么动态加载而不是链接 nvml：
 *          - 没有 N 卡的机器不能因为缺少 nvml.dll / libnvidia-ml.so 而加载失败；
 *          - 驱动安装目录里通常没有可供链接的 .lib / .so 开发版本；
 *          - GPU 采不到只该少一张卡，不能牵连 CPU / 内存 / 磁盘。
 *          结构体是 nvml.h 中对应类型的逐字副本，只用 NVML 1.0 起布局未变的 API。
 */

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace SZX::METRICS::DETAIL
{
inline namespace v1
{
/** nvmlReturn_t 的成功码。 */
constexpr int kNvmlSuccess = 0;
/** 对应 NVML_VALUE_NOT_AVAILABLE：WDDM 下按进程显存恒为此值。 */
constexpr unsigned long long kNvmlValueNotAvailable = ~0ULL;

struct NvmlDeviceSt;
using NvmlDevice = NvmlDeviceSt *;

struct NvmlUtilization final
{
    unsigned int gpu;
    unsigned int memory;
};
struct NvmlMemory final
{
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};
/** 对应 nvmlProcessInfo_t（_v3 的四字段形态）。 */
struct NvmlProcessInfo final
{
    unsigned int pid;
    unsigned long long usedGpuMemory;
    unsigned int gpuInstanceId;
    unsigned int computeInstanceId;
};

/** 一次 GPU 采样的原始结果，避免把内部 MetricValue 依赖引入本头。 */
struct NvmlSample final
{
    bool usageValid{};
    double usagePercent{};
    bool memoryValid{};
    std::uint64_t totalBytes{};
    std::uint64_t freeBytes{};
    std::optional<double> temperatureCelsius;
};

class NvmlLoader final
{
public:
    /** 显存明细每 30 秒重试一次「有没有 GPU」：重装驱动后不必重启进程。 */
    static constexpr std::int64_t kRetryIntervalMs = 30000;

    NvmlLoader() = default;
    ~NvmlLoader();
    NvmlLoader(const NvmlLoader &)                     = delete;
    auto operator=(const NvmlLoader &) -> NvmlLoader & = delete;

    [[nodiscard]] auto deviceCount() -> unsigned;
    [[nodiscard]] auto deviceName(unsigned index) -> std::string;
    /** 第 index 块卡的核心占用率、显存、温度。任一子项失败时其余子项仍然返回。 */
    [[nodiscard]] auto sample(unsigned index) -> NvmlSample;
    /** 第 index 块卡上按 pid 的显存占用；WDDM 下返回空表属于预期。 */
    [[nodiscard]] auto processMemory(unsigned index) -> std::map<unsigned, unsigned long long>;

private:
    using Init           = int (*)();
    using Shutdown       = int (*)();
    using GetCount       = int (*)(unsigned *);
    using GetHandle      = int (*)(unsigned, NvmlDevice *);
    using GetName        = int (*)(NvmlDevice, char *, unsigned);
    using GetUtilization = int (*)(NvmlDevice, NvmlUtilization *);
    using GetMemory      = int (*)(NvmlDevice, NvmlMemory *);
    using GetTemperature = int (*)(NvmlDevice, unsigned, unsigned *);
    using GetProcesses   = int (*)(NvmlDevice, NvmlProcessInfo *, unsigned int *);

    [[nodiscard]] auto ready() -> bool;
    [[nodiscard]] auto load() -> bool;
    [[nodiscard]] auto handleAt(unsigned index) -> NvmlDevice;
    template <typename Fn>
    [[nodiscard]] auto resolve(const char *name, const char *fallback = nullptr) const -> Fn
    {
#ifdef _WIN32
        auto *symbol = ::GetProcAddress(static_cast<HMODULE>(module_), name);
        if (symbol == nullptr && fallback != nullptr)
            symbol = ::GetProcAddress(static_cast<HMODULE>(module_), fallback);
#else
        auto *symbol = ::dlsym(module_, name);
        if (symbol == nullptr && fallback != nullptr)
            symbol = ::dlsym(module_, fallback);
#endif
        return reinterpret_cast<Fn>(symbol);
    }

    mutable std::mutex mutex_;
    void *module_{};
    bool initialized_{};
    std::int64_t nextAttemptAt_{};
    Init init_{};
    Shutdown shutdown_{};
    GetCount getCount_{};
    GetHandle getHandle_{};
    GetName getName_{};
    GetUtilization getUtilization_{};
    GetMemory getMemory_{};
    GetTemperature getTemperature_{};
    GetProcesses getComputeProcesses_{};
    GetProcesses getGraphicsProcesses_{};
};
} // namespace v1
} // namespace SZX::METRICS::DETAIL
