#include "SystemMetrics.h"

#include "FileHelper.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <pdh.h>
    #include <windows.h>
#else
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <unistd.h>
#endif

namespace SZX::ADMIN::SYSTEM
{
inline namespace v1
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Json;
using SZX::COMMON::Result;
template <typename T>
auto failure(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}
template <typename T>
auto nullable(const std::optional<T> &value) -> Json
{
    return value ? Json(*value) : Json(nullptr);
}
auto metricJson(const Metric &value) -> Json
{
    return Json{
        {"status", value.status},
        {"error_code", value.status == "unavailable" ? Json(static_cast<int>(ErrorCode::ServiceUnavailable)) : Json(nullptr)},
        {"usage_percent", nullable(value.usagePercent)},
        {"total_bytes", nullable(value.totalBytes)},
        {"used_bytes", nullable(value.usedBytes)},
        {"available_bytes", nullable(value.availableBytes)},
        {"free_bytes", nullable(value.freeBytes)}};
}
auto sampleJson(const MetricsSample &value) -> Json
{
    return Json{{"sequence", value.sequence},
                {"sampled_at", value.sampledAt},
                {"cpu", metricJson(value.cpu)},
                {"memory", metricJson(value.memory)}};
}
#ifndef _WIN32
auto unescapeMount(std::string value) -> std::string
{
    for (std::size_t i = 0; i + 3 < value.size(); ++i)
        if (value[i] == '\\' && value[i + 1] >= '0' && value[i + 1] <= '7' && value[i + 2] >= '0' && value[i + 2] <= '7' &&
            value[i + 3] >= '0' && value[i + 3] <= '7')
            value.replace(i, 4, 1, static_cast<char>((value[i + 1] - '0') * 64 + (value[i + 2] - '0') * 8 + value[i + 3] - '0'));
    return value;
}
#endif
} // namespace

auto parseCpuCounters(std::string_view text) -> std::optional<CpuCounters>
{
    std::istringstream input{std::string(text.substr(0, text.find('\n')))};
    std::string label;
    std::array<std::uint64_t, 8> ticks{};
    if (!(input >> label) || label != "cpu")
        return std::nullopt;
    for (auto &tick : ticks)
        if (!(input >> tick))
            return std::nullopt;
    CpuCounters result;
    for (auto tick : ticks)
    {
        if (tick > std::numeric_limits<std::uint64_t>::max() - result.total)
            return std::nullopt;
        result.total += tick;
    }
    result.idle = ticks[3] + ticks[4]; // idle + iowait; guest is already included in user/nice.
    return result;
}
auto cpuUsage(CpuCounters previous, CpuCounters current) noexcept -> std::optional<double>
{
    if (current.total <= previous.total || current.idle < previous.idle)
        return std::nullopt;
    const auto total = current.total - previous.total;
    const auto idle  = current.idle - previous.idle;
    if (idle > total)
        return std::nullopt;
    return 100.0 * static_cast<double>(total - idle) / static_cast<double>(total);
}
auto capacityMetric(std::uint64_t total, std::uint64_t free, std::uint64_t available) -> Metric
{
    if (!total || free > total || available > free)
        return {};
    Metric result;
    result.status         = "ok";
    result.totalBytes     = total;
    result.usedBytes      = total - free;
    result.availableBytes = available;
    result.freeBytes      = free;
    result.usagePercent   = 100.0 * static_cast<double>(total - free) / static_cast<double>(total);
    return result;
}
auto parseMemory(std::string_view text) -> Metric
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
    result.freeBytes.reset(); // available includes reclaimable cache, not raw MemFree.
    return result;
}

