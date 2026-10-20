/** @file MetricsService.cc
 * @brief 服务的公共实现：生命周期、采样线程、历史缓冲、卡片开关、C ABI 转换。
 * @details 平台差异全部收敛在 DETAIL::Collector（Platforms/*.cc），本文件只在
 *          Windows/Linux 之间共享同一套逻辑，不直接包含任何系统头。
 */

#include "MetricsAbi.h"
#include "MetricsInternal.h"
#include "SzxMetrics/Metrics.h"

#include "FileHelper.hpp"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace SZX::METRICS
{
namespace SERVICE
{
using DETAIL::CardFlags;
using DETAIL::Collector;
using DETAIL::DiskInfo;
using DETAIL::GpuExtraInfo;
using DETAIL::MetricValue;
using DETAIL::NetworkInfo;
using DETAIL::ProcessInfo;
using DETAIL::Sample;

namespace
{
namespace fs = std::filesystem;

/** 运行参数：由 szx_metrics_config 校验并拷贝而来。 */
struct Config final
{
    CardFlags cards;
    std::vector<std::string> diskPaths;
    std::vector<std::string> processNames;
    std::string statePath;
    unsigned sampleIntervalMs{SZX_METRICS_DEFAULT_SAMPLE_INTERVAL_MS};
    unsigned diskIntervalMs{SZX_METRICS_DEFAULT_DISK_INTERVAL_MS};
    unsigned historyCapacity{SZX_METRICS_DEFAULT_HISTORY_CAPACITY};
};

auto validPath(const std::string &path) -> bool
{
    if (path.empty() || path.size() > 4096 || path.find('\0') != std::string::npos)
        return false;
    return fs::path{std::u8string{path.begin(), path.end()}}.is_absolute();
}
auto normalizedInterval(unsigned value, unsigned fallback) -> unsigned
{
    if (!value)
        return fallback;
    return std::clamp(value, 100U, 60000U);
}
/** 把 C 配置校验并复制成内部 Config。返回 false 表示参数非法。 */
auto fromAbi(const szx_metrics_config &source, Config &target) -> bool
{
    if (source.struct_size < ABI::kConfigSize)
        return false;
    if (source.disk_path_count > SZX_METRICS_MAX_DISK_PATHS || source.process_name_count > SZX_METRICS_MAX_PROCESS_NAMES)
        return false;
    if ((source.disk_path_count && source.disk_paths == nullptr) ||
        (source.process_name_count && source.process_names == nullptr))
        return false;

    target.cards            = CardFlags::fromMask(source.flags & SZX_METRICS_CARD_ALL);
    target.sampleIntervalMs = normalizedInterval(source.sample_interval_ms, SZX_METRICS_DEFAULT_SAMPLE_INTERVAL_MS);
    target.diskIntervalMs   = normalizedInterval(source.disk_interval_ms, SZX_METRICS_DEFAULT_DISK_INTERVAL_MS);
    target.historyCapacity =
        std::clamp(source.history_capacity ? source.history_capacity : SZX_METRICS_DEFAULT_HISTORY_CAPACITY, 10U, 7200U);
    target.diskPaths.clear();
    for (std::uint32_t index = 0; index < source.disk_path_count; ++index)
    {
        const char *text = source.disk_paths[index];
        if (text == nullptr)
            return false;
        std::string path{text};
        if (!validPath(path))
            return false;
        target.diskPaths.push_back(std::move(path));
    }
    target.processNames.clear();
    for (std::uint32_t index = 0; index < source.process_name_count; ++index)
    {
        const char *text = source.process_names[index];
        if (text == nullptr || *text == '\0')
            return false;
        std::string name{text};
        if (name.size() > 128)
            return false;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        target.processNames.push_back(std::move(name));
    }
    if (source.state_path != nullptr && *source.state_path != '\0')
        target.statePath = source.state_path;
    return true;
}

/** 卡片开关持久化：一行纯文本掩码，读写失败一律静默降级。 */
void persistCards(const std::string &path, std::uint32_t mask) noexcept
{
    if (path.empty())
        return;
    try
    {
        SZX::COMMON::FileHelper::WriteText(fs::path{std::u8string{path.begin(), path.end()}}, std::to_string(mask));
    } catch (...)
    { /* 持久化失败不影响采集 */
    }
}
auto loadCards(const std::string &path) noexcept -> std::optional<std::uint32_t>
{
    if (path.empty())
        return std::nullopt;
    try
    {
        const auto text = SZX::COMMON::FileHelper::ReadText(fs::path{std::u8string{path.begin(), path.end()}});
        if (!text)
            return std::nullopt;
        return static_cast<std::uint32_t>(std::stoul(*text, nullptr, 10)) & SZX_METRICS_CARD_ALL;
    } catch (...)
    {
        return std::nullopt;
    }
}
} // namespace

/** 快照数据：release 之前保持不可变，可被多线程并发读取。 */
struct SnapshotData final
{
    std::uint64_t sequence{};
    std::int64_t sampledAt{};
    std::string instanceId;
    std::string platform;
    std::string cpuName;
    std::string memorySpec;
    std::uint32_t logicalCpus{};
    std::uint32_t sampleIntervalMs{SZX_METRICS_DEFAULT_SAMPLE_INTERVAL_MS};
    std::uint32_t diskIntervalMs{SZX_METRICS_DEFAULT_DISK_INTERVAL_MS};
    std::uint32_t cards{SZX_METRICS_CARD_ALL};
    MetricValue cpu;
    MetricValue memory;
    MetricValue gpu;
    GpuExtraInfo gpuExtra;
    std::uint32_t gpuCount{};
    std::string gpuName;
    std::vector<DiskInfo> disks;
    std::vector<NetworkInfo> networks;
    std::vector<ProcessInfo> processes;
};
struct HistoryData final
{
    std::string instanceId;
    std::uint32_t sampleIntervalMs{SZX_METRICS_DEFAULT_SAMPLE_INTERVAL_MS};
    std::uint32_t windowSeconds{};
    std::vector<Sample> samples;
};

/** 服务实现：线程、历史与状态全部封装在此，C 句柄只是它的载体。 */
class Service final
{
public:
    explicit Service(Config config) :
        config_(std::move(config))
    {
    }
    ~Service()
    {
        stop();
    }
    Service(const Service &)                     = delete;
    auto operator=(const Service &) -> Service & = delete;

    auto start() -> std::int32_t;
    void stop() noexcept;
    [[nodiscard]] auto running() const -> bool;
    [[nodiscard]] auto instanceId() const -> std::string;
    [[nodiscard]] auto cards() const -> std::uint32_t;
    auto setCards(std::uint32_t mask) -> std::int32_t;
    [[nodiscard]] auto snapshot() -> std::unique_ptr<SnapshotData>;
    [[nodiscard]] auto history(unsigned windowSeconds) -> std::unique_ptr<HistoryData>;

private:
    void run(const std::stop_token &stop);
    void sample(bool includeDisks);

    Config config_;
    std::unique_ptr<Collector> collector_;
    std::string instanceId_;
    std::string cpuName_;
    std::string memorySpec_;
    std::uint32_t logicalCpus_{};

    mutable std::mutex mutex_;
    std::mutex lifecycleMutex_;
    std::condition_variable_any wake_;
    std::jthread worker_;
    bool running_{};
    std::deque<Sample> samples_;
    std::vector<DiskInfo> disks_;
    std::vector<NetworkInfo> networks_;
    std::uint32_t gpuCount_{};
    std::string gpuName_;
    mutable std::mutex cardsMutex_;
    CardFlags cards_;
};

} // namespace SERVICE
} // namespace SZX::METRICS

