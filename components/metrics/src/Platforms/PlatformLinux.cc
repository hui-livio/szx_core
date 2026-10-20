/** @file PlatformLinux.cc
 * @brief Linux 采集器：/proc、/sys、statvfs、getifaddrs、NVML。
 * @details 与 Windows 版本实现同一 DETAIL::Collector 接口，公共层不感知差异。
 *          原则：只读本地伪文件系统，不发起网络请求，不扫描网络挂载点。
 */

#include "MetricsInternal.h"

#include "FileHelper.hpp"
#include "StringHelper.hpp"
#include "TimeHelper.hpp"

#include "../Platforms/NvmlLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

namespace SZX::METRICS::DETAIL
{
inline namespace v1
{
namespace
{
namespace fs = std::filesystem;

/** 只认本地文件系统：网络与伪文件系统不探测，避免请求期卡住。 */
constexpr std::array<std::string_view, 12> kLocalFileSystems{"ext2",    "ext3", "ext4", "xfs",   "btrfs", "zfs",
                                                             "overlay", "f2fs", "vfat", "exfat", "ntfs3", "bcachefs"};
/** 不展示的虚拟网口前缀。 */
constexpr std::array<std::string_view, 6> kVirtualInterfaces{"lo", "docker", "veth", "br-", "virbr", "tun"};

auto isLocalFileSystem(std::string_view type) -> bool
{
    return std::find(kLocalFileSystems.begin(), kLocalFileSystems.end(), type) != kLocalFileSystems.end();
}
auto isVirtualInterface(std::string_view name) -> bool
{
    return std::any_of(kVirtualInterfaces.begin(), kVirtualInterfaces.end(), [name](std::string_view prefix)
                       { return name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix; });
}
/** 反转 mountinfo 的八进制转义（空格写作 \040）。 */
auto unescapeMount(std::string value) -> std::string
{
    for (std::size_t i = 0; i + 3 < value.size(); ++i)
        if (value[i] == '\\' && value[i + 1] >= '0' && value[i + 1] <= '7' && value[i + 2] >= '0' && value[i + 2] <= '7' &&
            value[i + 3] >= '0' && value[i + 3] <= '7')
            value.replace(i, 4, 1, static_cast<char>((value[i + 1] - '0') * 64 + (value[i + 2] - '0') * 8 + value[i + 3] - '0'));
    return value;
}
auto trim(std::string_view text) -> std::string_view
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return text;
}
auto readText(const char *path) -> std::optional<std::string>
{
    return SZX::COMMON::FileHelper::ReadText(fs::path(path));
}
/** 读第一行的十进制整数。 */
auto readNumber(const std::string &path) -> std::optional<long long>
{
    const auto text = readText(path.c_str());
    if (!text)
        return std::nullopt;
    long long value         = 0;
    const auto line         = trim(*text);
    const auto [end, error] = std::from_chars(line.data(), line.data() + line.size(), value);
    if (error != std::errc{} || end != line.data() + line.size())
        return std::nullopt;
    return value;
}

struct MountEntry final
{
    std::string path;
    std::string type;
    bool local{};
};

/** 解析 /proc/self/mountinfo：left | " - " | fstype source superopts。 */
auto readMounts() -> std::vector<MountEntry>
{
    std::vector<MountEntry> result;
    const auto text = readText("/proc/self/mountinfo");
    if (!text)
        return result;
    std::istringstream lines{*text};
    std::string line;
    while (std::getline(lines, line))
    {
        const auto separator = line.find(" - ");
        if (separator == std::string::npos)
            continue;
        std::istringstream left{line.substr(0, separator)};
        std::istringstream right{line.substr(separator + 3)};
        std::string id, parent, device, root, path, options, fs;
        if (!(left >> id >> parent >> device >> root >> path >> options) || !(right >> fs))
            continue;
        result.push_back({unescapeMount(path), fs, isLocalFileSystem(fs)});
    }
    return result;
}
/** 介质类型：rotational=0 为 SSD，1 为 HDD；能看出 NVMe 时附上总线。 */
auto diskType(const std::string &path) -> std::string
{
    struct stat info{};
    if (::stat(path.c_str(), &info) != 0)
        return "未知类型";
    const std::string sys = "/sys/dev/block/" + std::to_string(major(info.st_dev)) + ":" + std::to_string(minor(info.st_dev));
    std::error_code error;
    const auto target        = fs::read_symlink(sys, error);
    const std::string device = target.empty() ? std::string{} : target.filename().string();
    const bool nvme          = device.find("nvme") != std::string::npos;
    const bool mmc           = device.find("mmc") != std::string::npos;

    std::string media = "未知介质";
    if (const auto rotational = readNumber(sys + "/queue/rotational"))
        media = *rotational == 0 ? "SSD" : "HDD";
    if (mmc)
        return "eMMC";
    if (nvme)
        return media + "(NVMe)";
    return media;
}
/** CPU 温度：优先 coretemp / k10temp 等 CPU 传感器，取最高读数。 */
auto readCpuTemperature() -> std::optional<double>
{
    std::error_code error;
    std::optional<double> hottest;
    for (const auto &entry : fs::directory_iterator("/sys/class/hwmon", error))
    {
        const auto name = SZX::COMMON::FileHelper::ReadText(entry.path() / "name");
        if (!name)
            continue;
        const auto chip = trim(*name);
        if (chip != "coretemp" && chip != "k10temp" && chip != "zenpower" && chip != "cpu_thermal" && chip != "acpitz")
            continue;
        for (const auto &file : fs::directory_iterator(entry.path(), error))
        {
            const auto filename = file.path().filename().string();
            if (filename.rfind("temp", 0) != 0 || filename.size() < 11 || filename.compare(filename.size() - 6, 6, "_input") != 0)
                continue;
            const auto milli = readNumber(file.path().string());
            if (!milli || *milli <= 0 || *milli > 150000)
                continue;
            const auto degrees = static_cast<double>(*milli) / 1000.0;
            if (!hottest || degrees > *hottest)
                hottest = degrees;
        }
    }
    return hottest;
}
/** /proc/stat 原文的首行计数，用于差分 CPU。 */
auto readCpuCounters() -> std::optional<CpuCountersRaw>
{
    const auto text = readText("/proc/stat");
    return text ? parseCpuCounters(*text) : std::nullopt;
}
/** /proc/net/dev 的累计收发字节，按网口名归档。 */
auto readNetworkCounters() -> std::map<std::string, std::pair<std::uint64_t, std::uint64_t>>
{
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> result;
    const auto text = readText("/proc/net/dev");
    if (!text)
        return result;
    std::istringstream lines{*text};
    std::string line;
    for (unsigned index = 0; std::getline(lines, line); ++index)
    {
        if (index < 2)
            continue; // 两行表头
        const auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        const auto name = std::string(trim(std::string_view(line).substr(0, colon)));
        std::istringstream fields{line.substr(colon + 1)};
        std::array<std::uint64_t, 16> values{};
        bool complete = true;
        for (auto &value : values)
            if (!(fields >> value))
            {
                complete = false;
                break;
            }
        if (!complete)
            continue;
        result[name] = {values[0], values[8]};
    }
    return result;
}
/** 进程名（comm，截断到 15 字符，因此长名与 .exe 后缀需要前缀匹配）。 */
auto processName(std::string_view pid) -> std::string
{
    const auto text = readText(("/proc/" + std::string(pid) + "/comm").c_str());
    if (!text)
        return {};
    std::string name = SZX::COMMON::StringHelper::ToLower(trim(*text));
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".exe") == 0)
        name.resize(name.size() - 4);
    return name;
}
auto processMatches(std::string_view name, std::string_view watched) -> bool
{
    return name == watched || (name.size() < watched.size() && watched.substr(0, name.size()) == name);
}
/** 读 /proc/<pid>/stat 的 utime+stime（字段 14/15）与常驻内存。 */
struct ProcessTicks final
{
    std::uint64_t ticks{};
    std::uint64_t residentBytes{};
};
auto readProcessTicks(std::string_view pid) -> std::optional<ProcessTicks>
{
    const auto text = readText(("/proc/" + std::string(pid) + "/stat").c_str());
    if (!text)
        return std::nullopt;
    // comm 字段可能含空格与括号，从最后一个 ')' 之后开始数
    const auto close = text->rfind(')');
    if (close == std::string::npos)
        return std::nullopt;
    std::istringstream fields{text->substr(close + 1)};
    std::string state;
    std::uint64_t utime = 0;
    std::uint64_t stime = 0;
    std::uint64_t value = 0;
    if (!(fields >> state))
        return std::nullopt;
    for (unsigned field = 4; field <= 15; ++field)
    {
        if (!(fields >> value))
            return std::nullopt;
        if (field == 14)
            utime = value;
        else if (field == 15)
            stime = value;
    }
    ProcessTicks result;
    result.ticks = utime + stime;
    if (const auto status = readText(("/proc/" + std::string(pid) + "/status").c_str()))
    {
        std::istringstream lines{*status};
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.rfind("VmRSS:", 0) != 0)
                continue;
            std::istringstream parts{line.substr(6)};
            std::uint64_t kilobytes = 0;
            if (parts >> kilobytes)
                result.residentBytes = kilobytes * 1024;
        }
    }
    return result;
}
auto longTickFrequency() -> double
{
    const long ticks = ::sysconf(_SC_CLK_TCK);
    return ticks > 0 ? static_cast<double>(ticks) : 100.0;
}