class SystemMetricsService::Platform final
{
public:
#ifdef _WIN32
    PDH_HQUERY query{};
    PDH_HCOUNTER counter{};
    bool primed{};
    ~Platform()
    {
        if (query)
            PdhCloseQuery(query);
    }
#else
    std::optional<CpuCounters> previous;
#endif
    auto cpu() -> Metric
    {
        Metric result;
#ifdef _WIN32
        if (!query)
        {
            if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS)
                return result;
            if (PdhAddEnglishCounterW(query, L"\\Processor Information(_Total)\\% Processor Time", 0, &counter) != ERROR_SUCCESS)
            {
                PdhCloseQuery(query);
                query = nullptr;
                return result;
            }
        }
        if (PdhCollectQueryData(query) != ERROR_SUCCESS)
        {
            primed = false;
            return result;
        }
        if (!std::exchange(primed, true))
        {
            result.status = "warming_up";
            return result;
        }
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) != ERROR_SUCCESS ||
            (value.CStatus != PDH_CSTATUS_VALID_DATA && value.CStatus != PDH_CSTATUS_NEW_DATA) ||
            !std::isfinite(value.doubleValue) || value.doubleValue < 0 || value.doubleValue > 100)
        {
            primed = false;
            return result;
        }
        result.usagePercent = value.doubleValue;
#else
        const auto text    = SZX::COMMON::FileHelper::ReadText("/proc/stat");
        const auto current = text ? parseCpuCounters(*text) : std::nullopt;
        if (!current)
        {
            previous.reset();
            return result;
        }
        if (previous)
            result.usagePercent = cpuUsage(*previous, *current);
        previous = current;
        if (!result.usagePercent)
        {
            result.status = "warming_up";
            return result;
        }