struct szx_metrics_service
{
    std::unique_ptr<SZX::METRICS::SERVICE::Service> impl;
};

struct szx_metrics_snapshot
{
    SZX::METRICS::SERVICE::SnapshotData data;
    mutable SZX::METRICS::DETAIL::StringArena arena;
};

struct szx_metrics_history
{
    SZX::METRICS::SERVICE::HistoryData data;
    mutable SZX::METRICS::DETAIL::StringArena arena;
};

namespace SZX::METRICS
{
namespace SERVICE
{

/** 单项采集的异常隔离：任何平台接口抛出都降级为 fallback。 */
template <typename Function, typename Fallback>
auto guarded(Function &&function, Fallback &&fallback) -> decltype(function())
{
    try
    {
        return function();
    } catch (...)
    {
        return fallback();
    }
}

auto Service::start() -> std::int32_t
{
    try
    {
        std::lock_guard lifecycle{lifecycleMutex_};
        if (worker_.joinable())
            return SZX_METRICS_INVALID_CONFIG;
        collector_   = DETAIL::createCollector();
        instanceId_  = SZX::COMMON::Uuid::v4().toString();
        logicalCpus_ = guarded([this] { return collector_->logicalCpus(); }, [] { return 0U; });
        cpuName_     = guarded([this] { return collector_->cpuName(); }, [] { return std::string{}; });
        memorySpec_  = guarded([this] { return collector_->memorySpec(); }, [] { return std::string{}; });
        if (const auto stored = loadCards(config_.statePath))
            config_.cards = CardFlags::fromMask(*stored);
        {
            std::lock_guard lock{cardsMutex_};
            cards_ = config_.cards;
        }
        {
            std::lock_guard lock{mutex_};
            samples_.clear();
            disks_.clear();
            networks_.clear();
            gpuCount_ = 0;
            gpuName_.clear();
        }
        sample(true);
        {
            std::lock_guard lock{mutex_};
            running_ = true;
        }
        worker_ = std::jthread([this](std::stop_token stop) { run(stop); });
        return SZX_METRICS_OK;
    } catch (...)
    {
        std::lock_guard lock{mutex_};
        running_ = false;
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
void Service::stop() noexcept
{
    try
    {
        std::lock_guard lifecycle{lifecycleMutex_};
        if (worker_.joinable())
        {
            worker_.request_stop();
            wake_.notify_all();
            worker_.join();
        }
        std::lock_guard lock{mutex_};
        running_ = false;
    } catch (...)
    { /* 析构路径不能传播异常 */
    }
}
auto Service::running() const -> bool
{
    std::lock_guard lock{mutex_};
    return running_;
}
auto Service::instanceId() const -> std::string
{
    std::lock_guard lock{mutex_};
    return running_ ? instanceId_ : std::string{};
}
auto Service::cards() const -> std::uint32_t
{
    std::lock_guard lock{cardsMutex_};
    return cards_.mask();
}
auto Service::setCards(std::uint32_t mask) -> std::int32_t
{
    const auto flags = CardFlags::fromMask(mask & SZX_METRICS_CARD_ALL);
    {
        std::lock_guard lock{cardsMutex_};
        cards_ = flags;
    }
    persistCards(config_.statePath, flags.mask());
    return SZX_METRICS_OK;
}
void Service::run(const std::stop_token &stop)
{
    auto nextDisk = std::chrono::steady_clock::now() + std::chrono::milliseconds(config_.diskIntervalMs);
    std::mutex waitMutex;
    try
    {
        while (!stop.stop_requested())
        {
            {
                std::unique_lock lock{waitMutex};
                // 四参重载是 condition_variable_any 配合 stop_token 的唯一跨平台形态
                wake_.wait_for(lock, stop, std::chrono::milliseconds(config_.sampleIntervalMs), [] { return false; });
            }
            if (stop.stop_requested())
                break;
            const auto now   = std::chrono::steady_clock::now();
            const bool disks = now >= nextDisk;
            sample(disks);
            if (disks)
                nextDisk = now + std::chrono::milliseconds(config_.diskIntervalMs);
        }
    } catch (...)
    {
        std::lock_guard lock{mutex_};
        running_ = false;
    }
}
void Service::sample(bool includeDisks)
{
    const auto flags = CardFlags::fromMask(cards());
    Sample next;
    next.sampledAt = SZX::COMMON::TimeHelper::NowTimestamp();

    if (flags.cpu)
    {
        next.cpu = guarded([this] { return collector_->cpu(); }, [] { return MetricValue{}; });
        if (next.cpu.state == SZX_METRICS_METRIC_OK)
        {
            const auto temperature =
                guarded([this] { return collector_->cpuTemperature(); }, [] { return std::optional<double>{}; });
            if (temperature)
            {
                next.cpu.present            |= SZX_METRICS_FIELD_TEMPERATURE;
                next.cpu.temperatureCelsius  = *temperature;
            }
        }
    }
    if (flags.memory)
        next.memory = guarded([this] { return collector_->memory(); }, [] { return MetricValue{}; });
    if (flags.gpu)
    {
        next.gpu      = guarded([this] { return collector_->gpu(0); }, [] { return MetricValue{}; });
        next.gpuExtra = guarded([this] { return collector_->gpuExtra(0); }, [] { return GpuExtraInfo{}; });
        // 任务管理器口径的引擎占用优先于 NVML 核占用率（WDDM 下后者对图形负载不敏感）
        if (next.gpuExtra.state == SZX_METRICS_METRIC_OK && next.gpuExtra.totalPercent)
        {
            next.gpu.state         = SZX_METRICS_METRIC_OK;
            next.gpu.present      |= SZX_METRICS_FIELD_USAGE;
            next.gpu.usagePercent  = *next.gpuExtra.totalPercent;
        }
    }
    if (flags.process && !config_.processNames.empty())
        next.processes = guarded([this] { return collector_->processes(config_.processNames, logicalCpus_); },
                                 [] { return std::vector<ProcessInfo>{}; });

    std::optional<std::vector<DiskInfo>> volumes;
    if (flags.disk && includeDisks)
        volumes = guarded([this, &next] { return collector_->disks(config_.diskPaths, next.sampledAt); },
                          [] { return std::vector<DiskInfo>{}; });
    std::optional<std::vector<NetworkInfo>> nics;
    if (flags.network)
        nics = guarded([this, &next] { return collector_->networks(next.sampledAt); }, [] { return std::vector<NetworkInfo>{}; });

    std::uint32_t gpuCount = 0;
    std::string gpuName;
    if (flags.gpu)
    {
        gpuCount = guarded([this] { return collector_->gpuCount(); }, [] { return 0U; });
        if (gpuCount)
            gpuName = guarded([this] { return collector_->gpuName(0); }, [] { return std::string{}; });
    }

    std::lock_guard lock{mutex_};
    next.sequence = samples_.empty() ? 1 : samples_.back().sequence + 1;
    samples_.push_back(std::move(next));
    while (samples_.size() > config_.historyCapacity)
        samples_.pop_front();
    if (volumes)
        disks_ = std::move(*volumes);
    if (nics)
        networks_ = std::move(*nics);
    gpuCount_ = gpuCount;
    gpuName_  = std::move(gpuName);
}
auto Service::snapshot() -> std::unique_ptr<SnapshotData>
{
    std::lock_guard lock{mutex_};
    if (!running_ || samples_.empty())
        return nullptr;
    const auto &latest       = samples_.back();
    auto result              = std::make_unique<SnapshotData>();
    result->sequence         = latest.sequence;
    result->sampledAt        = latest.sampledAt;
    result->instanceId       = instanceId_;
    result->platform         = collector_ ? collector_->platformName() : "unknown";
    result->cpuName          = cpuName_;
    result->memorySpec       = memorySpec_;
    result->logicalCpus      = logicalCpus_;
    result->sampleIntervalMs = config_.sampleIntervalMs;
    result->diskIntervalMs   = config_.diskIntervalMs;
    result->cards            = cards();
    result->cpu              = latest.cpu;
    result->memory           = latest.memory;
    result->gpu              = latest.gpu;
    result->gpuExtra         = latest.gpuExtra;
    result->gpuCount         = gpuCount_;
    result->gpuName          = gpuName_;
    result->disks            = disks_;
    result->networks         = networks_;
    result->processes        = latest.processes;
    return result;
}
auto Service::history(unsigned windowSeconds) -> std::unique_ptr<HistoryData>
{
    if (!windowSeconds || windowSeconds > SZX_METRICS_MAX_WINDOW_SECONDS)
        return nullptr;
    std::lock_guard lock{mutex_};
    if (!running_ || samples_.empty())
        return nullptr;
    auto result              = std::make_unique<HistoryData>();
    result->instanceId       = instanceId_;
    result->sampleIntervalMs = config_.sampleIntervalMs;
    result->windowSeconds    = windowSeconds;
    result->samples.reserve(samples_.size());
    const auto cutoff = samples_.back().sampledAt - static_cast<std::int64_t>(windowSeconds) * 1000;
    for (const auto &sample : samples_)
        if (sample.sampledAt >= cutoff)
            result->samples.push_back(sample);
    return result;
}

/* ---------------------------------------------------------------------- */
/* C ABI 转换助手                                                          */
/* ---------------------------------------------------------------------- */

namespace
{
using ABI::fill;

auto metricOf(const MetricValue &value) -> szx_metrics_metric
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
auto joinLines(const std::vector<std::string> &lines) -> std::string
{
    std::string result;
    for (const auto &line : lines)
    {
        if (!result.empty())
            result.push_back('\n');
        result += line;
    }
    return result;
}
/** 显存与占用两张表按同一顺序输出，索引因此稳定。 */
auto mergedNames(const GpuExtraInfo &extra) -> std::vector<std::string>
{
    std::vector<std::string> names;
    for (const auto &[name, unused] : extra.usageByProcess)
        names.push_back(name);
    for (const auto &[name, unused] : extra.vramByProcess)
        if (std::find(names.begin(), names.end(), name) == names.end())
            names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}
} // namespace

extern "C" {
SZX_METRICS_API std::uint32_t szx_metrics_abi_version(void)
{
    return SZX_METRICS_ABI_VERSION;
}
SZX_METRICS_API void szx_metrics_config_init(szx_metrics_config *config)
{
    if (config == nullptr)
        return;
    std::memset(config, 0, sizeof(*config));
    config->struct_size = SZX_METRICS_SIZE_OF(szx_metrics_config);
    config->flags       = SZX_METRICS_CARD_ALL;
}
SZX_METRICS_API std::int32_t szx_metrics_service_create(const szx_metrics_config *config, szx_metrics_service **out)
{
    if (out == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    *out = nullptr;
    try
    {
        Config parsed;
        if (config == nullptr)
        {
            szx_metrics_config defaults;
            szx_metrics_config_init(&defaults);
            if (!fromAbi(defaults, parsed))
                return SZX_METRICS_INVALID_CONFIG;
        }
        else if (!fromAbi(*config, parsed))
        {
            return SZX_METRICS_INVALID_CONFIG;
        }
        *out = new szx_metrics_service{std::make_unique<Service>(std::move(parsed))};
        return SZX_METRICS_OK;
    } catch (const std::bad_alloc &)
    {
        return SZX_METRICS_OUT_OF_MEMORY;
    } catch (...)
    {
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
SZX_METRICS_API void szx_metrics_service_destroy(szx_metrics_service *service)
{
    delete service;
}
SZX_METRICS_API std::int32_t szx_metrics_service_start(szx_metrics_service *service)
{
    return service && service->impl ? service->impl->start() : SZX_METRICS_INVALID_ARGUMENT;
}
SZX_METRICS_API void szx_metrics_service_stop(szx_metrics_service *service)
{
    if (service && service->impl)
        service->impl->stop();
}
SZX_METRICS_API std::int32_t szx_metrics_service_is_running(const szx_metrics_service *service)
{
    return service && service->impl && service->impl->running() ? 1 : 0;
}
SZX_METRICS_API const char *szx_metrics_service_instance_id(const szx_metrics_service *service)
{
    if (service == nullptr || !service->impl)
        return "";
    // 线程局部缓存：同线程下次调用前，返回指针保持有效。
    thread_local std::string cache;
    cache = service->impl->instanceId();
    return cache.c_str();
}
SZX_METRICS_API std::int32_t szx_metrics_service_snapshot(const szx_metrics_service *service, szx_metrics_snapshot **out)
{
    if (out == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    *out = nullptr;
    try
    {
        if (service == nullptr || !service->impl)
            return SZX_METRICS_INVALID_ARGUMENT;
        auto data = service->impl->snapshot();
        if (!data)
            return SZX_METRICS_NOT_RUNNING;
        *out = new szx_metrics_snapshot{std::move(*data)};
        return SZX_METRICS_OK;
    } catch (const std::bad_alloc &)
    {
        return SZX_METRICS_OUT_OF_MEMORY;
    } catch (...)
    {
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
SZX_METRICS_API void szx_metrics_snapshot_release(szx_metrics_snapshot *snapshot)
{
    delete snapshot;
}
SZX_METRICS_API std::uint64_t szx_metrics_snapshot_sequence(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->data.sequence : 0;
}
SZX_METRICS_API std::int64_t szx_metrics_snapshot_sampled_at(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->data.sampledAt : 0;
}
SZX_METRICS_API const char *szx_metrics_snapshot_instance_id(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->arena.intern(snapshot->data.instanceId) : "";
}
SZX_METRICS_API const char *szx_metrics_snapshot_platform(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->arena.intern(snapshot->data.platform) : "unknown";
}
SZX_METRICS_API const char *szx_metrics_snapshot_cpu_name(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->arena.intern(snapshot->data.cpuName) : "";
}
SZX_METRICS_API const char *szx_metrics_snapshot_memory_spec(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->arena.intern(snapshot->data.memorySpec) : "";
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_logical_cpus(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->data.logicalCpus : 0;
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_sample_interval_ms(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->data.sampleIntervalMs : 0;
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_disk_interval_ms(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->data.diskIntervalMs : 0;
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_cards(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->data.cards : 0;
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_cpu(const szx_metrics_snapshot *snapshot, szx_metrics_metric *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    return fill(out, ABI::kMetricPrefixSize, metricOf(snapshot->data.cpu));
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_memory(const szx_metrics_snapshot *snapshot, szx_metrics_metric *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    return fill(out, ABI::kMetricPrefixSize, metricOf(snapshot->data.memory));
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_gpu(const szx_metrics_snapshot *snapshot, szx_metrics_metric *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    return fill(out, ABI::kMetricPrefixSize, metricOf(snapshot->data.gpu));
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_gpu_extra(const szx_metrics_snapshot *snapshot, szx_metrics_gpu_extra *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    const auto &extra = snapshot->data.gpuExtra;
    szx_metrics_gpu_extra result{};
    result.struct_size         = SZX_METRICS_SIZE_OF(szx_metrics_gpu_extra);
    result.state               = extra.state;
    result.total_percent       = extra.totalPercent.value_or(0.0);
    result.cuda_percent        = extra.cudaPercent.value_or(0.0);
    result.graphics_3d_percent = extra.graphics3dPercent.value_or(0.0);
    result.present = (extra.totalPercent ? 1U : 0U) | (extra.cudaPercent ? 2U : 0U) | (extra.graphics3dPercent ? 4U : 0U);
    return fill(out, ABI::kGpuExtraSize, result);
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_gpu_count(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->data.gpuCount : 0;
}
SZX_METRICS_API const char *szx_metrics_snapshot_gpu_name(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? snapshot->arena.intern(snapshot->data.gpuName) : "";
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_gpu_process_count(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? static_cast<std::uint32_t>(mergedNames(snapshot->data.gpuExtra).size()) : 0;
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_gpu_process_at(const szx_metrics_snapshot *snapshot, std::uint32_t index,
                                                                 szx_metrics_named_value *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    const auto names = mergedNames(snapshot->data.gpuExtra);
    if (index >= names.size())
        return SZX_METRICS_OUT_OF_RANGE;
    const auto &extra = snapshot->data.gpuExtra;
    szx_metrics_named_value result{};
    result.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_named_value);
    result.name        = snapshot->arena.intern(names[index]);
    if (const auto found = extra.vramByProcess.find(names[index]); found != extra.vramByProcess.end())
        result.value = static_cast<double>(found->second);
    else if (const auto usage = extra.usageByProcess.find(names[index]); usage != extra.usageByProcess.end())
        result.value = usage->second;
    return fill(out, ABI::kNamedValueSize, result);
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_disk_count(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? static_cast<std::uint32_t>(snapshot->data.disks.size()) : 0;
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_disk_at(const szx_metrics_snapshot *snapshot, std::uint32_t index,
                                                          szx_metrics_disk *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    if (index >= snapshot->data.disks.size())
        return SZX_METRICS_OUT_OF_RANGE;
    const auto &disk = snapshot->data.disks[index];
    szx_metrics_disk result{};
    result.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_disk);
    result.id          = snapshot->arena.intern(disk.id);
    result.path        = snapshot->arena.intern(disk.path);
    result.type        = snapshot->arena.intern(disk.type);
    result.sampled_at  = disk.sampledAt;
    result.capacity    = metricOf(disk.capacity);
    return fill(out, ABI::kDiskSize, result);
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_network_count(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? static_cast<std::uint32_t>(snapshot->data.networks.size()) : 0;
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_network_at(const szx_metrics_snapshot *snapshot, std::uint32_t index,
                                                             szx_metrics_network *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    if (index >= snapshot->data.networks.size())
        return SZX_METRICS_OUT_OF_RANGE;
    const auto &nic = snapshot->data.networks[index];
    szx_metrics_network result{};
    result.struct_size      = SZX_METRICS_SIZE_OF(szx_metrics_network);
    result.id               = snapshot->arena.intern(nic.id);
    result.name             = snapshot->arena.intern(nic.name);
    result.description      = snapshot->arena.intern(nic.description);
    result.ipv4             = snapshot->arena.intern(joinLines(nic.ipv4));
    result.ipv6             = snapshot->arena.intern(joinLines(nic.ipv6));
    result.sampled_at       = nic.sampledAt;
    result.rx_bytes_per_sec = nic.rxBytesPerSec.value_or(0.0);
    result.tx_bytes_per_sec = nic.txBytesPerSec.value_or(0.0);
    result.link_mbps        = nic.linkMbps.value_or(0.0);
    result.present          = (nic.rxBytesPerSec ? 1U : 0U) | (nic.txBytesPerSec ? 2U : 0U) | (nic.linkMbps ? 4U : 0U);
    return fill(out, ABI::kNetworkSize, result);
}
SZX_METRICS_API std::uint32_t szx_metrics_snapshot_process_count(const szx_metrics_snapshot *snapshot)
{
    return snapshot ? static_cast<std::uint32_t>(snapshot->data.processes.size()) : 0;
}
SZX_METRICS_API std::int32_t szx_metrics_snapshot_process_at(const szx_metrics_snapshot *snapshot, std::uint32_t index,
                                                             szx_metrics_process *out)
{
    if (snapshot == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    if (index >= snapshot->data.processes.size())
        return SZX_METRICS_OUT_OF_RANGE;
    const auto &process = snapshot->data.processes[index];
    szx_metrics_process result{};
    result.struct_size  = SZX_METRICS_SIZE_OF(szx_metrics_process);
    result.state        = process.state;
    result.name         = snapshot->arena.intern(process.name);
    result.cpu_percent  = process.cpuPercent;
    result.memory_bytes = process.memoryBytes;
    return fill(out, ABI::kProcessSize, result);
}
SZX_METRICS_API std::int32_t szx_metrics_service_history(const szx_metrics_service *service, std::uint32_t windowSeconds,
                                                         szx_metrics_history **out)
{
    if (out == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    *out = nullptr;
    try
    {
        if (service == nullptr || !service->impl)
            return SZX_METRICS_INVALID_ARGUMENT;
        if (!windowSeconds || windowSeconds > SZX_METRICS_MAX_WINDOW_SECONDS)
            return SZX_METRICS_OUT_OF_RANGE;
        auto data = service->impl->history(windowSeconds);
        if (!data)
            return SZX_METRICS_NOT_RUNNING;
        *out = new szx_metrics_history{std::move(*data)};
        return SZX_METRICS_OK;
    } catch (const std::bad_alloc &)
    {
        return SZX_METRICS_OUT_OF_MEMORY;
    } catch (...)
    {
        return SZX_METRICS_INTERNAL_ERROR;
    }
}
SZX_METRICS_API void szx_metrics_history_release(szx_metrics_history *history)
{
    delete history;
}
SZX_METRICS_API const char *szx_metrics_history_instance_id(const szx_metrics_history *history)
{
    return history ? history->arena.intern(history->data.instanceId) : "";
}
SZX_METRICS_API std::uint32_t szx_metrics_history_sample_interval_ms(const szx_metrics_history *history)
{
    return history ? history->data.sampleIntervalMs : 0;
}
SZX_METRICS_API std::uint32_t szx_metrics_history_window_seconds(const szx_metrics_history *history)
{
    return history ? history->data.windowSeconds : 0;
}
SZX_METRICS_API std::uint32_t szx_metrics_history_count(const szx_metrics_history *history)
{
    return history ? static_cast<std::uint32_t>(history->data.samples.size()) : 0;
}
SZX_METRICS_API std::int32_t szx_metrics_history_sample_at(const szx_metrics_history *history, std::uint32_t index,
                                                           szx_metrics_sample *out)
{
    if (history == nullptr)
        return SZX_METRICS_INVALID_ARGUMENT;
    if (index >= history->data.samples.size())
        return SZX_METRICS_OUT_OF_RANGE;
    const auto &sample = history->data.samples[index];
    szx_metrics_sample result{};
    result.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_sample);
    result.sequence    = sample.sequence;
    result.sampled_at  = sample.sampledAt;
    result.cpu         = metricOf(sample.cpu);
    result.memory      = metricOf(sample.memory);
    result.gpu         = metricOf(sample.gpu);
    return fill(out, ABI::kSampleSize, result);
}
SZX_METRICS_API std::uint32_t szx_metrics_service_cards(const szx_metrics_service *service)
{
    return service && service->impl ? service->impl->cards() : SZX_METRICS_CARD_ALL;
}
SZX_METRICS_API std::int32_t szx_metrics_service_set_cards(szx_metrics_service *service, std::uint32_t flags)
{
    return service && service->impl ? service->impl->setCards(flags) : SZX_METRICS_INVALID_ARGUMENT;
}
} // extern "C"
} // namespace SERVICE
} // namespace SZX::METRICS