/** Linux 采集器。 */
class LinuxCollector final : public Collector
{
public:
    [[nodiscard]] auto platformName() const -> const char * override
    {
        return "linux";
    }
    [[nodiscard]] auto logicalCpus() const -> std::uint32_t override
    {
        const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
        return count > 0 ? static_cast<std::uint32_t>(count) : 0U;
    }
    [[nodiscard]] auto cpuName() const -> std::string override
    {
        const auto text = readText("/proc/cpuinfo");
        if (!text)
            return {};
        std::istringstream lines{*text};
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.rfind("model name", 0) != 0)
                continue;
            const auto colon = line.find(':');
            if (colon != std::string::npos)
                return std::string(trim(std::string_view(line).substr(colon + 1)));
        }
        return {};
    }
    [[nodiscard]] auto memorySpec() const -> std::string override
    {
        const auto text = readText("/proc/meminfo");
        if (!text)
            return {};
        std::istringstream lines{*text};
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.rfind("MemTotal:", 0) != 0)
                continue;
            std::istringstream parts{line.substr(9)};
            std::uint64_t kilobytes = 0;
            if (!(parts >> kilobytes))
                return {};
            char buffer[64]{};
            std::snprintf(buffer, sizeof(buffer), "%.1f GB", static_cast<double>(kilobytes) / (1024.0 * 1024.0));
            return buffer;
        }
        return {};
    }
    [[nodiscard]] auto cpuTemperature() -> std::optional<double> override
    {
        return readCpuTemperature();
    }
    [[nodiscard]] auto cpu() -> MetricValue override
    {
        MetricValue result;
        const auto current = readCpuCounters();
        if (!current)
        {
            previousCpu_.reset();
            return result;
        }
        if (previousCpu_)
        {
            if (const auto usage = cpuUsage(*previousCpu_, *current))
            {
                result.state        = SZX_METRICS_METRIC_OK;
                result.present      = SZX_METRICS_FIELD_USAGE;
                result.usagePercent = *usage;
            }
        }
        previousCpu_ = current;
        if (result.state != SZX_METRICS_METRIC_OK)
            result.state = SZX_METRICS_METRIC_WARMING_UP;
        return result;
    }
    [[nodiscard]] auto memory() -> MetricValue override
    {
        const auto text = readText("/proc/meminfo");
        return text ? parseMemory(*text) : MetricValue{};
    }
    [[nodiscard]] auto disks(const std::vector<std::string> &configured, std::int64_t sampledAt) -> std::vector<DiskInfo> override
    {
        // 先读一次挂载表：配置了路径时要靠它判断该路径是否落在本地卷上
        const auto mounts = readMounts();
        std::vector<std::string> paths;
        if (configured.empty())
        {
            for (const auto &mount : mounts)
                if (mount.local)
                    paths.push_back(mount.path);
        }
        else
        {
            paths = configured;
        }
        std::set<dev_t> seen;
        std::set<std::string> configuredPaths;
        std::vector<DiskInfo> result;
        for (const auto &path : paths)
        {
            if (!configuredPaths.insert(path).second)
                continue;
            if (result.size() >= SZX_METRICS_MAX_DISK_PATHS)
                break;
            DiskInfo disk{path, path, "未知类型", sampledAt, {}};
            const MountEntry *owner = nullptr;
            for (const auto &mount : mounts)
                if ((path == mount.path || mount.path == "/" || path.rfind(mount.path + "/", 0) == 0) &&
                    (!owner || mount.path.size() > owner->path.size()))
                    owner = &mount;
            if (!owner || !owner->local)
            {
                if (!configured.empty())
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
            disk.type = diskType(path);
            struct statvfs data{};
            if (::statvfs(path.c_str(), &data) == 0 && data.f_frsize &&
                data.f_blocks <= std::numeric_limits<std::uint64_t>::max() / data.f_frsize && data.f_bfree <= data.f_blocks &&
                data.f_bavail <= data.f_bfree)
                disk.capacity =
                    capacityMetric(data.f_blocks * data.f_frsize, data.f_bfree * data.f_frsize, data.f_bavail * data.f_frsize);
            result.push_back(std::move(disk));
        }
        return result;
    }
    [[nodiscard]] auto networks(std::int64_t sampledAt) -> std::vector<NetworkInfo> override
    {
        const auto counters = readNetworkCounters();
        const auto now      = std::chrono::steady_clock::now();
        const auto elapsed  = std::chrono::duration<double>(now - networkAt_).count();
        std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> current;

        std::map<std::string, std::vector<std::string>> ipv4;
        std::map<std::string, std::vector<std::string>> ipv6;
        ifaddrs *first = nullptr;
        if (::getifaddrs(&first) == 0)
        {
            for (auto *entry = first; entry != nullptr; entry = entry->ifa_next)
            {
                if (entry->ifa_addr == nullptr)
                    continue;
                std::array<char, INET6_ADDRSTRLEN> text{};
                const auto family = entry->ifa_addr->sa_family;
                if (family == AF_INET)
                {
                    const auto *address = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
                    if (::inet_ntop(AF_INET, &address->sin_addr, text.data(), text.size()))
                        ipv4[entry->ifa_name].emplace_back(text.data());
                }
                else if (family == AF_INET6)
                {
                    const auto *address = reinterpret_cast<const sockaddr_in6 *>(entry->ifa_addr);
                    if (address->sin6_scope_id)
                        continue; // 链路本地地址对面板无意义
                    if (::inet_ntop(AF_INET6, &address->sin6_addr, text.data(), text.size()))
                        ipv6[entry->ifa_name].emplace_back(text.data());
                }
            }
            ::freeifaddrs(first);
        }

        std::vector<NetworkInfo> result;
        for (const auto &[name, bytes] : counters)
        {
            if (isVirtualInterface(name))
                continue;
            NetworkInfo nic;
            nic.id          = name;
            nic.name        = name;
            nic.description = name;
            nic.ipv4        = ipv4[name];
            nic.ipv6        = ipv6[name];
            nic.sampledAt   = sampledAt;
            // /proc/net/dev 只列出存在且有地址族的接口，能出现即视为已连接
            const auto previous = previousCounters_.find(name);
            if (previous != previousCounters_.end() && elapsed > 0.0)
            {
                if (bytes.first >= previous->second.first)
                    nic.rxBytesPerSec = static_cast<double>(bytes.first - previous->second.first) / elapsed;
                if (bytes.second >= previous->second.second)
                    nic.txBytesPerSec = static_cast<double>(bytes.second - previous->second.second) / elapsed;
            }
            if (const auto speed = readNumber("/sys/class/net/" + name + "/speed"); speed && *speed > 0)
                nic.linkMbps = static_cast<double>(*speed);
            current[name] = bytes;
            result.push_back(std::move(nic));
        }
        previousCounters_ = std::move(current);
        networkAt_        = now;
        return result;
    }
    [[nodiscard]] auto processes(const std::vector<std::string> &names, std::uint32_t logicalCpus)
        -> std::vector<ProcessInfo> override
    {
        const auto now     = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double>(now - processAt_).count();
        const double hz    = longTickFrequency();
        std::map<std::string, double> deltas;
        std::map<std::string, std::uint64_t> memory;
        std::map<std::string, bool> found;
        std::map<std::string, ProcessTicks> current;

        std::error_code error;
        for (const auto &entry : fs::directory_iterator("/proc", error))
        {
            if (!entry.is_directory())
                continue;
            const auto pid = entry.path().filename().string();
            if (pid.empty() || !std::all_of(pid.begin(), pid.end(), [](char value) { return std::isdigit(value) != 0; }))
                continue;
            const auto name = processName(pid);
            if (name.empty())
                continue;
            const auto watched =
                std::find_if(names.begin(), names.end(), [&name](const std::string &want) { return processMatches(name, want); });
            if (watched == names.end())
                continue;
            const auto ticks = readProcessTicks(pid);
            if (!ticks)
                continue;
            found[*watched]   = true;
            current[pid]      = *ticks;
            memory[*watched] += ticks->residentBytes;
            if (const auto previous = previousTicks_.find(pid);
                previous != previousTicks_.end() && elapsed > 0.0 && ticks->ticks >= previous->second.ticks)
                deltas[*watched] += static_cast<double>(ticks->ticks - previous->second.ticks);
        }
        previousTicks_ = std::move(current);
        processAt_     = now;

        std::vector<ProcessInfo> result;
        for (const auto &name : names)
        {
            ProcessInfo info;
            info.name = name;
            if (!found[name])
            {
                info.state = SZX_METRICS_METRIC_STOPPED;
            }
            else
            {
                info.state       = SZX_METRICS_METRIC_OK;
                info.memoryBytes = memory[name];
                const double raw = deltas[name] / hz;
                info.cpuPercent =
                    logicalCpus ? std::clamp(100.0 * raw / elapsed / static_cast<double>(logicalCpus), 0.0, 100.0) : 0.0;
            }
            result.push_back(std::move(info));
        }
        return result;
    }
    [[nodiscard]] auto gpuCount() -> std::uint32_t override
    {
        return nvml_.deviceCount();
    }
    [[nodiscard]] auto gpuName(std::uint32_t index) -> std::string override
    {
        return nvml_.deviceName(index);
    }
    [[nodiscard]] auto gpu(std::uint32_t index) -> MetricValue override
    {
        const auto sample = nvml_.sample(index);
        MetricValue result;
        if (sample.usageValid)
        {
            result.state         = SZX_METRICS_METRIC_OK;
            result.present      |= SZX_METRICS_FIELD_USAGE;
            result.usagePercent  = sample.usagePercent;
        }
        if (sample.memoryValid)
        {
            result.state |= SZX_METRICS_METRIC_OK;
            if (result.state == 0)
                result.state = SZX_METRICS_METRIC_OK;
            result.present |=
                SZX_METRICS_FIELD_TOTAL | SZX_METRICS_FIELD_USED | SZX_METRICS_FIELD_AVAILABLE | SZX_METRICS_FIELD_FREE;
            result.totalBytes     = sample.totalBytes;
            result.usedBytes      = sample.totalBytes - sample.freeBytes;
            result.availableBytes = sample.freeBytes;
            result.freeBytes      = sample.freeBytes;
        }
        if (sample.temperatureCelsius)
        {
            result.present            |= SZX_METRICS_FIELD_TEMPERATURE;
            result.temperatureCelsius  = *sample.temperatureCelsius;
        }
        return result;
    }
    [[nodiscard]] auto gpuExtra(std::uint32_t index) -> GpuExtraInfo override
    {
        GpuExtraInfo extra;
        // Linux 没有任务管理器的 GPU Engine 计数器；NVML 只提供按进程显存
        const auto memory = nvml_.processMemory(index);
        if (memory.empty())
            return extra;
        extra.state = SZX_METRICS_METRIC_OK;
        for (const auto &[pid, bytes] : memory)
        {
            const auto name = processName(std::to_string(pid));
            if (name.empty())
                continue;
            extra.vramByProcess[name] += bytes;
        }
        return extra;
    }

private:
    std::optional<CpuCountersRaw> previousCpu_;
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> previousCounters_;
    std::chrono::steady_clock::time_point networkAt_{};
    std::map<std::string, ProcessTicks> previousTicks_;
    std::chrono::steady_clock::time_point processAt_{};
    NvmlLoader nvml_;
};
} // namespace

auto createCollector() -> std::unique_ptr<Collector>
{
    return std::make_unique<LinuxCollector>();
}
} // namespace v1
} // namespace SZX::METRICS::DETAIL