#endif
        result.status = "ok";
        return result;
    }
    auto memory() -> Metric
    {
#ifdef _WIN32
        MEMORYSTATUSEX data{};
        data.dwLength = sizeof(data);
        if (!GlobalMemoryStatusEx(&data))
            return {};
        auto result = capacityMetric(data.ullTotalPhys, data.ullAvailPhys, data.ullAvailPhys);
        result.freeBytes.reset();
        return result;
#else
        const auto text = SZX::COMMON::FileHelper::ReadText("/proc/meminfo");
        return text ? parseMemory(*text) : Metric{};
#endif
    }
    auto disks(const MetricsConfig &config, std::int64_t now) -> std::vector<DiskMetric>
    {
        std::vector<std::string> paths;
#ifdef _WIN32
        if (config.diskPaths.empty())
        {
            std::array<wchar_t, 512> drives{};
            const auto count = GetLogicalDriveStringsW(static_cast<DWORD>(drives.size()), drives.data());
            if (count && count < drives.size())
                for (const wchar_t *drive = drives.data(); *drive; drive += wcslen(drive) + 1)
                    if (GetDriveTypeW(drive) == DRIVE_FIXED)
                        paths.push_back(std::filesystem::path(drive).string());
        }
        else
            paths = config.diskPaths;
        std::set<std::wstring> seen;
        std::vector<DiskMetric> result;
        std::set<std::string> configuredPaths;
        for (const auto &path : paths)
        {
            if (!configuredPaths.insert(path).second)
                continue;
            if (result.size() >= 32)
                break;
            const auto native = std::filesystem::u8path(path).wstring();
            std::array<wchar_t, MAX_PATH> volume{};
            DiskMetric disk{path, path, now, {}};
            if (!GetVolumePathNameW(native.c_str(), volume.data(), static_cast<DWORD>(volume.size())) ||
                GetDriveTypeW(volume.data()) != DRIVE_FIXED)
            {
                result.push_back(std::move(disk));
                continue;
            }
            if (!seen.insert(volume.data()).second)
                continue;
            disk.id = disk.path = std::filesystem::path(volume.data()).string();
            ULARGE_INTEGER available{}, total{}, free{};
            if (GetDiskFreeSpaceExW(volume.data(), &available, &total, &free))
                disk.capacity = capacityMetric(total.QuadPart, free.QuadPart, available.QuadPart);
            result.push_back(std::move(disk));
        }
#else
        // Only known local filesystems: never probe network/pseudo mounts from an HTTP request.
        const auto mountText = SZX::COMMON::FileHelper::ReadText("/proc/self/mountinfo");
        struct Mount
        {
            std::string path;
            bool local;
        };
        std::vector<Mount> mounts;
        if (mountText)
        {
            std::istringstream lines{*mountText};
            std::string line;
            const std::set<std::string> local{"ext2",    "ext3", "ext4", "xfs",   "btrfs", "zfs",
                                              "overlay", "f2fs", "vfat", "exfat", "ntfs3"};
            while (std::getline(lines, line))
            {
                const auto separator = line.find(" - ");
                if (separator == std::string::npos)
                    continue;
                std::istringstream left{line.substr(0, separator)}, right{line.substr(separator + 3)};
                std::string id, parent, device, root, path, fs;
                if (left >> id >> parent >> device >> root >> path && right >> fs)
                    mounts.push_back({unescapeMount(path), local.contains(fs)});
            }
        }
        if (config.diskPaths.empty())
        {
            for (const auto &mount : mounts)
                if (mount.local)
                    paths.push_back(mount.path);
        }
        else
            paths = config.diskPaths;
        std::set<dev_t> seen;
        std::vector<DiskMetric> result;
        std::set<std::string> configuredPaths;
        for (const auto &path : paths)
        {
            if (!configuredPaths.insert(path).second)
                continue;
            if (result.size() >= 32)
                break;
            const Mount *owner = nullptr;
            for (const auto &mount : mounts)
                if ((path == mount.path || mount.path == "/" || path.starts_with(mount.path + "/")) &&
                    (!owner || mount.path.size() > owner->path.size()))
                    owner = &mount;
            DiskMetric disk{path, path, now, {}};
            if (!owner || !owner->local)
            {
                if (!config.diskPaths.empty())
                    result.push_back(std::move(disk));
                continue;
            }
            struct stat info{};
            if (::stat(path.c_str(), &info) != 0)
            {
                result.push_back(std::move(disk));
                continue;
            }
            if (!seen.insert(info.st_dev).second)
                continue;
            disk.id   = std::to_string(info.st_dev);
            disk.path = owner->path;
            struct statvfs data{};
            if (::statvfs(path.c_str(), &data) == 0 && data.f_frsize &&
                data.f_blocks <= std::numeric_limits<std::uint64_t>::max() / data.f_frsize && data.f_bfree <= data.f_blocks &&
                data.f_bavail <= data.f_bfree)
                disk.capacity =
                    capacityMetric(data.f_blocks * data.f_frsize, data.f_bfree * data.f_frsize, data.f_bavail * data.f_frsize);
            result.push_back(std::move(disk));
        }
#endif
        return result;
    }
};

SystemMetricsService::SystemMetricsService(MetricsConfig config) :
    config_(std::move(config))
{
}
SystemMetricsService::~SystemMetricsService()
{
    stop();
}
auto SystemMetricsService::start() -> Result<void>
{
    try
    {
        std::lock_guard lifecycle{lifecycleMutex_};
        if (worker_.joinable())
            return failure<void>(ErrorCode::InvalidConfiguration);
        if (config_.diskPaths.size() > 32)
            return failure<void>(ErrorCode::InvalidConfiguration);
        for (const auto &path : config_.diskPaths)
            if (path.empty() || !std::filesystem::u8path(path).is_absolute() || path.size() > 4096 ||
                path.find('\0') != std::string::npos)
                return failure<void>(ErrorCode::InvalidConfiguration);
        platform_   = std::make_unique<Platform>();
        instanceId_ = SZX::COMMON::Uuid::v4().toString();
#ifdef _WIN32
        logicalCpus_ = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
        const auto count = sysconf(_SC_NPROCESSORS_ONLN);
        logicalCpus_     = count > 0 ? static_cast<unsigned>(count) : 0;
#endif
        {
            std::lock_guard lock{mutex_};
            samples_.clear();
            disks_.clear();
        }
        sample(true);
        {
            std::lock_guard lock{mutex_};
            running_ = true;
        }
        worker_ = std::jthread(
            [this](std::stop_token stop)
            {
                try
                {
                    auto nextDisk = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                    std::mutex waitMutex;
                    while (!stop.stop_requested())
                    {
                        std::unique_lock lock{waitMutex};
                        wake_.wait_for(lock, stop, std::chrono::seconds(2), [] { return false; });
                        if (stop.stop_requested())
                            break;
                        const auto now   = std::chrono::steady_clock::now();
                        const bool disks = now >= nextDisk;
                        sample(disks);
                        if (disks)
                            nextDisk = now + std::chrono::seconds(30);
                    }
                } catch (...)
                {
                    std::lock_guard lock{mutex_};
                    running_ = false;
                }
            });
        return Result<void>::Success();
    } catch (...)
    {
        std::lock_guard lock{mutex_};
        running_ = false;
        return failure<void>(ErrorCode::ServiceUnavailable);
    }
}
void SystemMetricsService::stop() noexcept
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
    { /* destructor must not propagate */
    }
}
void SystemMetricsService::sample(bool disks)
{
    MetricsSample next;
    next.sampledAt = SZX::COMMON::TimeHelper::NowTimestamp();
    try
    {
        next.cpu = platform_->cpu();
    } catch (...)
    {
        next.cpu = {};
    }
    try
    {
        next.memory = platform_->memory();
    } catch (...)
    {
        next.memory = {};
    }
    std::optional<std::vector<DiskMetric>> volumes;
    if (disks)
    {
        try
        {
            volumes = platform_->disks(config_, next.sampledAt);
        } catch (...)
        { /* preserve last disk timestamp: frontend marks it stale */
        }
    }
    std::lock_guard lock{mutex_};
    next.sequence = samples_.empty() ? 1 : samples_.back().sequence + 1;
    samples_.push_back(std::move(next));
    while (samples_.size() > 450)
        samples_.pop_front();
    if (volumes)
        disks_ = std::move(*volumes);
}
auto SystemMetricsService::snapshot() const -> Result<Json>
{
    try
    {
        std::lock_guard lock{mutex_};
        if (!running_ || samples_.empty())
            return failure<Json>(ErrorCode::ServiceUnavailable);
        auto result            = sampleJson(samples_.back());
        result["instance_id"]  = instanceId_;
        result["logical_cpus"] = logicalCpus_;
#ifdef _WIN32
        result["platform"] = "windows";
#else
        result["platform"] = "linux";
#endif
        result["sample_interval_ms"] = 2000;
        result["disk_interval_ms"]   = 30000;
        result["disks"]              = Json::array();
        for (const auto &disk : disks_)
            result["disks"].push_back(Json{
                {"id", disk.id}, {"path", disk.path}, {"sampled_at", disk.sampledAt}, {"capacity", metricJson(disk.capacity)}});
        return Result<Json>::Success(std::move(result));
    } catch (...)
    {
        return failure<Json>(ErrorCode::InternalFailure);
    }
}
auto SystemMetricsService::history(unsigned windowSeconds) const -> Result<Json>
{
    try
    {
        if (!windowSeconds || windowSeconds > 900)
            return failure<Json>(ErrorCode::ValidationError);
        std::lock_guard lock{mutex_};
        if (!running_ || samples_.empty())
            return failure<Json>(ErrorCode::ServiceUnavailable);
        Json items        = Json::array();
        const auto cutoff = samples_.back().sampledAt - static_cast<std::int64_t>(windowSeconds) * 1000;
        for (const auto &sample : samples_)
            if (sample.sampledAt >= cutoff)
                items.push_back(sampleJson(sample));
        return Result<Json>::Success(Json{{"instance_id", instanceId_},
                                          {"sample_interval_ms", 2000},
                                          {"window_seconds", windowSeconds},
                                          {"samples", std::move(items)}});
    } catch (...)
    {
        return failure<Json>(ErrorCode::InternalFailure);
    }
}
} // namespace v1
} // namespace SZX::ADMIN::SYSTEM
